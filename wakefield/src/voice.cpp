#include "voice.h"
#include "synth.h"  // For Synth::getOscillatorBaseLevel()
#include "ui.h"    // For SynthParameters definition
#include "voice_profiler.h"

// Global profiler instance
VoiceProfiler g_voiceProfiler;

namespace {

constexpr float kHalfRateInterpCoeffs[Voice::kHalfRateHistorySize] = {
    0.3125f, 0.9375f, -0.3125f, 0.0625f
};

inline void pushHalfRateSample(float* history, float sample) {
    for (int i = Voice::kHalfRateHistorySize - 1; i > 0; --i) {
        history[i] = history[i - 1];
    }
    history[0] = sample;
}

inline float reconstructHalfRateSample(const float* history) {
    float result = 0.0f;
    for (int i = 0; i < Voice::kHalfRateHistorySize; ++i) {
        result += history[i] * kHalfRateInterpCoeffs[i];
    }
    return result;
}

} // namespace

#ifdef ENABLE_VOICE_PROFILING
#define VOICE_PROFILE_START(var) auto var = g_voiceProfiler.now()
#define VOICE_PROFILE_END(field, var) g_voiceProfiler.field += g_voiceProfiler.elapsed_ns(var)
#else
#define VOICE_PROFILE_START(var) ((void)0)
#define VOICE_PROFILE_END(field, var) ((void)0)
#endif

void Voice::resetFMHistory() {
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        lastOscOutputs[i] = 0.0f;
        for (int h = 0; h < kHalfRateHistorySize; ++h) {
            halfRateOscHistory[i][h] = 0.0f;
        }
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        lastSamplerOutputs[i] = 0.0f;
        for (int h = 0; h < kHalfRateHistorySize; ++h) {
            halfRateSamplerHistory[i][h] = 0.0f;
        }
    }
}

void Voice::forceSilence() {
    active = false;
    freeRunningOscIndex = -1;
    freeRunningSamplerIndex = -1;
    clearGlobalFmInputs();
    for (int i = 0; i < 4; ++i) {
        envelopes[i].reset();
        envelopeValues[i] = 0.0f;
        envelopeTargets[i] = 0.0f;
        envelopeIncrements[i] = 0.0f;
    }
    cachedAnySolo = false;
    cachedActiveOscCount = OSCILLATORS_PER_VOICE;
    cachedActiveSamplerCount = SAMPLERS_PER_VOICE;
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        cachedOscSolo[i] = false;
        cachedOscMuted[i] = false;
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        cachedSamplerSolo[i] = false;
        cachedSamplerMuted[i] = false;
    }
    for (int i = 0; i < 4; ++i) {
        cachedChaosSolo[i] = false;
    }
    resetFMHistory();
    ampGateEnvelope.reset();
    ampGateValue = 0.0f;
    ampGateTarget = 0.0f;
    ampGateIncrement = 0.0f;
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        // Critical (smoothed)
        pitchMod[i] = 0.0f;
        smoothedPitchMod[i] = 0.0f;
        ampMod[i] = 0.0f;
        smoothedAmpMod[i] = 0.0f;
        cachedOscLevel[i] = 0.0f;
        smoothedOscLevel[i] = 0.0f;
        // Increments for linear interpolation
        pitchModIncrement[i] = 0.0f;
        ampModIncrement[i] = 0.0f;
        oscLevelIncrement[i] = 0.0f;
        morphMod[i] = 0.0f;
        // Non-critical (direct)
        ratioMod[i] = 0.0f;
        offsetMod[i] = 0.0f;
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        // Critical (smoothed)
        samplerPitchMod[i] = 0.0f;
        smoothedSamplerPitchMod[i] = 0.0f;
        samplerLevelMod[i] = 0.0f;
        smoothedSamplerLevelMod[i] = 0.0f;
        // Increments for linear interpolation
        samplerPitchModIncrement[i] = 0.0f;
        samplerLevelModIncrement[i] = 0.0f;
        // Non-critical (direct)
        samplerLoopStartMod[i] = 0.0f;
        samplerLoopLengthMod[i] = 0.0f;
        samplerCrossfadeMod[i] = 0.0f;
        samplerPhaseDriver[i] = -1.0f;
        samplers[i].stopPlayback();
    }
}

