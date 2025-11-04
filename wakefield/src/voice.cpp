#include "voice.h"
#include "synth.h"  // For Synth::getOscillatorBaseLevel()
#include "ui.h"    // For SynthParameters definition

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
        dutyMod[i] = 0.0f;
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

        if (!envelopes[0].isActive()) {
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
            return 0.0f;
        }

        cachedFmGlobalDepthBase = params ? params->fmGlobalDepth.load() : 0.0f;
    }

    constexpr float alpha = kSmoothingAlpha;

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

    for (int i = 0; i < 4; ++i) {
        envelopeValues[i] += alpha * (envelopeTargets[i] - envelopeValues[i]);
    }

    float fmInputs[OSCILLATORS_PER_VOICE] = {0.0f};
    for (int target = 0; target < OSCILLATORS_PER_VOICE; ++target) {
        if (target >= cachedActiveOscCount) {
            fmInputs[target] = 0.0f;
            continue;
        }
        float totalFM = 0.0f;
        for (int source = 0; source < OSCILLATORS_PER_VOICE; ++source) {
            if (source >= cachedActiveOscCount) continue;
            float depth = fmDepthMod[target][source];
            if (depth != 0.0f) {
                totalFM += lastOscOutputs[source] * (depth * 100.0f);
            }
        }
        for (int source = 0; source < SAMPLERS_PER_VOICE; ++source) {
            if (source >= cachedActiveSamplerCount) continue;
            int sourceIdx = kFMOscillatorTargetCount + source;
            float depth = fmDepthMod[target][sourceIdx];
            if (depth != 0.0f) {
                totalFM += lastSamplerOutputs[source] * (depth * 100.0f);
            }
        }
        fmInputs[target] = totalFM * globalDepth;
    }

    float currentOutputs[OSCILLATORS_PER_VOICE];
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        if (i >= cachedActiveOscCount) {
            currentOutputs[i] = 0.0f;
            continue;
        }
        currentOutputs[i] = oscillators[i].process(sampleRate,
                                                   fmInputs[i],
                                                   smoothedPitchMod[i],
                                                   morphMod[i],
                                                   dutyMod[i],
                                                   ratioMod[i],
                                                   offsetMod[i]);
        if (!std::isfinite(currentOutputs[i])) {
            currentOutputs[i] = 0.0f;
        }
    }

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

        finalGain *= envelopeValues[0];
        oscFinalGains[i] = finalGain;
    }

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
            float depth = fmDepthMod[targetIndex][source];
            if (depth != 0.0f) {
                totalFM += lastOscOutputs[source] * (depth * 100.0f);
            }
        }
        for (int source = 0; source < SAMPLERS_PER_VOICE; ++source) {
            if (source >= cachedActiveSamplerCount) continue;
            int sourceIdx = kFMOscillatorTargetCount + source;
            float depth = fmDepthMod[targetIndex][sourceIdx];
            if (depth != 0.0f) {
                totalFM += lastSamplerOutputs[source] * (depth * 100.0f);
            }
        }
        samplerFMInputs[target] = totalFM * globalDepth;
    }

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
                                               note);
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

        samplerFinalOutputs[i] = samplerOut * envelopeValues[0];
    }

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

    return mixedSample;
}
