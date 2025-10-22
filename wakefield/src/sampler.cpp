#include "sampler.h"
#include "sample_bank.h"
#include <algorithm>
#include <cstring>
#include <limits>

// Minimum loop length in samples (to prevent glitches)
static constexpr uint32_t MIN_LOOP_LENGTH = 2048;

Sampler::Sampler()
    : currentSample(nullptr)
    , loopStartNorm(0.0f)
    , loopLengthNorm(1.0f)
    , crossfadeLengthNorm(0.1f)
    , playbackSpeed(1.0f)
    , tzfmDepth(0.0f)
    , level(1.0f)
    , mode(PlaybackMode::FORWARD)
    , primaryVoice(&voiceA)
    , secondaryVoice(&voiceB)
    , crossfading(false)
    , crossfadeSamplesTotal(0)
    , crossfadeSamplesRemaining(0)
    , pendingStart(0)
    , pendingEnd(0)
    , wasInZoneLastSample(false)
    , loopBoundariesCalculated(false)
    , modulatorSmoothed(0.0f) {

    // Initialize voice A as active
    voiceA.phase_q32_32 = 0;
    voiceA.loop_start = 0;
    voiceA.loop_end = 0;
    voiceA.amplitude = 1.0f;
    voiceA.active = true;

    // Initialize voice B as inactive
    voiceB.phase_q32_32 = 0;
    voiceB.loop_start = 0;
    voiceB.loop_end = 0;
    voiceB.amplitude = 0.0f;
    voiceB.active = false;
}

void Sampler::setSample(const SampleData* sample) {
    currentSample = sample;
    loopBoundariesCalculated = false;
    reset();
}

void Sampler::setLoopStart(float normalized) {
    loopStartNorm = std::clamp(normalized, 0.0f, 1.0f);
}

void Sampler::setLoopLength(float normalized) {
    loopLengthNorm = std::clamp(normalized, 0.0f, 1.0f);
}

void Sampler::setCrossfadeLength(float normalized) {
    crossfadeLengthNorm = std::clamp(normalized, 0.0f, 1.0f);
}

void Sampler::setPlaybackSpeed(float speed) {
    playbackSpeed = speed;
}

void Sampler::setTZFMDepth(float depth) {
    tzfmDepth = std::clamp(depth, 0.0f, 1.0f);
}

void Sampler::setPlaybackMode(PlaybackMode newMode) {
    mode = newMode;
}

void Sampler::setLevel(float newLevel) {
    level = std::clamp(newLevel, 0.0f, 1.0f);
}

float Sampler::getPlayheadPosition() const {
    if (!currentSample || currentSample->sampleCount == 0) {
        return 0.0f;
    }
    uint32_t idx = static_cast<uint32_t>(primaryVoice->phase_q32_32 >> 32);
    return static_cast<float>(idx) / static_cast<float>(currentSample->sampleCount);
}

void Sampler::reset() {
    if (currentSample && currentSample->sampleCount > 0) {
        calculateLoopBoundaries(0.0f, 0.0f);
        primaryVoice->phase_q32_32 = static_cast<uint64_t>(pendingStart) << 32;
        primaryVoice->loop_start = pendingStart;
        primaryVoice->loop_end = pendingEnd;
        loopBoundariesCalculated = true;
    } else {
        primaryVoice->phase_q32_32 = 0;
        primaryVoice->loop_start = 0;
        primaryVoice->loop_end = 0;
    }
    crossfading = false;
    wasInZoneLastSample = false;
}

const char* Sampler::getSampleName() const {
    if (currentSample) {
        return currentSample->name.c_str();
    }
    return "No Sample";
}