float Voice::generateSample(unsigned int frameIndex) {
    if (!active) {
        for (int i = 0; i < 4; ++i) {
            envelopeValues[i] = 0.0f;
            envelopeIncrements[i] = 0.0f;
        }
        ampGateValue = 0.0f;
        ampGateIncrement = 0.0f;
        return 0.0f;
    }

#ifdef ENABLE_VOICE_PROFILING
    g_voiceProfiler.totalSamples++;
#endif

    // === Half-Rate Processing: Determine even/odd sample ===
    // When enabled, oscillators/samplers only run on even samples (0, 2, 4, ...)
    // Odd samples (1, 3, 5, ...) use cached outputs from previous even sample
    const bool isEvenSample = (frameIndex & 1) == 0;

    const int restrictedOsc = freeRunningOscIndex;
    const bool restrictOscillators = (restrictedOsc >= 0);
    const int restrictedSampler = freeRunningSamplerIndex;
    const bool restrictSamplers = (restrictedSampler >= 0);

    if (frameIndex == 0) {
        cachedActiveOscCount = OSCILLATORS_PER_VOICE;
        cachedActiveSamplerCount = SAMPLERS_PER_VOICE;
        if (params) {
            cachedActiveOscCount = std::clamp(params->activeOscCount.load(), 1, OSCILLATORS_PER_VOICE);
            cachedActiveSamplerCount = std::clamp(params->activeSamplerCount.load(), 1, SAMPLERS_PER_VOICE);
        }

        if (params) {
            bool oscSoloFound = false;
            bool samplerSoloFound = false;
            bool chaosSoloFound = false;

            for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
                bool solo = params->oscSolo[i].load();
                cachedOscSolo[i] = solo;
                cachedOscMuted[i] = params->oscMuted[i].load();
                if (solo) {
                    oscSoloFound = true;
                }
            }
            for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
                bool solo = params->samplerSolo[i].load();
                cachedSamplerSolo[i] = solo;
                cachedSamplerMuted[i] = params->samplerMuted[i].load();
                if (solo) {
                    samplerSoloFound = true;
                }
            }
            for (int i = 0; i < 4; ++i) {
                bool solo = params->chaosSolo[i].load();
                cachedChaosSolo[i] = solo;
                if (solo) {
                    chaosSoloFound = true;
                }
            }

            if (oscSoloFound) {
                cachedAnySolo = true;
            } else if (samplerSoloFound) {
                cachedAnySolo = true;
            } else if (chaosSoloFound) {
                cachedAnySolo = true;
            } else {
                cachedAnySolo = false;
            }
        } else {
            cachedAnySolo = false;
            for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
                cachedOscSolo[i] = false;
                cachedOscMuted[i] = false;
            }
            for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
                cachedSamplerSolo[i] = false;
                cachedSamplerMuted[i] = false;
            }
            for (int i = 0; i < 4; ++i) {
                cachedChaosSolo[i] = false;
            }
        }

        unsigned int blockSize = std::max(1u, currentBufferSize);
        for (int i = 0; i < 4; ++i) {
            float newTarget = envelopes[i].processBlock(blockSize);
            // Calculate per-sample increment for linear interpolation
            envelopeIncrements[i] = (newTarget - envelopeValues[i]) / static_cast<float>(blockSize);
            envelopeTargets[i] = newTarget;
        }

        float newGateTarget = ampGateEnvelope.processBlock(blockSize);
        ampGateIncrement = (newGateTarget - ampGateValue) / static_cast<float>(blockSize);
    }

    float ampEnvelope = std::clamp(ampGateValue, 0.0f, 1.0f);

    if (ampGateTarget <= 0.0f && ampGateValue < kAmpGateSilenceThreshold) {
        if (synth) {
            for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
                synth->saveSamplerPhase(i, samplers[i].getCurrentPhase());
            }
        }
        active = false;
        for (int i = 0; i < 4; ++i) {
            envelopeValues[i] = 0.0f;
            envelopeTargets[i] = 0.0f;
            envelopeIncrements[i] = 0.0f;
        }
        resetFMHistory();
        ampGateEnvelope.reset();
        ampGateValue = 0.0f;
        ampGateTarget = 0.0f;
        ampGateIncrement = 0.0f;
        return 0.0f;
    }

    // === PROFILING: Oscillator FM ===
#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_START(t_osc_fm);
#endif

    float fmInputs[OSCILLATORS_PER_VOICE] = {0.0f};
    const float* oscFmForFrame = globalFmInputs
        ? globalFmInputs + static_cast<size_t>(frameIndex) * OSCILLATORS_PER_VOICE
        : nullptr;
    if (oscFmForFrame) {
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            fmInputs[i] = oscFmForFrame[i];
        }
    }

