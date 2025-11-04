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
        for (int i = 0; i < 4; ++i) envelopeValues[i] = 0.0f;
        return 0.0f;
    }

    // Get envelope levels and cache for modulation routing (ENV1..ENV4)
    for (int i = 0; i < 4; ++i) {
        envelopeValues[i] = envelopes[i].process();
    }

    // If envelope finished, deactivate voice and clear FM history
    if (!envelopes[0].isActive()) {
        // Save sampler phases before deactivating (for Note Reset OFF)
        if (synth) {
            for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
                synth->saveSamplerPhase(i, samplers[i].getCurrentPhase());
            }
        }
        active = false;
        for (int i = 0; i < 4; ++i) envelopeValues[i] = 0.0f;
        resetFMHistory();
        return 0.0f;
    }

    // Apply one-pole smoothing to CRITICAL parameters (prevents zipper noise)
    // Formula: smoothed += alpha * (target - smoothed)
    // Alpha = 0.02 settles in ~100 samples (~2ms @ 48kHz)
    constexpr float alpha = kSmoothingAlpha;

    // Smooth oscillator critical params
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        smoothedPitchMod[i] += alpha * (pitchMod[i] - smoothedPitchMod[i]);
        smoothedAmpMod[i] += alpha * (ampMod[i] - smoothedAmpMod[i]);
        smoothedOscLevel[i] += alpha * (cachedOscLevel[i] - smoothedOscLevel[i]);
    }

    // Smooth sampler critical params
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        smoothedSamplerPitchMod[i] += alpha * (samplerPitchMod[i] - smoothedSamplerPitchMod[i]);
        smoothedSamplerLevelMod[i] += alpha * (samplerLevelMod[i] - smoothedSamplerLevelMod[i]);
    }

    // Smooth FM global depth
    smoothedFmGlobalDepthMod += alpha * (fmGlobalDepthMod - smoothedFmGlobalDepthMod);
    
    float fmInputs[OSCILLATORS_PER_VOICE] = {0.0f};
    int activeOscCount = OSCILLATORS_PER_VOICE;
    int activeSamplerCount = SAMPLERS_PER_VOICE;
    if (params) {
        activeOscCount = std::clamp(params->activeOscCount.load(), 1, OSCILLATORS_PER_VOICE);
        activeSamplerCount = std::clamp(params->activeSamplerCount.load(), 1, SAMPLERS_PER_VOICE);
    }
    if (params && synth) {
        // Get global FM depth (base + smoothed modulation) - applies to all FM amounts
        float globalDepth = std::clamp(params->fmGlobalDepth.load() + smoothedFmGlobalDepthMod, 0.0f, 1.0f);

        // Initialize sub-buffer FM depth slices at frame 0
        if (frameIndex == 0 || !fmSliceInitialized) {
            fmSliceInterval = std::max(1u, currentBufferSize / static_cast<unsigned int>(kFmDepthSlices));
            fmSliceStartFrame = 0;
            fmSliceEndFrame = std::min(static_cast<unsigned int>(fmSliceInterval), currentBufferSize - 1);
            // Fill both prev and curr with current modulation state (will update curr at first boundary)
            auto sliceOut = synth->processModulationMatrix(this);
            for (int t = 0; t < kFMTargetCount; ++t) {
                for (int s = 0; s < kFMSourceCount; ++s) {
                    fmDepthSlicePrev[t][s] = sliceOut.fmDepth[t][s];
                    fmDepthSliceCurr[t][s] = sliceOut.fmDepth[t][s];
                }
            }
            fmSliceInitialized = true;
        }

        // Advance slice boundary and refresh current slice as needed
        if (static_cast<int>(frameIndex) >= fmSliceEndFrame && fmSliceEndFrame < static_cast<int>(currentBufferSize)) {
            // Shift curr to prev, compute new curr at this boundary
            for (int t = 0; t < kFMTargetCount; ++t) {
                for (int s = 0; s < kFMSourceCount; ++s) {
                    fmDepthSlicePrev[t][s] = fmDepthSliceCurr[t][s];
                }
            }
            auto sliceOut = synth->processModulationMatrix(this);
            for (int t = 0; t < kFMTargetCount; ++t) {
                for (int s = 0; s < kFMSourceCount; ++s) {
                    fmDepthSliceCurr[t][s] = sliceOut.fmDepth[t][s];
                }
            }
            fmSliceStartFrame = fmSliceEndFrame;
            fmSliceEndFrame = std::min(fmSliceEndFrame + fmSliceInterval, static_cast<int>(currentBufferSize) - 1);
        }

        // Compute slice interpolation factor (0..1 within current slice)
        float sliceSpan = std::max(1, fmSliceEndFrame - fmSliceStartFrame);
        float sliceT = std::clamp((static_cast<int>(frameIndex) - fmSliceStartFrame) / static_cast<float>(sliceSpan), 0.0f, 1.0f);

        for (int target = 0; target < OSCILLATORS_PER_VOICE; ++target) {
            if (target >= activeOscCount) { fmInputs[target] = 0.0f; continue; }
            float totalFM = 0.0f;
            // Oscillator sources (0-3)
            for (int source = 0; source < OSCILLATORS_PER_VOICE; ++source) {
                if (source >= activeOscCount) continue;
                // Use block-rate base depth + per-slice interpolated voice modulation
                float depth = fmDepthMod[target][source];
                float add = fmDepthSlicePrev[target][source] + (fmDepthSliceCurr[target][source] - fmDepthSlicePrev[target][source]) * sliceT;
                depth = std::clamp(depth + add, -0.99f, 0.99f);

                if (depth != 0.0f) {
                    totalFM += lastOscOutputs[source] * (depth * 100.0f);
                }
            }
            // Sampler sources (4-7)
            for (int source = 0; source < SAMPLERS_PER_VOICE; ++source) {
                if (source >= activeSamplerCount) continue;
                int targetIdx = kFMOscillatorTargetCount + source;
                float depth = fmDepthMod[target][targetIdx];
                // Per-slice interpolated voice modulation
                int sourceIdx = kFMOscillatorTargetCount + source;
                float add = fmDepthSlicePrev[target][sourceIdx] + (fmDepthSliceCurr[target][sourceIdx] - fmDepthSlicePrev[target][sourceIdx]) * sliceT;
                depth = std::clamp(depth + add, -0.99f, 0.99f);

                if (depth != 0.0f) {
                    totalFM += lastSamplerOutputs[source] * (depth * 100.0f);
                }
            }
            fmInputs[target] = totalFM * globalDepth;
        }
    }

    // Generate current oscillator outputs with FM and modulation applied
    // Critical params (pitch) use smoothed values, non-critical (morph/duty/ratio/offset) use direct block-rate
    float currentOutputs[OSCILLATORS_PER_VOICE];
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        if (i >= activeOscCount) { currentOutputs[i] = 0.0f; continue; }
        currentOutputs[i] = oscillators[i].process(sampleRate, fmInputs[i],
                                                    smoothedPitchMod[i],  // Smoothed (critical)
                                                    morphMod[i],          // Direct (non-critical)
                                                    dutyMod[i],           // Direct (non-critical)
                                                    ratioMod[i],          // Direct (non-critical)
                                                    offsetMod[i]);        // Direct (non-critical)
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
        if (i >= activeOscCount) { oscFinalGains[i] = 0.0f; continue; }
        // Get base amp and smoothed level (critical for zipper prevention)
        float baseAmp = synth ? synth->getOscillatorBaseAmp(i) : 1.0f;
        float baseLevel = smoothedOscLevel[i];  // Smoothed (critical)

        // Calculate modulated amplitude (clamped 0-1) - use smoothed ampMod
        float modulatedAmp = std::min(std::max(baseAmp + smoothedAmpMod[i], 0.0f), 1.0f);

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
        finalGain *= envelopeValues[0];

        oscFinalGains[i] = finalGain;
    }

    // Determine FM input for each sampler using previous outputs (1-sample delay)
    float samplerFMInputs[SAMPLERS_PER_VOICE] = {0.0f};
    if (params && synth) {
        // Get global FM depth (base + smoothed modulation)
        float globalDepth = std::clamp(params->fmGlobalDepth.load() + smoothedFmGlobalDepthMod, 0.0f, 1.0f);

        for (int target = 0; target < SAMPLERS_PER_VOICE; ++target) {
            if (target >= activeSamplerCount) { samplerFMInputs[target] = 0.0f; continue; }
            int targetIndex = kFMOscillatorTargetCount + target;
            float totalFM = 0.0f;
            // Oscillator sources (0-3)
            for (int source = 0; source < OSCILLATORS_PER_VOICE; ++source) {
                if (source >= activeOscCount) continue;
                float depth = fmDepthMod[targetIndex][source];

                if (depth != 0.0f) {
                    totalFM += lastOscOutputs[source] * (depth * 100.0f);
                }
            }
            // Sampler sources (4-7)
            for (int source = 0; source < SAMPLERS_PER_VOICE; ++source) {
                if (source >= activeSamplerCount) continue;
                int sourceIdx = kFMOscillatorTargetCount + source;
                float depth = fmDepthMod[targetIndex][sourceIdx];

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
        if (i >= activeSamplerCount) { currentSamplerOutputs[i] = 0.0f; samplerFinalOutputs[i] = 0.0f; continue; }
        if (!samplers[i].isKeyMode()) {
            currentSamplerOutputs[i] = 0.0f;
            samplerFinalOutputs[i] = 0.0f;
            continue;
        }

        // Process sampler with FM and modulation
        // Critical params (pitch, level) use smoothed values, non-critical use direct block-rate
        float samplerLevelOffset = cachedSamplerLevelMod[i];  // Direct (non-critical)
        float samplerOut = samplers[i].process(sampleRate, samplerFMInputs[i],
                                              smoothedSamplerPitchMod[i],    // Smoothed (critical)
                                              samplerLoopStartMod[i],         // Direct (non-critical)
                                              samplerLoopLengthMod[i],        // Direct (non-critical)
                                              samplerCrossfadeMod[i],         // Direct (non-critical)
                                              smoothedSamplerLevelMod[i],     // Smoothed (critical)
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

        // Apply the voice envelope to sampler level (consistent with oscillator gains)
        samplerFinalOutputs[i] = samplerOut * envelopeValues[0];
    }

    // Phase 2 FIX: Normalize ALL sources together (oscillators + samplers)
    // Count total active sources with non-zero, finite output
    int totalActiveSources = 0;
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        if (i >= activeOscCount) continue;
        if (oscFinalGains[i] > 0.0f && std::isfinite(currentOutputs[i])) {
            totalActiveSources++;
        }
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        if (i >= activeSamplerCount) continue;
        if (samplerFinalOutputs[i] != 0.0f && std::isfinite(samplerFinalOutputs[i])) {
            totalActiveSources++;
        }
    }

    // Normalize by total source count to prevent voice from exceeding 1.0
    float sourceNormalization = (totalActiveSources > 0) ? (1.0f / totalActiveSources) : 1.0f;

    // Mix oscillators with combined normalization
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        if (i >= activeOscCount) continue;
        mixedSample += currentOutputs[i] * oscFinalGains[i] * sourceNormalization;
    }

    // Mix samplers with combined normalization
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        if (i >= activeSamplerCount) continue;
        mixedSample += samplerFinalOutputs[i] * sourceNormalization;
    }

    // Sanitize output to prevent NaN/Inf from propagating
    if (!std::isfinite(mixedSample)) {
        mixedSample = 0.0f;
    }

    // Cache outputs for next sample's FM routing
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        if (i >= activeOscCount) { lastOscOutputs[i] = 0.0f; continue; }
        lastOscOutputs[i] = currentOutputs[i];
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        if (i >= activeSamplerCount) { lastSamplerOutputs[i] = 0.0f; continue; }
        lastSamplerOutputs[i] = currentSamplerOutputs[i];
    }

    // Return mixed sample WITHOUT envelope multiplication
    // Envelope is now routed through modulation matrix to oscillator levels
    return mixedSample;
}