void Sampler::calculateLoopBoundaries(float startMod, float lengthMod) {
    if (!currentSample || currentSample->sampleCount < MIN_LOOP_LENGTH) {
        pendingStart = 0;
        pendingEnd = 0;
        return;
    }

    const uint32_t totalSamples = currentSample->sampleCount;
    const uint32_t availableSpan = totalSamples > MIN_LOOP_LENGTH ?
                                   totalSamples - MIN_LOOP_LENGTH : 0;

    // Apply modulation to loop start (clamped to 0-1)
    float modulatedStart = std::clamp(loopStartNorm + startMod, 0.0f, 1.0f);
    pendingStart = availableSpan > 0 ?
                   static_cast<uint32_t>(modulatedStart * availableSpan) : 0;

    // Apply modulation to loop length (clamped to 0-1)
    float modulatedLength = std::clamp(loopLengthNorm + lengthMod, 0.0f, 1.0f);
    uint32_t loopLen = MIN_LOOP_LENGTH +
                      (availableSpan > 0 ?
                       static_cast<uint32_t>(modulatedLength * availableSpan) : 0);

    pendingEnd = pendingStart + loopLen;
    if (pendingEnd > totalSamples) {
        pendingEnd = totalSamples;
    }
}

void Sampler::wrapPhase(SamplerVoice* voice) const {
    const int64_t start_q = static_cast<int64_t>(voice->loop_start) << 32;
    const int64_t end_q = static_cast<int64_t>(voice->loop_end) << 32;
    const int64_t span_q = end_q - start_q;

    if (span_q <= 0) return;

    int64_t phase = static_cast<int64_t>(voice->phase_q32_32);
    int64_t normalized = phase - start_q;

    // Modulo wrapping for both directions
    if (normalized >= span_q) {
        normalized = normalized % span_q;
    } else if (normalized < 0) {
        normalized = span_q - ((-normalized) % span_q);
        if (normalized == span_q) normalized = 0;
    }

    voice->phase_q32_32 = static_cast<uint64_t>(start_q + normalized);
}

int16_t Sampler::getSample(const SamplerVoice* voice, bool isReverse) const {
    if (!voice->active || voice->amplitude <= 0.0f || !currentSample) {
        return 0;
    }
    if (voice->loop_end <= voice->loop_start) {
        return 0;
    }

    // Extract integer sample index from Q32.32 phase
    uint32_t i = static_cast<uint32_t>(voice->phase_q32_32 >> 32);

    // Bounds check with graceful handling during crossfade
    if (crossfading && voice == primaryVoice) {
        if (i >= currentSample->sampleCount) {
            if (voice->amplitude > 0.1f) {
                i = currentSample->sampleCount - 1;
            } else {
                i %= currentSample->sampleCount;
            }
        }
    } else {
        if (i >= currentSample->sampleCount) {
            return 0;
        }
    }

    // Additional fade near buffer end during crossfade
    float additionalFade = 1.0f;
    if (crossfading && voice == primaryVoice &&
        i >= currentSample->sampleCount - 8) {
        uint32_t distanceFromEnd = currentSample->sampleCount - 1 - i;
        additionalFade = static_cast<float>(distanceFromEnd) / 7.0f;
        additionalFade = std::clamp(additionalFade, 0.0f, 1.0f);
    }

    // Get second sample for interpolation (handle loop boundaries)
    uint32_t i2;
    if (isReverse) {
        i2 = (i > voice->loop_start) ? (i - 1) : (voice->loop_end - 1);
    } else {
        i2 = (i < voice->loop_end - 1) ? (i + 1) : voice->loop_start;
    }

    // Extract 8-bit fractional part for interpolation
    const uint32_t frac32 = static_cast<uint32_t>(voice->phase_q32_32 & 0xFFFFFFFFull);
    const uint8_t mu8 = static_cast<uint8_t>(frac32 >> 24);

    // Perform interpolation
    int16_t sample = interpolate(currentSample->samples[i],
                                currentSample->samples[i2],
                                mu8);

    // Apply additional fade if needed
    sample = static_cast<int16_t>(sample * additionalFade);

    return sample;
}