#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_END(timeOscFM, t_osc_fm);
#endif

    // === PROFILING: Oscillator Processing ===
#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_START(t_osc_proc);
#endif

    float currentOutputs[OSCILLATORS_PER_VOICE]{};

    // Half-rate processing: Only run oscillators on even samples (0, 2, 4, ...)
    const bool processOscillators = !halfRateEnabled || isEvenSample;
    const float oscSampleRate = halfRateEnabled ? (sampleRate * 0.5f) : sampleRate;

    if (!restrictSamplers) {
        if (processOscillators) {
            for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
                bool shouldProcess = true;
                if (i >= cachedActiveOscCount) {
                    shouldProcess = false;
                } else if (oscillators[i].getMode() == BrainwaveMode::FREE && freeRunningOscIndex < 0) {
                    shouldProcess = false;
                } else if (oscillators[i].getMode() == BrainwaveMode::KEY) {
                    if (freeRunningOscIndex >= 0) {
                        shouldProcess = false;
                    } else if (params) {
                        int oscNoteSource = params->getOscNoteSource(i);
                        if (oscNoteSource != noteSource) {
                            shouldProcess = false;
                        }
                    }
                }

                float oscSample = 0.0f;
                if (shouldProcess) {
                    oscSample = oscillators[i].process(oscSampleRate,
                                                        fmInputs[i],
                                                        smoothedPitchMod[i],
                                                        morphMod[i],
                                                        ratioMod[i],
                                                        offsetMod[i]);
                    if (!std::isfinite(oscSample)) {
                        oscSample = 0.0f;
                    }
                }

                currentOutputs[i] = oscSample;
                pushHalfRateSample(halfRateOscHistory[i], oscSample);
            }
        } else if (halfRateEnabled) {
            for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
                currentOutputs[i] = reconstructHalfRateSample(halfRateOscHistory[i]);
            }
        }
    } else {
        // Oscillators are fully restricted (e.g., sampler free-run mode)
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            pushHalfRateSample(halfRateOscHistory[i], 0.0f);
            currentOutputs[i] = 0.0f;
        }
    }

#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_END(timeOscProcess, t_osc_proc);
#endif

    // === PROFILING: Oscillator Gain Calculation ===
#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_START(t_osc_gain);
#endif

    float mixedSample = 0.0f;
    float oscFinalGains[OSCILLATORS_PER_VOICE]{};
    if (!restrictSamplers) {
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            if (i >= cachedActiveOscCount) {
                continue;
            }

            float baseAmp = synth ? synth->getOscillatorBaseAmp(i) : 1.0f;
            float baseLevel = smoothedOscLevel[i];
            float modulatedAmp = std::clamp(baseAmp + smoothedAmpMod[i], 0.0f, 1.0f);
            float finalGain = modulatedAmp * baseLevel;

            if (cachedAnySolo) {
                if (!cachedOscSolo[i]) {
                    finalGain = 0.0f;
                }
            } else if (cachedOscMuted[i]) {
                finalGain = 0.0f;
            }

            finalGain *= ampEnvelope;
            oscFinalGains[i] = finalGain;
        }
    }

#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_END(timeOscGainCalc, t_osc_gain);
#endif

    // === PROFILING: Sampler FM ===
#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_START(t_samp_fm);
#endif

    float samplerFMInputs[SAMPLERS_PER_VOICE] = {0.0f};
    const float* samplerFmForFrame = globalSamplerFmInputs
        ? globalSamplerFmInputs + static_cast<size_t>(frameIndex) * SAMPLERS_PER_VOICE
        : nullptr;
    if (samplerFmForFrame) {
        for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
            samplerFMInputs[i] = samplerFmForFrame[i];
        }
    }

#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_END(timeSamplerFM, t_samp_fm);
#endif

    // === PROFILING: Sampler Processing ===
