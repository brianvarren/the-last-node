#include "voice.h"
#include "synth.h"  // For Synth::getOscillatorBaseLevel()
#include "ui.h"    // For SynthParameters definition
#include "voice_profiler.h"

// Global profiler instance
VoiceProfiler g_voiceProfiler;

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
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        lastSamplerOutputs[i] = 0.0f;
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
    resetAmpControllers();
    ampGateValue = 0.0f;
    ampGateTarget = 0.0f;
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        // Critical (smoothed)
        pitchMod[i] = 0.0f;
        smoothedPitchMod[i] = 0.0f;
        ampMod[i] = 0.0f;
        smoothedAmpMod[i] = 0.0f;
        cachedOscLevel[i] = 0.0f;
        smoothedOscLevel[i] = 0.0f;
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
        }
        return 0.0f;
    }

#ifdef ENABLE_VOICE_PROFILING
    g_voiceProfiler.totalSamples++;
#endif

    ampGateValue += kAmpGateSmoothingAlpha * (ampGateTarget - ampGateValue);
    ampGateValue = std::clamp(ampGateValue, 0.0f, 1.0f);
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
            envelopeTargets[i] = envelopes[i].processBlock(blockSize);
        }
    }

    if (ampGateTarget <= 0.0f && ampGateValue < kAmpGateSilenceThreshold) {
        bool controllersAudible = false;
        for (int i = 0; i < OSCILLATORS_PER_VOICE && !controllersAudible; ++i) {
            if (oscAmpControllerActive[i] && oscAmpControllerValue[i] > kAmpGateSilenceThreshold) {
                controllersAudible = true;
            }
        }
        for (int i = 0; i < SAMPLERS_PER_VOICE && !controllersAudible; ++i) {
            if (samplerAmpControllerActive[i] && samplerAmpControllerValue[i] > kAmpGateSilenceThreshold) {
                controllersAudible = true;
            }
        }
        if (!controllersAudible) {
            if (synth) {
                for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
                    synth->saveSamplerPhase(i, samplers[i].getCurrentPhase());
                }
            }
            active = false;
            for (int i = 0; i < 4; ++i) {
                envelopeValues[i] = 0.0f;
                envelopeTargets[i] = 0.0f;
            }
            resetFMHistory();
            resetAmpControllers();
            ampGateValue = 0.0f;
            ampGateTarget = 0.0f;
            return 0.0f;
        }
    }

    constexpr float alpha = kSmoothingAlpha;
#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_START(t_smoothing);
#endif

    if (!restrictSamplers) {
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            smoothedPitchMod[i] += alpha * (pitchMod[i] - smoothedPitchMod[i]);
            smoothedAmpMod[i] += alpha * (ampMod[i] - smoothedAmpMod[i]);
            smoothedOscLevel[i] += alpha * (cachedOscLevel[i] - smoothedOscLevel[i]);
        }
    }
    if (!restrictOscillators) {
        for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
            smoothedSamplerPitchMod[i] += alpha * (samplerPitchMod[i] - smoothedSamplerPitchMod[i]);
            smoothedSamplerLevelMod[i] += alpha * (samplerLevelMod[i] - smoothedSamplerLevelMod[i]);
        }
    }

    for (int i = 0; i < 4; ++i) {
        envelopeValues[i] += alpha * (envelopeTargets[i] - envelopeValues[i]);
    }

#ifdef ENABLE_VOICE_PROFILING
    VOICE_PROFILE_END(timeSmoothing, t_smoothing);
#endif

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
    if (!restrictSamplers) {
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            if (i >= cachedActiveOscCount) {
                continue;
            }
            // FREE mode oscillators: only run in free-running voices (freeRunningOscIndex >= 0)
            if (oscillators[i].getMode() == BrainwaveMode::FREE && freeRunningOscIndex < 0) {
                continue;
            }
            // KEY mode oscillators: only run when note source matches AND not in a free-running voice
            if (oscillators[i].getMode() == BrainwaveMode::KEY) {
                if (freeRunningOscIndex >= 0) {
                    continue;
                }
                if (params) {
                    int oscNoteSource = params->getOscNoteSource(i);
                    if (oscNoteSource != noteSource) {
                        continue;
                    }
                }
            }
            float oscSample = oscillators[i].process(sampleRate,
                                                     fmInputs[i],
                                                     smoothedPitchMod[i],
                                                     morphMod[i],
                                                     ratioMod[i],
                                                     offsetMod[i]);
            if (!std::isfinite(oscSample)) {
                oscSample = 0.0f;
            }
            currentOutputs[i] = oscSample;
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

            float ampControl = oscAmpControllerActive[i] ? oscAmpControllerValue[i] : ampGateValue;
            finalGain *= std::clamp(ampControl, 0.0f, 1.0f);
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
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        if (i >= cachedActiveSamplerCount) {
            currentSamplerOutputs[i] = 0.0f;
            samplerFinalOutputs[i] = 0.0f;
            continue;
        }
        if (restrictOscillators) {
            currentSamplerOutputs[i] = 0.0f;
            samplerFinalOutputs[i] = 0.0f;
            continue;
        }

        bool samplerKeyMode = samplers[i].isKeyMode();
        bool processSampler = false;
        if (samplerKeyMode) {
            if (!restrictSamplers) {
                if (!params) {
                    processSampler = true;
                } else {
                    int samplerNoteSource = params->getSamplerNoteSource(i);
                    processSampler = (samplerNoteSource == noteSource);
                }
            }
        } else {
            processSampler = (restrictSamplers && restrictedSampler == i);
        }
        if (!processSampler) {
            currentSamplerOutputs[i] = 0.0f;
            samplerFinalOutputs[i] = 0.0f;
            continue;
        }

        float samplerOut = samplers[i].process(sampleRate,
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
        currentSamplerOutputs[i] = samplerOut;

        if (cachedAnySolo) {
            if (!cachedSamplerSolo[i]) {
                samplerOut = 0.0f;
            }
        } else if (cachedSamplerMuted[i]) {
            samplerOut = 0.0f;
        }

        float samplerControl = samplerAmpControllerActive[i] ? samplerAmpControllerValue[i] : ampGateValue;
        samplerFinalOutputs[i] = samplerOut * std::clamp(samplerControl, 0.0f, 1.0f);
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

    return mixedSample;
}
