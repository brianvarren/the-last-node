#include "compressor.h"
#include <cstring>

Compressor::Compressor(float sampleRate)
    : sampleRate(sampleRate),
      thresholdDB(-20.0f),
      ratio(4.0f),
      kneeDB(6.0f),
      mix(1.0f),
      autoMakeup(true),
      manualMakeupDB(0.0f),
      rmsMode(false),
      detAttackCoeff(0.0f),
      detReleaseCoeff(0.0f),
      gainAttackCoeff(0.0f),
      gainReleaseCoeff(0.0f),
      envelopeL(0.0f),
      envelopeR(0.0f),
      rmsEnvL(0.0f),
      rmsEnvR(0.0f),
      rmsCoeff(0.0f),
      gainSmoother(1.0f),
      currentGainReductionDB(0.0f),
      makeupGainDB(0.0f),
      grHistoryIndex(0),
      grHistorySamples(0),
      makeupSampleCounter(0)
{
    // Initialize GR history
    std::memset(grHistory, 0, sizeof(grHistory));

    // Set default attack/release times
    setAttack(5.0f);    // 5ms attack
    setRelease(50.0f);  // 50ms release

    // Set RMS averaging window (~20ms)
    float rmsMs = 20.0f;
    float rmsSec = rmsMs / 1000.0f;
    rmsCoeff = std::exp(-1.0f / (rmsSec * sampleRate));
}

void Compressor::setThreshold(float dB) {
    thresholdDB.store(std::clamp(dB, -60.0f, 0.0f));
}

void Compressor::setRatio(float r) {
    ratio.store(std::max(1.0f, r));
}

void Compressor::setAttack(float ms) {
    ms = std::clamp(ms, 0.1f, 100.0f);
    // Convert milliseconds to coefficient using exponential smoothing
    // Formula: coeff = exp(-1 / (time_in_seconds * sample_rate))
    float timeInSeconds = ms / 1000.0f;
    float coeff = std::exp(-1.0f / (timeInSeconds * sampleRate));
    detAttackCoeff.store(coeff);
    gainAttackCoeff.store(coeff);
}

void Compressor::setRelease(float ms) {
    ms = std::clamp(ms, 10.0f, 1000.0f);
    float timeInSeconds = ms / 1000.0f;
    float coeff = std::exp(-1.0f / (timeInSeconds * sampleRate));
    detReleaseCoeff.store(coeff);
    gainReleaseCoeff.store(coeff);
}

void Compressor::setKnee(float dB) {
    kneeDB.store(std::clamp(dB, 0.0f, 20.0f));
}

void Compressor::setMix(float m) {
    mix.store(std::clamp(m, 0.0f, 1.0f));
}

void Compressor::setAutoMakeup(bool enable) {
    autoMakeup.store(enable);
}

void Compressor::setManualMakeup(float dB) {
    manualMakeupDB.store(std::clamp(dB, 0.0f, 24.0f));
}

void Compressor::setDetectionMode(bool rms) {
    rmsMode.store(rms);
}

float Compressor::computeGainReduction(float inputDB, float threshold, float ratioVal, float knee) {
    // Standard soft-knee compressor curve
    // y = x below (T - K/2)
    // y = T + (x - T)/R above (T + K/2)
    // in between: quadratic blend
    float x = inputDB;
    float T = threshold;
    float K = std::max(0.0f, knee);

    float y;
    if (K <= 0.0f) {
        // Hard knee
        if (x <= T) {
            y = x;
        } else {
            y = T + (x - T) / ratioVal;
        }
    } else {
        float lo = T - K * 0.5f;
        float hi = T + K * 0.5f;
        if (x <= lo) {
            y = x;
        } else if (x >= hi) {
            y = T + (x - T) / ratioVal;
        } else {
            // In the knee region
            // x' measured from knee start
            float xd = x - lo;
            // Quadratic soft knee per V. Zavalishin / common implementations
            // y = x + (1/R - 1) * (xd^2) / (2K)
            y = x + (1.0f / ratioVal - 1.0f) * (xd * xd) / (2.0f * K);
        }
    }
    // Gain reduction (negative dB): GR = y - x
    return y - x;
}

float Compressor::softClip(float x) {
    // Tasteful soft clipping using tanh
    // Gentle saturation that prevents hard clipping
    const float ceiling = 0.95f;

    if (std::abs(x) < ceiling) {
        return x;
    }

    // Apply soft saturation above ceiling
    float sign = (x > 0.0f) ? 1.0f : -1.0f;
    float absX = std::abs(x);

    // Smooth transition into saturation
    float over = (absX - ceiling) / (1.0f - ceiling);
    float saturated = ceiling + (1.0f - ceiling) * std::tanh(over);

    return sign * saturated;
}