int64_t Sampler::calculateIncrement(float sampleRate, float fmInput,
                                    float pitchMod, bool isReverse, int midiNote) {
    if (!currentSample) {
        return 0;
    }

    // Calculate base increment from sample rate ratio and playback speed
    // Q32.32 format: (source_rate / output_rate) * playbackSpeed
    double baseRatio = (static_cast<double>(currentSample->sampleRate) /
                       static_cast<double>(sampleRate)) * playbackSpeed;

    // KEY mode: apply exponential pitch tracking based on MIDI note
    // C4 (note 60) = 1.0, each semitone = 2^(1/12)
    if (mode == PlaybackMode::FORWARD) {
        double pitchScale = std::pow(2.0, (midiNote - 60) / 12.0);
        baseRatio *= pitchScale;
    }

    // Apply pitch modulation (in octaves)
    baseRatio *= std::pow(2.0, pitchMod);

    int64_t baseInc = static_cast<int64_t>(baseRatio * (1ULL << 32));

    // Apply direction
    if (isReverse) {
        baseInc = -baseInc;
    }

    // Apply TZFM if enabled
    if (tzfmDepth > 0.001f) {
        float rawMod = std::clamp(fmInput, -1.0f, 1.0f);

        // One-pole smoothing to prevent clicks
        modulatorSmoothed = MODULATOR_SMOOTHING * modulatorSmoothed +
                          (1.0f - MODULATOR_SMOOTHING) * rawMod;

        // Apply modulation: allows through-zero
        float modulated = static_cast<float>(baseInc) *
                         (1.0f + modulatorSmoothed * tzfmDepth);
        baseInc = static_cast<int64_t>(modulated);
    }

    // Safety limits
    const int64_t MAX_INC = 1LL << 37;
    const int64_t MIN_INC = -(1LL << 37);
    baseInc = std::clamp(baseInc, MIN_INC, MAX_INC);

    return baseInc;
}

bool Sampler::isInCrossfadeZone(uint64_t phase, uint32_t loopStart,
                               uint32_t loopEnd, uint32_t xfadeLen,
                               bool isReverse) const {
    uint32_t idx = static_cast<uint32_t>(phase >> 32);

    if (isReverse) {
        // Reverse: trigger zone at beginning of loop
        uint32_t zoneEnd = (loopStart + xfadeLen < loopEnd) ?
                          (loopStart + xfadeLen) : loopEnd;
        return (idx >= loopStart && idx < zoneEnd);
    } else {
        // Forward: trigger zone at end of loop
        uint32_t zoneStart = (loopEnd > xfadeLen) ?
                           (loopEnd - xfadeLen) : loopStart;
        return (idx >= zoneStart && idx < loopEnd);
    }
}

void Sampler::setupCrossfade(uint32_t xfadeLen, uint32_t xfadeSamples,
                            bool isReverse) {
    // Secondary voice gets new loop boundaries
    secondaryVoice->loop_start = pendingStart;
    secondaryVoice->loop_end = pendingEnd;
    secondaryVoice->active = true;

    // Position secondary voice at start of new loop
    if (isReverse) {
        secondaryVoice->phase_q32_32 =
            static_cast<uint64_t>(pendingEnd - 1) << 32;
    } else {
        secondaryVoice->phase_q32_32 =
            static_cast<uint64_t>(pendingStart) << 32;
    }

    // Start crossfade
    crossfading = true;
    crossfadeSamplesTotal = xfadeSamples;
    crossfadeSamplesRemaining = xfadeSamples;
}

