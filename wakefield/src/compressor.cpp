#include "compressor.h"
#include <cstring>

Compressor::Compressor(float sampleRate)
    : sampleRate(sampleRate),
      thresholdDB(-20.0f),
      ratio(4.0f),
      attackCoeff(0.0f),
      releaseCoeff(0.0f),
      kneeDB(6.0f),
      mix(1.0f),
      autoMakeup(true),
      manualMakeupDB(0.0f),
      rmsMode(false),
      envelopeL(0.0f),
      envelopeR(0.0f),
      gainSmoother(1.0f),
      currentGainReductionDB(0.0f),
      makeupGainDB(0.0f),
      rmsIndex(0),
      rmsSumL(0.0f),
      rmsSumR(0.0f),
      grHistoryIndex(0),
      grHistorySamples(0)
{
    // Initialize RMS buffers
    std::memset(rmsBufferL, 0, sizeof(rmsBufferL));
    std::memset(rmsBufferR, 0, sizeof(rmsBufferR));

    // Initialize GR history
    std::memset(grHistory, 0, sizeof(grHistory));

    // Set default attack/release times
    setAttack(5.0f);    // 5ms attack
    setRelease(50.0f);  // 50ms release
}

void Compressor::setThreshold(float dB) {
    thresholdDB = std::clamp(dB, -60.0f, 0.0f);
}

void Compressor::setRatio(float r) {
    ratio = std::max(1.0f, r);
}

void Compressor::setAttack(float ms) {
    ms = std::clamp(ms, 0.1f, 100.0f);
    // Convert milliseconds to coefficient using exponential smoothing
    // Formula: coeff = exp(-1 / (time_in_seconds * sample_rate))
    float timeInSeconds = ms / 1000.0f;
    attackCoeff = std::exp(-1.0f / (timeInSeconds * sampleRate));
}

void Compressor::setRelease(float ms) {
    ms = std::clamp(ms, 10.0f, 1000.0f);
    float timeInSeconds = ms / 1000.0f;
    releaseCoeff = std::exp(-1.0f / (timeInSeconds * sampleRate));
}

void Compressor::setKnee(float dB) {
    kneeDB = std::clamp(dB, 0.0f, 20.0f);
}

void Compressor::setMix(float m) {
    mix = std::clamp(m, 0.0f, 1.0f);
}

void Compressor::setAutoMakeup(bool enable) {
    autoMakeup = enable;
}

void Compressor::setManualMakeup(float dB) {
    manualMakeupDB = std::clamp(dB, 0.0f, 24.0f);
}

void Compressor::setDetectionMode(bool rms) {
    rmsMode = rms;
}

float Compressor::computeGainReduction(float inputDB) {
    // Soft-knee compression curve
    // Below threshold: no compression
    // In knee range: smooth transition
    // Above threshold: full compression ratio

    float overDB = inputDB - thresholdDB;

    if (kneeDB > 0.0f && overDB > -kneeDB / 2.0f && overDB < kneeDB / 2.0f) {
        // Soft knee region: quadratic interpolation
        float x = overDB + kneeDB / 2.0f;
        overDB = x * x / (2.0f * kneeDB);
    } else if (overDB < -kneeDB / 2.0f) {
        // Below knee
        overDB = 0.0f;
    } else {
        // Above knee
        overDB = overDB - kneeDB / 2.0f;
    }

    // Apply ratio (gain reduction in dB)
    float gainReductionDB = -overDB * (1.0f - 1.0f / ratio);

    return gainReductionDB;
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
    if (!autoMakeup) {
        // Use manual makeup gain
        makeupGainDB = manualMakeupDB;
        return;
    }

    if (grHistorySamples < GR_HISTORY_SIZE / 4) {
        makeupGainDB = 0.0f;
        return;
    }

    // Calculate average gain reduction over recent history
    float sum = 0.0f;
    int count = std::min(grHistorySamples, GR_HISTORY_SIZE);

    for (int i = 0; i < count; ++i) {
        sum += grHistory[i];
    }

    // Makeup gain is approximately 60-70% of average GR (conservative)
    makeupGainDB = -(sum / count) * 0.65f;

    // Limit makeup gain to reasonable range
    makeupGainDB = std::clamp(makeupGainDB, 0.0f, 24.0f);
}