#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_START(t_samp_proc);
#endif

    float currentSamplerOutputs[SAMPLERS_PER_VOICE] = {0.0f};
    float samplerFinalOutputs[SAMPLERS_PER_VOICE] = {0.0f};

    // Half-rate processing: Only run samplers on even samples (0, 2, 4, ...)
    const bool processSamplers = !halfRateEnabled || isEvenSample;
    const float samplerSampleRate = halfRateEnabled ? (sampleRate * 0.5f) : sampleRate;

    if (processSamplers) {
        for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
            float samplerOut = 0.0f;
            bool shouldProcess = true;

            if (i >= cachedActiveSamplerCount) {
                shouldProcess = false;
            } else if (restrictOscillators) {
                shouldProcess = false;
            } else {
                bool samplerKeyMode = samplers[i].isKeyMode();
                if (samplerKeyMode) {
                    if (!restrictSamplers) {
                        if (!params) {
                            shouldProcess = true;
                        } else {
                            int samplerNoteSource = params->getSamplerNoteSource(i);
                            shouldProcess = (samplerNoteSource == noteSource);
                        }
                    } else {
                        shouldProcess = false;
                    }
                } else {
                    shouldProcess = (restrictSamplers && restrictedSampler == i);
                }
            }

            if (shouldProcess) {
                samplerOut = samplers[i].process(samplerSampleRate,
                                                 samplerFMInputs[i],
                                                 smoothedSamplerPitchMod[i],
                                                 samplerLoopStartMod[i],
                                                 samplerLoopLengthMod[i],
                                                 samplerCrossfadeMod[i],
                                                 smoothedSamplerLevelMod[i],
                                                 cachedSamplerLevelMod[i],
                                                 samplerPhaseDriver[i],
                                                 note,
                                                 synth ? synth->currentTempo : 120.0f,
                                                 synth ? synth->getSamplerSyncMode(i) : 0);
                if (!std::isfinite(samplerOut)) {
                    samplerOut = 0.0f;
                }
            }

            currentSamplerOutputs[i] = samplerOut;
            pushHalfRateSample(halfRateSamplerHistory[i], samplerOut);

            float samplerOutPost = samplerOut;
            if (cachedAnySolo) {
                if (!cachedSamplerSolo[i]) {
                    samplerOutPost = 0.0f;
                }
            } else if (cachedSamplerMuted[i]) {
                samplerOutPost = 0.0f;
            }

            samplerFinalOutputs[i] = samplerOutPost * ampEnvelope;
        }
    } else if (halfRateEnabled) {
        for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
            float interpolated = reconstructHalfRateSample(halfRateSamplerHistory[i]);
            currentSamplerOutputs[i] = interpolated;

            float samplerOut = interpolated;
            if (cachedAnySolo) {
                if (!cachedSamplerSolo[i]) {
                    samplerOut = 0.0f;
                }
            } else if (cachedSamplerMuted[i]) {
                samplerOut = 0.0f;
            }

            samplerFinalOutputs[i] = samplerOut * ampEnvelope;
        }
    }

#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_END(timeSamplerProcess, t_samp_proc);
#endif

    // === PROFILING: Mixing ===
#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_START(t_mix);
#endif

    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        if (i >= cachedActiveOscCount) continue;
        mixedSample += currentOutputs[i] * oscFinalGains[i];
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        if (i >= cachedActiveSamplerCount) continue;
        mixedSample += samplerFinalOutputs[i];
    }

    if (!std::isfinite(mixedSample)) {
        mixedSample = 0.0f;
    }

#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_END(timeMixing, t_mix);
#endif

    // === PROFILING: History Update ===
#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_START(t_history);
#endif

    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        if (i >= cachedActiveOscCount) {
            lastOscOutputs[i] = 0.0f;
            continue;
        }
        lastOscOutputs[i] = currentOutputs[i];
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        if (i >= cachedActiveSamplerCount) {
            lastSamplerOutputs[i] = 0.0f;
            continue;
        }
        lastSamplerOutputs[i] = currentSamplerOutputs[i];
    }

#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_END(timeHistoryUpdate, t_history);
#endif

#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_START(t_smoothing);
#endif

    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        smoothedPitchMod[i] += pitchModIncrement[i];
        smoothedAmpMod[i] += ampModIncrement[i];
        smoothedOscLevel[i] += oscLevelIncrement[i];
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        smoothedSamplerPitchMod[i] += samplerPitchModIncrement[i];
        smoothedSamplerLevelMod[i] += samplerLevelModIncrement[i];
    }
    for (int i = 0; i < 4; ++i) {
        envelopeValues[i] += envelopeIncrements[i];
    }
    ampGateValue = std::clamp(ampGateValue + ampGateIncrement, 0.0f, 1.0f);

#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_END(timeSmoothing, t_smoothing);
#endif

    return mixedSample;
}
