#include "voice.h"
#include "synth.h"  // For Synth::getOscillatorBaseLevel()
#include "ui.h"    // For SynthParameters definition
#include "voice_profiler.h"

// Global profiler instance
VoiceProfiler g_voiceProfiler;

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
    for (int i = 0; i < 4; ++i) {
        envelopes[i].reset();
        envelopeValues[i] = 0.0f;
        envelopeTargets[i] = 0.0f;
    }
    cachedAnySolo = false;
    cachedActiveOscCount = OSCILLATORS_PER_VOICE;
    cachedActiveSamplerCount = SAMPLERS_PER_VOICE;
    cachedFmGlobalDepthBase = 0.0f;
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
        // Non-critical (direct)
        morphMod[i] = 0.0f;
        prevMorphMod[i] = 0.0f;
        prevMorphMod[i] = 0.0f;
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

    ampGateValue += kAmpGateSmoothingAlpha * (ampGateTarget - ampGateValue);
    ampGateValue = std::clamp(ampGateValue, 0.0f, 1.0f);

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
        cachedFmGlobalDepthBase = params ? params->fmGlobalDepth.load() : 0.0f;
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
    constexpr float fmAlpha = kFmDepthSmoothingAlpha;

    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        smoothedPitchMod[i] += alpha * (pitchMod[i] - smoothedPitchMod[i]);
        smoothedAmpMod[i] += alpha * (ampMod[i] - smoothedAmpMod[i]);
        smoothedOscLevel[i] += alpha * (cachedOscLevel[i] - smoothedOscLevel[i]);
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        smoothedSamplerPitchMod[i] += alpha * (samplerPitchMod[i] - smoothedSamplerPitchMod[i]);
        smoothedSamplerLevelMod[i] += alpha * (samplerLevelMod[i] - smoothedSamplerLevelMod[i]);
    }

    smoothedFmGlobalDepthMod += alpha * (fmGlobalDepthMod - smoothedFmGlobalDepthMod);
    float globalDepth = std::clamp(cachedFmGlobalDepthBase + smoothedFmGlobalDepthMod, 0.0f, 1.0f);

    // Smooth FM depth matrix for musical UI control
    for (int t = 0; t < kFMTargetCount; ++t) {
        for (int s = 0; s < kFMSourceCount; ++s) {
            smoothedFmDepthMod[t][s] += fmAlpha * (fmDepthMod[t][s] - smoothedFmDepthMod[t][s]);
        }
    }

    for (int i = 0; i < 4; ++i) {
        envelopeValues[i] += alpha * (envelopeTargets[i] - envelopeValues[i]);
    }

    //     g_voiceProfiler.timeSmoothing += g_voiceProfiler.elapsed_ns(t_start);

    // === PROFILING: Oscillator FM ===
    //     //     auto t_osc_fm = g_voiceProfiler.now();

    float fmInputs[OSCILLATORS_PER_VOICE] = {0.0f};
    for (int target = 0; target < OSCILLATORS_PER_VOICE; ++target) {
        if (target >= cachedActiveOscCount) {
            fmInputs[target] = 0.0f;
            continue;
        }
        float totalFM = 0.0f;
        for (int source = 0; source < OSCILLATORS_PER_VOICE; ++source) {
            if (source >= cachedActiveOscCount) continue;
            float depth = smoothedFmDepthMod[target][source];
            if (depth != 0.0f) {
                totalFM += lastOscOutputs[source] * (depth * 100.0f);
            }
        }
        for (int source = 0; source < SAMPLERS_PER_VOICE; ++source) {
            if (source >= cachedActiveSamplerCount) continue;
            int sourceIdx = kFMOscillatorTargetCount + source;
            float depth = smoothedFmDepthMod[target][sourceIdx];
            if (depth != 0.0f) {
                totalFM += lastSamplerOutputs[source] * (depth * 100.0f);
            }
        }
        fmInputs[target] = totalFM * globalDepth;
    }

    //     g_voiceProfiler.timeOscFM += g_voiceProfiler.elapsed_ns(t_osc_fm);

    // === PROFILING: Oscillator Processing ===
    //     //     auto t_osc_proc = g_voiceProfiler.now();

    float currentOutputs[OSCILLATORS_PER_VOICE];
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        if (i >= cachedActiveOscCount) {
            currentOutputs[i] = 0.0f;
            continue;
        }
        float frameT = (currentBufferSize <= 1)
            ? 1.0f
            : static_cast<float>(frameIndex) / static_cast<float>(currentBufferSize - 1);
        float morphInterp = prevMorphMod[i] + frameT * (morphMod[i] - prevMorphMod[i]);

        currentOutputs[i] = oscillators[i].process(sampleRate,
                                                   fmInputs[i],
                                                   smoothedPitchMod[i],
                                                   morphInterp,
                                                   ratioMod[i],
                                                   offsetMod[i]);
        if (!std::isfinite(currentOutputs[i])) {
            currentOutputs[i] = 0.0f;
        }
    }

    //     g_voiceProfiler.timeOscProcess += g_voiceProfiler.elapsed_ns(t_osc_proc);

    // === PROFILING: Oscillator Gain Calculation ===
    //     //     auto t_osc_gain = g_voiceProfiler.now();

    float mixedSample = 0.0f;
    float oscFinalGains[OSCILLATORS_PER_VOICE];
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        if (i >= cachedActiveOscCount) {
            oscFinalGains[i] = 0.0f;
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

    //     g_voiceProfiler.timeOscGainCalc += g_voiceProfiler.elapsed_ns(t_osc_gain);

    // === PROFILING: Sampler FM ===
    //     //     auto t_samp_fm = g_voiceProfiler.now();

    float samplerFMInputs[SAMPLERS_PER_VOICE] = {0.0f};
    for (int target = 0; target < SAMPLERS_PER_VOICE; ++target) {
        if (target >= cachedActiveSamplerCount) {
            samplerFMInputs[target] = 0.0f;
            continue;
        }
        int targetIndex = kFMOscillatorTargetCount + target;
        float totalFM = 0.0f;
        for (int source = 0; source < OSCILLATORS_PER_VOICE; ++source) {
            if (source >= cachedActiveOscCount) continue;
            float depth = smoothedFmDepthMod[targetIndex][source];
            if (depth != 0.0f) {
                totalFM += lastOscOutputs[source] * (depth * 100.0f);
            }
        }
        for (int source = 0; source < SAMPLERS_PER_VOICE; ++source) {
            if (source >= cachedActiveSamplerCount) continue;
            int sourceIdx = kFMOscillatorTargetCount + source;
            float depth = smoothedFmDepthMod[targetIndex][sourceIdx];
            if (depth != 0.0f) {
                totalFM += lastSamplerOutputs[source] * (depth * 100.0f);
            }
        }
        samplerFMInputs[target] = totalFM * globalDepth;
    }

    //     g_voiceProfiler.timeSamplerFM += g_voiceProfiler.elapsed_ns(t_samp_fm);

    // === PROFILING: Sampler Processing ===
    //     //     auto t_samp_proc = g_voiceProfiler.now();

    float currentSamplerOutputs[SAMPLERS_PER_VOICE] = {0.0f};
    float samplerFinalOutputs[SAMPLERS_PER_VOICE] = {0.0f};
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        if (i >= cachedActiveSamplerCount) {
            currentSamplerOutputs[i] = 0.0f;
            samplerFinalOutputs[i] = 0.0f;
            continue;
        }
        if (!samplers[i].isKeyMode()) {
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

    //     g_voiceProfiler.timeSamplerProcess += g_voiceProfiler.elapsed_ns(t_samp_proc);

    // === PROFILING: Mixing ===
    //     //     auto t_mix = g_voiceProfiler.now();

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

    //     g_voiceProfiler.timeMixing += g_voiceProfiler.elapsed_ns(t_mix);

    // === PROFILING: History Update ===
    //     //     auto t_history = g_voiceProfiler.now();

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

    //     g_voiceProfiler.timeHistoryUpdate += g_voiceProfiler.elapsed_ns(t_history);

    return mixedSample;
}