float Sampler::process(float sampleRate, float fmInput, float pitchMod,
                      float loopStartMod, float loopLengthMod,
                      float crossfadeMod, float levelMod, int midiNote) {
    // Early exit if no sample loaded
    if (!currentSample || !currentSample->samples ||
        currentSample->sampleCount < 2) {
        return 0.0f;
    }

    // Calculate loop boundaries if needed
    if (!loopBoundariesCalculated) {
        calculateLoopBoundaries(loopStartMod, loopLengthMod);
        loopBoundariesCalculated = true;

        // Initialize primary voice if cold start
        if (primaryVoice->loop_end == 0) {
            primaryVoice->loop_start = pendingStart;
            primaryVoice->loop_end = pendingEnd;
            primaryVoice->phase_q32_32 = static_cast<uint64_t>(pendingStart) << 32;
        }
    }

    // Calculate crossfade length (in source samples)
    uint32_t xfadeLen = 0;
    if (primaryVoice->loop_end > primaryVoice->loop_start) {
        uint32_t loopLen = primaryVoice->loop_end - primaryVoice->loop_start;
        uint32_t maxXfade = loopLen / 2;

        // Apply crossfade modulation
        float modulatedXfade = std::clamp(crossfadeLengthNorm + crossfadeMod,
                                         0.0f, 1.0f);
        xfadeLen = static_cast<uint32_t>(maxXfade * modulatedXfade);
        xfadeLen = std::clamp(xfadeLen, 8u, maxXfade);
    }

    // Determine playback direction
    bool isReverse = (mode == PlaybackMode::REVERSE);

    // Calculate phase increment
    int64_t inc = calculateIncrement(sampleRate, fmInput, pitchMod, isReverse, midiNote);

    // Convert crossfade length to output samples using actual increment
    uint32_t xfadeSamples = 16u;
    if (xfadeLen > 0) {
        uint64_t incMagnitude = static_cast<uint64_t>(inc >= 0 ? inc : -inc);
        if (incMagnitude == 0) {
            incMagnitude = 1;
        }
        uint64_t totalDistance = static_cast<uint64_t>(xfadeLen) << 32;
        uint64_t framesNeeded = (totalDistance + incMagnitude - 1) / incMagnitude;  // ceil division
        framesNeeded = std::max<uint64_t>(16ull, framesNeeded);
        if (framesNeeded > std::numeric_limits<uint32_t>::max()) {
            framesNeeded = std::numeric_limits<uint32_t>::max();
        }
        xfadeSamples = static_cast<uint32_t>(framesNeeded);
    }

    // Check for crossfade trigger before wrapping
    if (!crossfading && xfadeLen > 0) {
        bool inZone = isInCrossfadeZone(primaryVoice->phase_q32_32,
                                       primaryVoice->loop_start,
                                       primaryVoice->loop_end,
                                       xfadeLen, isReverse);

        if (inZone && !wasInZoneLastSample) {
            calculateLoopBoundaries(loopStartMod, loopLengthMod);
            setupCrossfade(xfadeLen, xfadeSamples, isReverse);
        }
        wasInZoneLastSample = inZone;
    }

    // Advance primary voice
    primaryVoice->phase_q32_32 += inc;

    // Only wrap when not crossfading
    if (!crossfading) {
        wrapPhase(primaryVoice);
    }

    // Handle crossfading
    if (crossfading) {
        // Advance secondary voice
        secondaryVoice->phase_q32_32 += inc;
        wrapPhase(secondaryVoice);

        // Calculate constant-power crossfade amplitudes
        float t = static_cast<float>(crossfadeSamplesTotal -
                                    crossfadeSamplesRemaining) /
                 static_cast<float>(crossfadeSamplesTotal);
        primaryVoice->amplitude = std::cos(M_PI_2 * t);
        secondaryVoice->amplitude = std::sin(M_PI_2 * t);

        if (--crossfadeSamplesRemaining == 0) {
            // Crossfade complete - swap voices
            crossfading = false;
            SamplerVoice* temp = primaryVoice;
            primaryVoice = secondaryVoice;
            secondaryVoice = temp;
            secondaryVoice->active = false;
            secondaryVoice->amplitude = 0.0f;
            primaryVoice->amplitude = 1.0f;
            loopBoundariesCalculated = false;
        }
    }

    // Mix both voices
    int32_t mixedSample = 0;
    bool isRevNow = (inc < 0);

    if (primaryVoice->active && primaryVoice->amplitude > 0.0f) {
        int16_t s = getSample(primaryVoice, isRevNow);
        mixedSample += static_cast<int32_t>(s * primaryVoice->amplitude);
    }

    if (secondaryVoice->active && secondaryVoice->amplitude > 0.0f) {
        int16_t s = getSample(secondaryVoice, isRevNow);
        mixedSample += static_cast<int32_t>(s * secondaryVoice->amplitude);
    }

    // Clamp to prevent overflow
    mixedSample = std::clamp(mixedSample, -32768, 32767);

    // Convert Q15 to float and apply level modulation
    float output = static_cast<float>(mixedSample) / 32768.0f;
    float modulatedLevel = std::clamp(level + levelMod, 0.0f, 1.0f);

    return output * modulatedLevel;
}

// Linear interpolation between two int16_t values using 8-bit fractional weight
int16_t Sampler::interpolate(int16_t x, int16_t y, uint8_t mu) {
    // mu is 0-255, where 0 = x, 255 = y
    int32_t result = x + (((static_cast<int32_t>(y) - x) * mu) >> 8);
    return static_cast<int16_t>(result);
}
