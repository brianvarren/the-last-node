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

float Voice::generateSample(unsigned int frameIndex) {
    if (!active) {
        envelopeValue = 0.0f;
        return 0.0f;
    }

    // Get envelope level and cache for modulation routing
    envelopeValue = envelope.process();

    // If envelope finished, deactivate voice and clear FM history
    if (!envelope.isActive()) {
        // Save sampler phases before deactivating (for Note Reset OFF)
        if (synth) {
            for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
                synth->saveSamplerPhase(i, samplers[i].getCurrentPhase());
            }
        }
        active = false;
        envelopeValue = 0.0f;
        resetFMHistory();
        return 0.0f;
    }

    // Determine FM input for each oscillator using previous outputs (1-sample delay)
    // FM matrix Targets: OSC1-4 (0-3), SAMP1-4 (4-7), CHAOS CLK1-4 (8-11)
    // Sources: OSC1-4 (0-3), SAMP1-4 (4-7), Chaos1X-4Y (8-15)
    auto getModulatedDepth = [&](int targetIndex, int sourceIndex) -> float {
        if (!params) {
            return 0.0f;
        }
        float depth = params->getFMDepth(targetIndex, sourceIndex);
        if (synth) {
            depth += synth->getFMDepthMod(targetIndex, sourceIndex);
        }
        depth += fmDepthMod[targetIndex][sourceIndex];
        return std::clamp(depth, -0.99f, 0.99f);
    };

    float fmInputs[OSCILLATORS_PER_VOICE] = {0.0f};
    if (params && synth) {
        // Get global FM depth (base + modulation)
        float globalDepth = std::clamp(params->fmGlobalDepth.load() + fmGlobalDepthMod, 0.0f, 1.0f);

        for (int target = 0; target < OSCILLATORS_PER_VOICE; ++target) {
            float totalFM = 0.0f;
            // Oscillator sources (0-3)
            for (int source = 0; source < OSCILLATORS_PER_VOICE; ++source) {
                float depth = getModulatedDepth(target, source);
                if (depth != 0.0f) {
                    totalFM += lastOscOutputs[source] * (depth * 100.0f);
                }
            }
            // Sampler sources (4-7)
            for (int source = 0; source < SAMPLERS_PER_VOICE; ++source) {
                float depth = getModulatedDepth(target, kFMOscillatorTargetCount + source);
                if (depth != 0.0f) {
                    totalFM += lastSamplerOutputs[source] * (depth * 100.0f);
                }
            }
    // Chaos sources (8-11): C1X, C2X, C3X, C4X
    for (int source = 0; source < kFMChaosSourceCount; ++source) {
        int sourceIndex = kFMOscillatorTargetCount + SAMPLERS_PER_VOICE + source;
        float depth = getModulatedDepth(target, sourceIndex);
        if (depth != 0.0f) {
            int chaosIndex = source;
            float chaosOutput = synth->getChaosOutputAtFrame(chaosIndex, frameIndex);
            totalFM += chaosOutput * (depth * 100.0f);
        }
    }
            fmInputs[target] = totalFM * globalDepth;
        }
    }

    // Generate current oscillator outputs with FM and modulation applied
    float currentOutputs[OSCILLATORS_PER_VOICE];
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        currentOutputs[i] = oscillators[i].process(sampleRate, fmInputs[i],
                                                    pitchMod[i], morphMod[i], dutyMod[i],
                                                    ratioMod[i], offsetMod[i]);
        // Sanitize oscillator output
        if (!std::isfinite(currentOutputs[i])) {
            currentOutputs[i] = 0.0f;
        }
    }

    // Check if any channels are solo'd (OSC, SAMP, or CHAOS)
    bool anySolo = false;
    if (params) {
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            if (params->oscSolo[i].load()) { anySolo = true; break; }
        }
        if (!anySolo) {
            for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
                if (params->samplerSolo[i].load()) { anySolo = true; break; }
            }
        }
        if (!anySolo) {
            for (int i = 0; i < 4; ++i) {
                if (params->chaosSolo[i].load()) { anySolo = true; break; }
            }
        }
    }

    // Phase 2: Mix all oscillators (calculate gains first, normalize with samplers later)
    // Amp is the modulation target, Level is the static mixer
    float mixedSample = 0.0f;

    // First pass: calculate oscillator final gains
    float oscFinalGains[OSCILLATORS_PER_VOICE];
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        // Get base amp and level from synth (control-rate)
        float baseAmp = synth ? synth->getOscillatorBaseAmp(i) : 1.0f;
        float baseLevel = synth ? synth->getModulatedOscLevel(i) : 0.0f;

        // Calculate modulated amplitude (clamped 0-1)
        float modulatedAmp = std::min(std::max(baseAmp + ampMod[i], 0.0f), 1.0f);

        // Final gain is amp × level
        float finalGain = modulatedAmp * baseLevel;

        // Apply mute/solo logic
        if (params) {
            bool isSolo = params->oscSolo[i].load();
            bool isMuted = params->oscMuted[i].load();

            if (anySolo) {
                // If any channel is solo'd, only play solo'd channels
                if (!isSolo) {
                    finalGain = 0.0f;
                }
            } else if (isMuted) {
                // If no solo and this channel is muted, silence it
                finalGain = 0.0f;
            }
        }

        oscFinalGains[i] = finalGain;
    }

    // Determine FM input for each sampler using previous outputs (1-sample delay)
    float samplerFMInputs[SAMPLERS_PER_VOICE] = {0.0f};
    if (params && synth) {
        // Get global FM depth (base + modulation)
        float globalDepth = std::clamp(params->fmGlobalDepth.load() + fmGlobalDepthMod, 0.0f, 1.0f);

        for (int target = 0; target < SAMPLERS_PER_VOICE; ++target) {
            int targetIndex = kFMOscillatorTargetCount + target;
            float totalFM = 0.0f;
            // Oscillator sources (0-3)
            for (int source = 0; source < OSCILLATORS_PER_VOICE; ++source) {
                float depth = getModulatedDepth(targetIndex, source);
                if (depth != 0.0f) {
                    totalFM += lastOscOutputs[source] * (depth * 100.0f);
                }
            }
            // Sampler sources (4-7)
            for (int source = 0; source < SAMPLERS_PER_VOICE; ++source) {
                float depth = getModulatedDepth(targetIndex, kFMOscillatorTargetCount + source);
                if (depth != 0.0f) {
                    totalFM += lastSamplerOutputs[source] * (depth * 100.0f);
                }
            }
            // Chaos sources (8-15)
#if 0  // TEMP: disable sampler chaos FM routing while investigating multi-voice crackle (commit 495fdff)
            for (int source = 0; source < kFMChaosSourceCount; ++source) {
                int sourceIndex = kFMOscillatorTargetCount + SAMPLERS_PER_VOICE + source;
                float depth = getModulatedDepth(targetIndex, sourceIndex);
                if (depth != 0.0f) {
                    int chaosIndex = source;
                    float chaosOutput = synth->getChaosOutput(chaosIndex);
                    totalFM += chaosOutput * (depth * 100.0f);
                }
            }
#endif
            samplerFMInputs[target] = totalFM * globalDepth;
        }
    }

    // Phase 2: Process all samplers (will normalize with oscillators combined)
    float currentSamplerOutputs[SAMPLERS_PER_VOICE] = {0.0f};
    float samplerFinalOutputs[SAMPLERS_PER_VOICE] = {0.0f};

    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        if (!samplers[i].isKeyMode()) {
            currentSamplerOutputs[i] = 0.0f;
            samplerFinalOutputs[i] = 0.0f;
            continue;
        }

        // Process sampler with FM and modulation (pass MIDI note for KEY mode tracking)
        float samplerLevelOffset = synth ? synth->getMixerSamplerLevelMod(i) : 0.0f;
        float samplerOut = samplers[i].process(sampleRate, samplerFMInputs[i],
                                              samplerPitchMod[i],
                                              samplerLoopStartMod[i],
                                              samplerLoopLengthMod[i],
                                              samplerCrossfadeMod[i],
                                              samplerLevelMod[i],
                                              samplerLevelOffset,
                                              samplerPhaseDriver[i],
                                              note);
        // Sanitize sampler output
        if (!std::isfinite(samplerOut)) {
            samplerOut = 0.0f;
        }
        currentSamplerOutputs[i] = samplerOut;

        // Apply mute/solo logic to samplers
        if (params) {
            bool isSolo = params->samplerSolo[i].load();
            bool isMuted = params->samplerMuted[i].load();

            if (anySolo) {
                // If any channel is solo'd, only play solo'd channels
                if (!isSolo) {
                    samplerOut = 0.0f;
                }
            } else if (isMuted) {
                // If no solo and this channel is muted, silence it
                samplerOut = 0.0f;
            }
        }

        samplerFinalOutputs[i] = samplerOut;
    }

    // Phase 2 FIX: Normalize ALL sources together (oscillators + samplers)
    // Count total active sources with non-zero, finite output
    int totalActiveSources = 0;
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        if (oscFinalGains[i] > 0.0f && std::isfinite(currentOutputs[i])) {
            totalActiveSources++;
        }
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        if (samplerFinalOutputs[i] != 0.0f && std::isfinite(samplerFinalOutputs[i])) {
            totalActiveSources++;
        }
    }

    // Normalize by total source count to prevent voice from exceeding 1.0
    float sourceNormalization = (totalActiveSources > 0) ? (1.0f / totalActiveSources) : 1.0f;

    // Mix oscillators with combined normalization
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        mixedSample += currentOutputs[i] * oscFinalGains[i] * sourceNormalization;
    }

    // Mix samplers with combined normalization
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        mixedSample += samplerFinalOutputs[i] * sourceNormalization;
    }

    // Sanitize output to prevent NaN/Inf from propagating
    if (!std::isfinite(mixedSample)) {
        mixedSample = 0.0f;
    }

    // Cache outputs for next sample's FM routing
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        lastOscOutputs[i] = currentOutputs[i];
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        lastSamplerOutputs[i] = currentSamplerOutputs[i];
    }

    // Return mixed sample WITHOUT envelope multiplication
    // Envelope is now routed through modulation matrix to oscillator levels
    return mixedSample;
}