void Compressor::process(const float* input, float* output, int numSamples) {
    // Process stereo interleaved samples
    for (int i = 0; i < numSamples; i += 2) {
        float inputL = input[i];
        float inputR = input[i + 1];

        // Store dry signal for mix
        float dryL = inputL;
        float dryR = inputR;

        // === STEP 1: Envelope Detection ===
        float level;

        if (rmsMode) {
            // RMS detection (slower, smoother)
            // Update circular buffers
            rmsSumL -= rmsBufferL[rmsIndex];
            rmsSumR -= rmsBufferR[rmsIndex];

            float squareL = inputL * inputL;
            float squareR = inputR * inputR;

            rmsBufferL[rmsIndex] = squareL;
            rmsBufferR[rmsIndex] = squareR;

            rmsSumL += squareL;
            rmsSumR += squareR;

            rmsIndex = (rmsIndex + 1) % RMS_WINDOW_SIZE;

            float rmsL = std::sqrt(rmsSumL / RMS_WINDOW_SIZE);
            float rmsR = std::sqrt(rmsSumR / RMS_WINDOW_SIZE);

            level = std::max(rmsL, rmsR);  // Stereo-linked
        } else {
            // Peak detection (faster, more aggressive)
            float absL = std::abs(inputL);
            float absR = std::abs(inputR);

            // Smooth envelope follower
            envelopeL = absL > envelopeL ?
                absL :
                absL + attackCoeff * (envelopeL - absL);

            envelopeR = absR > envelopeR ?
                absR :
                absR + attackCoeff * (envelopeR - absR);

            level = std::max(envelopeL, envelopeR);  // Stereo-linked
        }

        // Convert to dB (with floor to prevent log(0))
        const float minLevel = 1e-6f;  // -120 dB
        level = std::max(level, minLevel);
        float levelDB = 20.0f * std::log10(level);

        // === STEP 2: Gain Computer ===
        float targetGainReductionDB = computeGainReduction(levelDB);

        // === STEP 3: Ballistics (Attack/Release Smoothing) ===
        // Current gain reduction in linear domain
        float targetGainLinear = std::pow(10.0f, targetGainReductionDB / 20.0f);

        // Smooth towards target using attack/release
        float coeff = (targetGainLinear < gainSmoother) ? attackCoeff : releaseCoeff;
        gainSmoother = targetGainLinear + coeff * (gainSmoother - targetGainLinear);

        // Convert back to dB for metering
        currentGainReductionDB = 20.0f * std::log10(std::max(gainSmoother, minLevel));

        // === STEP 4: Apply Gain Reduction + Makeup ===
        float totalGainDB = currentGainReductionDB + makeupGainDB;
        float totalGainLinear = std::pow(10.0f, totalGainDB / 20.0f);

        float wetL = inputL * totalGainLinear;
        float wetR = inputR * totalGainLinear;

        // === STEP 5: Soft Clipping ===
        wetL = softClip(wetL);
        wetR = softClip(wetR);

        // === STEP 6: Dry/Wet Mix ===
        output[i] = dryL * (1.0f - mix) + wetL * mix;
        output[i + 1] = dryR * (1.0f - mix) + wetR * mix;

        // === Update Gain Reduction History ===
        if (i % 8 == 0) {  // Update every 8 samples to reduce overhead
            grHistory[grHistoryIndex] = currentGainReductionDB;
            grHistoryIndex = (grHistoryIndex + 1) % GR_HISTORY_SIZE;
            if (grHistorySamples < GR_HISTORY_SIZE) {
                grHistorySamples++;
            }
        }
    }

    // Update makeup gain periodically (not per-sample)
    static int updateCounter = 0;
    updateCounter++;
    if (updateCounter >= 4800) {  // Every ~100ms at 48kHz (stereo)
        updateMakeupGain();
        updateCounter = 0;
    }
}
