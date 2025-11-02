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
    envelope.reset();
    envelopeValue = 0.0f;
    resetFMHistory();
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        pitchMod[i] = 0.0f;
        morphMod[i] = 0.0f;
        dutyMod[i] = 0.0f;
        ratioMod[i] = 0.0f;
        offsetMod[i] = 0.0f;
        ampMod[i] = 0.0f;
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        samplerPitchMod[i] = 0.0f;
        samplerLoopStartMod[i] = 0.0f;
        samplerLoopLengthMod[i] = 0.0f;
        samplerCrossfadeMod[i] = 0.0f;
        samplerLevelMod[i] = 0.0f;
        samplerPhaseDriver[i] = -1.0f;
        samplers[i].stopPlayback();
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
    (void)frameIndex;  // No per-sample chaos FM routing in this configuration.
    // FM matrix Targets: OSC1-4 (0-3), SAMP1-4 (4-7)
    // Sources: OSC1-4 (0-3), SAMP1-4 (4-7)
    // OPTIMIZATION: fmDepthMod is now pre-computed per buffer with base + global + voice mod
    // Audio-rate interpolation prevents zippering when envelope modulates FM depths
    
    // Calculate interpolation factor for smooth audio-rate transition between buffer boundaries
    // frameIndex 0 = use prev values, frameIndex bufferSize-1 = use current values
    // This prevents zippering when envelope/LFO modulates control-rate parameters
    float interpFactor = 0.0f;
    if (currentBufferSize > 1) {
        interpFactor = static_cast<float>(frameIndex) / static_cast<float>(currentBufferSize - 1);
        interpFactor = std::clamp(interpFactor, 0.0f, 1.0f);
    }
    
    // Helper lambda for linear interpolation (used for all control-rate parameters)
    auto lerp = [interpFactor](float prev, float curr) -> float {
        return prev + (curr - prev) * interpFactor;
    };
    
    // Interpolate all oscillator modulation values to audio rate
    float interpPitchMod[OSCILLATORS_PER_VOICE];
    float interpMorphMod[OSCILLATORS_PER_VOICE];
    float interpDutyMod[OSCILLATORS_PER_VOICE];
    float interpRatioMod[OSCILLATORS_PER_VOICE];
    float interpOffsetMod[OSCILLATORS_PER_VOICE];
    float interpAmpMod[OSCILLATORS_PER_VOICE];
    float interpCachedOscLevel[OSCILLATORS_PER_VOICE];
    
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        interpPitchMod[i] = lerp(prevPitchMod[i], pitchMod[i]);
        interpMorphMod[i] = lerp(prevMorphMod[i], morphMod[i]);
        interpDutyMod[i] = lerp(prevDutyMod[i], dutyMod[i]);
        interpRatioMod[i] = lerp(prevRatioMod[i], ratioMod[i]);
        interpOffsetMod[i] = lerp(prevOffsetMod[i], offsetMod[i]);
        interpAmpMod[i] = lerp(prevAmpMod[i], ampMod[i]);
        interpCachedOscLevel[i] = lerp(prevCachedOscLevel[i], cachedOscLevel[i]);
    }
    
    // Interpolate sampler modulation values
    float interpSamplerPitchMod[SAMPLERS_PER_VOICE];
    float interpSamplerLoopStartMod[SAMPLERS_PER_VOICE];
    float interpSamplerLoopLengthMod[SAMPLERS_PER_VOICE];
    float interpSamplerCrossfadeMod[SAMPLERS_PER_VOICE];
    float interpSamplerLevelMod[SAMPLERS_PER_VOICE];
    float interpCachedSamplerLevelMod[SAMPLERS_PER_VOICE];
    
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        interpSamplerPitchMod[i] = lerp(prevSamplerPitchMod[i], samplerPitchMod[i]);
        interpSamplerLoopStartMod[i] = lerp(prevSamplerLoopStartMod[i], samplerLoopStartMod[i]);
        interpSamplerLoopLengthMod[i] = lerp(prevSamplerLoopLengthMod[i], samplerLoopLengthMod[i]);
        interpSamplerCrossfadeMod[i] = lerp(prevSamplerCrossfadeMod[i], samplerCrossfadeMod[i]);
        interpSamplerLevelMod[i] = lerp(prevSamplerLevelMod[i], samplerLevelMod[i]);
        interpCachedSamplerLevelMod[i] = lerp(prevCachedSamplerLevelMod[i], cachedSamplerLevelMod[i]);
    }
    
    // Interpolate FM global depth
    float interpFmGlobalDepthMod = lerp(prevFmGlobalDepthMod, fmGlobalDepthMod);
    
    float fmInputs[OSCILLATORS_PER_VOICE] = {0.0f};
    if (params && synth) {
        // Get global FM depth (base + interpolated modulation) - applies to all FM amounts
        float globalDepth = std::clamp(params->fmGlobalDepth.load() + interpFmGlobalDepthMod, 0.0f, 1.0f);

        for (int target = 0; target < OSCILLATORS_PER_VOICE; ++target) {
            float totalFM = 0.0f;
            // Oscillator sources (0-3)
            for (int source = 0; source < OSCILLATORS_PER_VOICE; ++source) {
                // Audio-rate interpolation: smooth transition between buffer boundaries
                float prevDepth = prevFmDepthMod[target][source];
                float currDepth = fmDepthMod[target][source];
                float depth = prevDepth + (currDepth - prevDepth) * interpFactor;
                
                if (depth != 0.0f) {
                    totalFM += lastOscOutputs[source] * (depth * 100.0f);
                }
            }
            // Sampler sources (4-7)
            for (int source = 0; source < SAMPLERS_PER_VOICE; ++source) {
                // Audio-rate interpolation: smooth transition between buffer boundaries
                int targetIdx = kFMOscillatorTargetCount + source;
                float prevDepth = prevFmDepthMod[target][targetIdx];
                float currDepth = fmDepthMod[target][targetIdx];
                float depth = prevDepth + (currDepth - prevDepth) * interpFactor;
                
                if (depth != 0.0f) {
                    totalFM += lastSamplerOutputs[source] * (depth * 100.0f);
                }
            }
            fmInputs[target] = totalFM * globalDepth;
        }
    }

    // Generate current oscillator outputs with FM and interpolated modulation applied
    float currentOutputs[OSCILLATORS_PER_VOICE];
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        // Use interpolated modulation values for smooth audio-rate transitions
        currentOutputs[i] = oscillators[i].process(sampleRate, fmInputs[i],
                                                    interpPitchMod[i], interpMorphMod[i], interpDutyMod[i],
                                                    interpRatioMod[i], interpOffsetMod[i]);
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
        // Get base amp and interpolated level (level is pre-computed per buffer, interpolated to audio rate)
        float baseAmp = synth ? synth->getOscillatorBaseAmp(i) : 1.0f;
        float baseLevel = interpCachedOscLevel[i];  // Interpolated to audio rate - prevents zippering!

        // Calculate modulated amplitude (clamped 0-1) - use interpolated ampMod
        float modulatedAmp = std::min(std::max(baseAmp + interpAmpMod[i], 0.0f), 1.0f);

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

        // Apply per-sample envelope so amplitude follows ADSR without zippering
        finalGain *= envelopeValue;

        oscFinalGains[i] = finalGain;
    }

    // Determine FM input for each sampler using previous outputs (1-sample delay)
    // Note: interpFactor already calculated above for oscillator FM
    float samplerFMInputs[SAMPLERS_PER_VOICE] = {0.0f};
    if (params && synth) {
        // Get global FM depth (base + interpolated modulation)
        float globalDepth = std::clamp(params->fmGlobalDepth.load() + interpFmGlobalDepthMod, 0.0f, 1.0f);

        for (int target = 0; target < SAMPLERS_PER_VOICE; ++target) {
            int targetIndex = kFMOscillatorTargetCount + target;
            float totalFM = 0.0f;
            // Oscillator sources (0-3)
            for (int source = 0; source < OSCILLATORS_PER_VOICE; ++source) {
                // Audio-rate interpolation: smooth transition between buffer boundaries
                float prevDepth = prevFmDepthMod[targetIndex][source];
                float currDepth = fmDepthMod[targetIndex][source];
                float depth = prevDepth + (currDepth - prevDepth) * interpFactor;
                
                if (depth != 0.0f) {
                    totalFM += lastOscOutputs[source] * (depth * 100.0f);
                }
            }
            // Sampler sources (4-7)
            for (int source = 0; source < SAMPLERS_PER_VOICE; ++source) {
                // Audio-rate interpolation: smooth transition between buffer boundaries
                int sourceIdx = kFMOscillatorTargetCount + source;
                float prevDepth = prevFmDepthMod[targetIndex][sourceIdx];
                float currDepth = fmDepthMod[targetIndex][sourceIdx];
                float depth = prevDepth + (currDepth - prevDepth) * interpFactor;
                
                if (depth != 0.0f) {
                    totalFM += lastSamplerOutputs[source] * (depth * 100.0f);
                }
            }
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

        // Process sampler with FM and interpolated modulation (pass MIDI note for KEY mode tracking)
        float samplerLevelOffset = interpCachedSamplerLevelMod[i];  // Interpolated to audio rate!
        float samplerOut = samplers[i].process(sampleRate, samplerFMInputs[i],
                                              interpSamplerPitchMod[i],
                                              interpSamplerLoopStartMod[i],
                                              interpSamplerLoopLengthMod[i],
                                              interpSamplerCrossfadeMod[i],
                                              interpSamplerLevelMod[i],
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

        // Apply envelope post mute/solo so release tails are sample accurate
        samplerFinalOutputs[i] = samplerOut * envelopeValue;
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