void Compressor::updateMakeupGain() {
    if (!autoMakeup.load()) {
        // Use manual makeup gain
        makeupGainDB.store(manualMakeupDB.load());
        return;
    }

    if (grHistorySamples < GR_HISTORY_SIZE / 4) {
        makeupGainDB.store(0.0f);
        return;
    }

    // Calculate average gain reduction over recent history
    float sum = 0.0f;
    int count = std::min(grHistorySamples, GR_HISTORY_SIZE);

    for (int i = 0; i < count; ++i) {
        sum += grHistory[i];
    }

    // Makeup gain is approximately 60-70% of average GR (conservative)
    float mk = -(sum / count) * 0.65f;

    // Limit makeup gain to reasonable range
    mk = std::clamp(mk, 0.0f, 24.0f);
    makeupGainDB.store(mk);
}

void Compressor::process(const float* input, float* output, int numSamples) {
    // Snapshot parameters for this call
    const float th = thresholdDB.load();
    const float ra = std::max(1.0f, ratio.load());
    const float kn = kneeDB.load();
    const float mixVal = std::clamp(mix.load(), 0.0f, 1.0f);
    const bool useRms = rmsMode.load();
    const float detAtk = detAttackCoeff.load();
    const float detRel = detReleaseCoeff.load();
    const float gnAtk  = gainAttackCoeff.load();
    const float gnRel  = gainReleaseCoeff.load();

    // Process stereo interleaved samples
    for (int i = 0; i < numSamples; i += 2) {
        float inputL = input[i];
        float inputR = input[i + 1];

        // Store dry signal for mix
        float dryL = inputL;
        float dryR = inputR;

        // === STEP 1: Envelope Detection ===
        float level;

        if (useRms) {
            // Exponential RMS detection (~20ms window)
            float sL = inputL * inputL;
            float sR = inputR * inputR;
            rmsEnvL = sL + rmsCoeff * (rmsEnvL - sL);
            rmsEnvR = sR + rmsCoeff * (rmsEnvR - sR);
            float rmsL = std::sqrt(std::max(0.0f, rmsEnvL));
            float rmsR = std::sqrt(std::max(0.0f, rmsEnvR));
            level = std::max(rmsL, rmsR);
        } else {
            // Peak detection with attack/release ballistics
            float absL = std::abs(inputL);
            float absR = std::abs(inputR);

            float cL = (absL > envelopeL) ? detAtk : detRel;
            float cR = (absR > envelopeR) ? detAtk : detRel;
            envelopeL = absL + cL * (envelopeL - absL);
            envelopeR = absR + cR * (envelopeR - absR);
            level = std::max(envelopeL, envelopeR);
        }

        // Convert to dB (with floor to prevent log(0))
        const float minLevel = 1e-6f;  // -120 dB
        level = std::max(level, minLevel);
        float levelDB = 20.0f * std::log10(level);

        // === STEP 2: Gain Computer ===
        float targetGainReductionDB = computeGainReduction(levelDB, th, ra, kn);

        // === STEP 3: Ballistics (Attack/Release Smoothing) ===
        // Current gain reduction in linear domain
        float targetGainLinear = std::pow(10.0f, targetGainReductionDB / 20.0f);

        // Smooth towards target using attack/release
        float coeff = (targetGainLinear < gainSmoother) ? gnAtk : gnRel;
        gainSmoother = targetGainLinear + coeff * (gainSmoother - targetGainLinear);

        // Convert back to dB for metering
        currentGainReductionDB = 20.0f * std::log10(std::max(gainSmoother, minLevel));

        // === STEP 4: Apply Gain Reduction + Makeup ===
        float totalGainDB = currentGainReductionDB + makeupGainDB.load();
        float totalGainLinear = std::pow(10.0f, totalGainDB / 20.0f);

        float wetL = inputL * totalGainLinear;
        float wetR = inputR * totalGainLinear;

        // === STEP 5: Soft Clipping ===
        wetL = softClip(wetL);
        wetR = softClip(wetR);

        // === STEP 6: Dry/Wet Mix ===
        output[i] = dryL * (1.0f - mixVal) + wetL * mixVal;
        output[i + 1] = dryR * (1.0f - mixVal) + wetR * mixVal;

        // === Update Gain Reduction History ===
        if (i % 8 == 0) {  // Update every 8 samples to reduce overhead
            grHistory[grHistoryIndex] = currentGainReductionDB;
            grHistoryIndex = (grHistoryIndex + 1) % GR_HISTORY_SIZE;
            if (grHistorySamples < GR_HISTORY_SIZE) {
                grHistorySamples++;
            }
        }
    }

    // Update makeup gain periodically (~100ms)
    makeupSampleCounter += numSamples; // numSamples is interleaved samples
    int samplesPerUpdate = static_cast<int>(sampleRate * 0.1f * 2.0f); // stereo
    if (makeupSampleCounter >= samplesPerUpdate) {
        updateMakeupGain();
        makeupSampleCounter -= samplesPerUpdate;
        if (makeupSampleCounter < 0) makeupSampleCounter = 0;
    }
}
