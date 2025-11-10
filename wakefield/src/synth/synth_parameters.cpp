// ============================================================================
// SYNTH PARAMETERS - Parameter Setters and Configuration
// ============================================================================
//
// RESPONSIBILITIES:
// - Envelope parameter updates: updateEnvelopeParameters() for ADSR
// - Oscillator state management: setOscillatorState() for shape/frequency/level
// - Reverb configuration: updateReverbParameters()
// - Compressor configuration: updateCompressorParameters()
// - Filter configuration: updateFilterParameters() for all filter types
// - Modulation routing: normalizePhaseForDriver(), getModulationSource()
// - Modulation matrix: processModulationMatrix(), applyModCurve()
// - Sampler configuration: setSamplerSample/Loop/Speed/etc.
// - Chaos generator configuration: setChaosParameter/ClockFreq/etc.
//
// DOES NOT CONTAIN:
// - Audio processing (see synth_process.cpp)
// - Voice management (see synth_voices.cpp)
// - DSP implementation (see voice.cpp, oscillator.cpp, reverb.cpp, etc.)
//
// ============================================================================

#include "../synth.h"
#include "../ui.h"
#include "../clock.h"

void Synth::updateEnvelopeParameters(float attack, float decay, float sustain, float release) {
    // Update ENV1 with smoothed parameters and apply bends from params
    for (auto& voice : voices) {
        voice.envelopes[0].setAttack(attack);
        voice.envelopes[0].setDecay(decay);
        voice.envelopes[0].setSustain(sustain);
        voice.envelopes[0].setRelease(release);
        // Bends
        if (params) {
            voice.envelopes[0].setAttackBend(params->getEnvAttackBend(0));
            voice.envelopes[0].setReleaseBend(params->getEnvReleaseBend(0));
        }
    }

    // Update ENV2-ENV4 directly from params (unsmoothed)
    if (params) {
        for (auto& voice : voices) {
            for (int i = 1; i < 4; ++i) {
                voice.envelopes[i].setAttack(params->getEnvAttack(i));
                voice.envelopes[i].setDecay(params->getEnvDecay(i));
                voice.envelopes[i].setSustain(params->getEnvSustain(i));
                voice.envelopes[i].setRelease(params->getEnvRelease(i));
                voice.envelopes[i].setAttackBend(params->getEnvAttackBend(i));
                voice.envelopes[i].setReleaseBend(params->getEnvReleaseBend(i));
            }
        }
    }
}

void Synth::setOscillatorState(int index, int shapeIndex,
                               float baseFreq, float morph,
                               float ratio, float offsetHz, float amp, float level) {
    if (index < 0 || index >= OSCILLATORS_PER_VOICE) {
        return;
    }

    // No change detection - always update for real-time control
    // The oscillator setters are simple and cheap to call every frame

    BrainwaveMode resolvedMode = BrainwaveMode::KEY;
    if (params) {
        int noteSource = params->getOscNoteSource(index);
        if (noteSource == static_cast<int>(NoteSource::FREE)) {
            resolvedMode = BrainwaveMode::FREE;
        }
    }

    bool wasFree = oscillatorNoteSourceFree[index];
    bool isFree = (resolvedMode == BrainwaveMode::FREE);
    oscillatorNoteSourceFree[index] = isFree;

    for (auto& voice : voices) {
        BrainwaveOscillator& osc = voice.oscillators[index];
        osc.setMode(resolvedMode);
        osc.setShape(shapeIndex);
        osc.setFrequency(baseFreq);
        osc.setMorph(morph);
        osc.setRatio(ratio);
        osc.setOffset(offsetHz);
    }

    // Store base amp and level at control rate (used in voice mixing)
    oscillatorBaseAmps[index] = amp;
    oscillatorBaseLevels[index] = level;

    // Spawn/kill free-running voice when note source changes to/from "None"
    if (isFree && !wasFree) {
        spawnFreeRunningVoice(index);
    } else if (!isFree && wasFree) {
        killFreeRunningVoice(index);
    }
}

void Synth::updateReverbParameters(float delayTime, float size, float damping, float mix, float decay,
                                   float diffusion, float modDepth, float modFreq) {
    reverb.setDelayTime(delayTime);
    reverb.setSize(size);
    reverb.setDamping(damping);
    reverb.setMix(mix);
    reverb.setDecay(decay);
    reverb.setDiffusion(diffusion);
    reverb.setModDepth(modDepth);
    reverb.setModFreq(modFreq);
}

void Synth::updateCompressorParameters(float threshold, float ratio, float attack,
                                       float release, float knee, float mix,
                                       bool autoMakeup, float manualMakeup, bool rmsMode) {
    compressor.setThreshold(threshold);
    compressor.setRatio(ratio);
    compressor.setAttack(attack);
    compressor.setRelease(release);
    compressor.setKnee(knee);
    compressor.setMix(mix);
    compressor.setAutoMakeup(autoMakeup);
    compressor.setManualMakeup(manualMakeup);
    compressor.setDetectionMode(rmsMode);
}

float Synth::getCompressorGainReduction() const {
    return compressor.getGainReduction();
}

void Synth::updateFilterParameters(int type, float cutoff, float gain,
                                   float resonance, float drive, float feedbackHP,
                                   float spread, float notchFeedback, float bandWidth) {
    currentFilterType = type;
    
    // Update all filter types (the active one will be used during processing)
    filterL.setCutoff(cutoff);
    filterR.setCutoff(cutoff);
    
    highShelfL.setCutoff(cutoff);
    highShelfR.setCutoff(cutoff);
    highShelfL.setGainDb(gain);
    highShelfR.setGainDb(gain);
    
    lowShelfL.setCutoff(cutoff);
    lowShelfR.setCutoff(cutoff);
    lowShelfL.setGainDb(gain);
    lowShelfR.setGainDb(gain);

    ladderFilterL.setCutoff(cutoff);
    ladderFilterR.setCutoff(cutoff);
    ladderFilterL.setResonance(resonance);
    ladderFilterR.setResonance(resonance);
    ladderFilterL.setDrive(drive);
    ladderFilterR.setDrive(drive);
    ladderFilterL.setFeedbackHighpass(feedbackHP);
    ladderFilterR.setFeedbackHighpass(feedbackHP);

    diodeFilterL.setCutoff(cutoff);
    diodeFilterR.setCutoff(cutoff);
    diodeFilterL.setResonance(resonance);
    diodeFilterR.setResonance(resonance);
    diodeFilterL.setDrive(drive);
    diodeFilterR.setDrive(drive);
    diodeFilterL.setFeedbackHighpass(feedbackHP);
    diodeFilterR.setFeedbackHighpass(feedbackHP);

    bandpassFilterL.setCutoff(cutoff);
    bandpassFilterR.setCutoff(cutoff);
    bandpassFilterL.setResonance(resonance);
    bandpassFilterR.setResonance(resonance);
    bandpassFilterL.setDrive(drive);
    bandpassFilterR.setDrive(drive);
    bandpassFilterL.setFeedbackHighpass(feedbackHP);
    bandpassFilterR.setFeedbackHighpass(feedbackHP);
    bandpassFilterL.setWidth(bandWidth);
    bandpassFilterR.setWidth(bandWidth);

    bandpass8FilterL.setCutoff(cutoff);
    bandpass8FilterR.setCutoff(cutoff);
    bandpass8FilterL.setResonance(resonance);
    bandpass8FilterR.setResonance(resonance);
    bandpass8FilterL.setDrive(drive);
    bandpass8FilterR.setDrive(drive);
    bandpass8FilterL.setWidth(bandWidth);
    bandpass8FilterR.setWidth(bandWidth);

    bandpass2FilterL.setCutoff(cutoff);
    bandpass2FilterR.setCutoff(cutoff);
    bandpass2FilterL.setResonance(resonance);
    bandpass2FilterR.setResonance(resonance);
    bandpass2FilterL.setDrive(drive);
    bandpass2FilterR.setDrive(drive);
    bandpass2FilterL.setFeedbackHighpass(feedbackHP);
    bandpass2FilterR.setFeedbackHighpass(feedbackHP);
    bandpass2FilterL.setWidth(bandWidth);
    bandpass2FilterR.setWidth(bandWidth);

    notchFilterL.setCutoff(cutoff);
    notchFilterR.setCutoff(cutoff);
    notchFilterL.setSpread(spread);
    notchFilterR.setSpread(spread);
    notchFilterL.setResonance(resonance);
    notchFilterR.setResonance(resonance);
    notchFilterL.setDrive(drive);
    notchFilterR.setDrive(drive);
    notchFilterL.setNotchFeedback(notchFeedback);
    notchFilterR.setNotchFeedback(notchFeedback);
    // Note: DryWet is applied with modulation in processAudio, not here
}
float Synth::normalizePhaseForDriver(float value, int type) const {
    if (type == 0) {
        return std::clamp(value, 0.0f, 1.0f);
    }
    return std::clamp((value + 1.0f) * 0.5f, 0.0f, 1.0f);
}

float Synth::getModulationSource(int sourceIndex, const Voice* voiceContext) {
    // Source indices from ui_mod_data.cpp:
    // 0-3: LFO 1-4
    // 4-7: ENV 1-4
    // 8: MIDI Note, 9: Velocity, 10: Aftertouch, 11: Mod Wheel, 12: Pitch Bend
    // 13: Clock
    // 14-21: Chaos 1-4 X/Y pairs (14=C1X, 15=C1Y, 16=C2X, 17=C2Y, etc.)
    // 22-25: Track 1-4 Note
    // 26-29: Track 1-4 Velocity
    // 30-33: Track 1-4 Gate %
    // 34-37: Track 1-4 Probability

    if (sourceIndex >= 0 && sourceIndex <= 3) {
        // LFO 1-4
        return getLFOOutput(sourceIndex);
    } else if (sourceIndex >= 4 && sourceIndex <= 7) {
        // ENV 1-4 (per-voice)
        int envIdx = sourceIndex - 4; // 0..3
        if (params) {
            int activeEnv = std::clamp(params->activeEnvCount.load(), 1, 4);
            if (envIdx >= activeEnv) return 0.0f;
        }
        if (voiceContext) {
            return voiceContext->getEnvelopeValue(envIdx);
        }
        // Fallback: max of active voices for this envelope index
        float maxEnv = 0.0f;
        for (int i = MAX_VOICES - 1; i >= 0; --i) {
            if (voices[i].active) {
                float env = voices[i].getEnvelopeValue(envIdx);
                if (env > maxEnv) maxEnv = env;
            }
        }
        return maxEnv;
    } else if (sourceIndex == 8) {
        // MIDI Note (0-127 mapped to -1 to +1, with 64 as center)
        if (voiceContext && voiceContext->active) {
            return (voiceContext->note - 64.0f) / 64.0f;
        }
        // If no voice context, use most recent active voice
        for (int i = MAX_VOICES - 1; i >= 0; --i) {
            if (voices[i].active) {
                return (voices[i].note - 64.0f) / 64.0f;
            }
        }
        return 0.0f;
    } else if (sourceIndex == 9) {
        // Velocity (0-127 mapped to 0 to +1)
        if (voiceContext && voiceContext->active) {
            return voiceContext->velocity / 127.0f;
        }
        // If no voice context, use most recent active voice
        for (int i = MAX_VOICES - 1; i >= 0; --i) {
            if (voices[i].active) {
                return voices[i].velocity / 127.0f;
            }
        }
        return 0.0f;
    } else if (sourceIndex == 10) {
        // Aftertouch - TODO: implement
        return 0.0f;
    } else if (sourceIndex == 11) {
        // Mod Wheel - TODO: implement
        return 0.0f;
    } else if (sourceIndex == 12) {
        // Pitch Bend - TODO: implement
        return 0.0f;
    } else if (sourceIndex == 13) {
        // Clock
        if (clock) {
            float phase = static_cast<float>(clock->getPhase(Subdivision::SIXTEENTH));
            return phase * 2.0f - 1.0f;
        }
        return -1.0f;
    } else if (sourceIndex >= 14 && sourceIndex <= 21) {
        // Chaos 1-4 X/Y pairs
        int chaosIndex = (sourceIndex - 14) / 2;  // 14,15->0, 16,17->1, 18,19->2, 20,21->3
        bool isY = ((sourceIndex - 14) % 2) == 1;  // Odd indices are Y
        if (isY) {
            return getChaosOutputY(chaosIndex);
        } else {
            return getChaosOutput(chaosIndex);
        }
    } else if (sourceIndex >= 22 && sourceIndex <= 25) {
        // Track 1-4 Note (0-127 mapped to -1 to +1, with 64 as center)
        int trackIdx = sourceIndex - 22;  // 0..3
        if (trackState[trackIdx].active) {
            return (trackState[trackIdx].note - 64.0f) / 64.0f;
        }
        return 0.0f;
    } else if (sourceIndex >= 26 && sourceIndex <= 29) {
        // Track 1-4 Velocity (0-127 mapped to 0 to +1)
        int trackIdx = sourceIndex - 26;  // 0..3
        if (trackState[trackIdx].active) {
            return trackState[trackIdx].velocity / 127.0f;
        }
        return 0.0f;
    } else if (sourceIndex >= 30 && sourceIndex <= 33) {
        // Track 1-4 Gate % (0.0-1.0)
        int trackIdx = sourceIndex - 30;  // 0..3
        if (trackState[trackIdx].active) {
            return trackState[trackIdx].gatePercent * 2.0f - 1.0f;  // Map 0-1 to -1 to +1
        }
        return -1.0f;  // Default to -1 when inactive
    } else if (sourceIndex >= 34 && sourceIndex <= 37) {
        // Track 1-4 Probability (0.0-1.0)
        int trackIdx = sourceIndex - 34;  // 0..3
        return trackState[trackIdx].probability * 2.0f - 1.0f;  // Map 0-1 to -1 to +1
    }

    return 0.0f;
}

float Synth::applyModCurve(float input, int curveType) {
    // Curve types from ui_mod.cpp:
    // 0: Linear, 1: Exponential, 2: Logarithmic, 3: S-Curve

    switch (curveType) {
        case 0: // Linear
            return input;

        case 1: // Exponential
            if (input >= 0.0f) {
                return (std::exp(input) - 1.0f) / (std::exp(1.0f) - 1.0f);
            } else {
                return -(std::exp(-input) - 1.0f) / (std::exp(1.0f) - 1.0f);
            }

        case 2: // Logarithmic
            if (input >= 0.0f) {
                return std::log(1.0f + input) / std::log(2.0f);
            } else {
                return -std::log(1.0f - input) / std::log(2.0f);
            }

        case 3: // S-Curve (tanh)
            return std::tanh(input * 2.0f);

        default:
            return input;
    }
}

Synth::ModulationOutputs Synth::processModulationMatrix(const Voice* voiceContext) {
    ModulationOutputs outputs;

    if (!ui) return outputs;

    // Process all 16 modulation slots
    for (int i = 0; i < 16; ++i) {
        const ModulationSlot& slot = ui->modulationSlots[i];

        // Skip empty or incomplete slots
        if (!slot.isComplete()) continue;

        // Get source value (-1 to +1)
        float sourceValue = getModulationSource(slot.source, voiceContext);

        // Apply curve shaping
        float shapedValue = applyModCurve(sourceValue, slot.curve);

        // Amount scaling (-99..+99 -> -1..+1 approx)
        float amount = static_cast<float>(slot.amount) / 99.0f;
        float modValue;

        // Type 0 = Unidirectional (-->) maps source -1..+1 to 0..1, then scales by amount
        // Type 1 = Bidirectional (<->) scales -1..+1 directly by amount
        if (slot.type == 0) {
            if (slot.source >= 4 && slot.source <= 7) {
                float uniSource = std::clamp(shapedValue, 0.0f, 1.0f);
                modValue = uniSource * amount;
            } else {
                float mapped01 = (shapedValue + 1.0f) * 0.5f;  // 0..1
                modValue = mapped01 * amount;
            }
        } else {
            modValue = shapedValue * amount;
        }

        int destination = slot.destination;

        // Apply to destination
        // Destination indices from ui_mod_data:
        // 0-4: OSC 1 Pitch/Morph/Ratio/Offset/Amp
        // 5-9: OSC 2 Pitch/Morph/Ratio/Offset/Amp
        // 10-14: OSC 3 Pitch/Morph/Ratio/Offset/Amp
        // 15-19: OSC 4 Pitch/Morph/Ratio/Offset/Amp
        // 20-26: Filter Cutoff/Resonance/Drive/Width/NotchFeedback/Spread/DryWet
        // 27-28: Reverb Mix/Size
        // 29-48: SAMP 1-4 Pitch/LoopStart/LoopLength/Crossfade/Level
        // 49-60: LFO 1-4 Rate/Morph/Duty
        // 61: Mixer Master Volume
        // 62-65: Mixer Oscillator Levels
        // 66-69: Mixer Sampler Levels
        // 70-73: Sequencer Track 1-4 Phase Drivers
        // 74-77: Sampler 1-4 Phase Drivers
        // 78: FM Global Depth
        // 83- (83 + kFMTargetCount*kFMSourceCount - 1): Individual FM depths
        // Remaining: Chaos parameters (Clock/U/Level)

        const int fmGlobalIndex = 78;
        const int fmCellStart = fmGlobalIndex + 1;
        const int fmCellEnd = fmCellStart + kFMTargetCount * kFMSourceCount;
        const int chaosBase = fmCellEnd;

        if (destination == fmGlobalIndex) {
            outputs.fmGlobalDepth += modValue;
            continue;
        }

        if (destination >= fmCellStart && destination < fmCellEnd) {
            // Skip per-voice sources (ENV1-4) in global pass
            if (!voiceContext && slot.source >= 4 && slot.source <= 7) {
                continue;
            }
            int cellIndex = destination - fmCellStart;
            int target = cellIndex / kFMSourceCount;
            int source = cellIndex % kFMSourceCount;
            outputs.fmDepth[target][source] += modValue;
            continue;
        }

        if (destination >= chaosBase) {
            int chaosIndex = (destination - chaosBase) / 3;
            int chaosParam = (destination - chaosBase) % 3;
            if (chaosIndex >= 0 && chaosIndex < 4) {
                switch (chaosParam) {
                    case 0: outputs.chaosClockFreq[chaosIndex] += modValue; break;
                    case 1: outputs.chaosParameter[chaosIndex] += modValue; break;
                    case 2: outputs.chaosLevel[chaosIndex] += modValue; break;
                }
                continue;
            }
        }

        auto adjustAmpMod = [&](float value) -> float {
            if (slot.type == 1 && slot.source >= 4 && slot.source <= 7 && amount > 0.0f) {
                return value - amount;
            }
            return value;
        };

        switch (destination) {
            // OSC 1
            case 0: outputs.osc1Pitch += modValue; break;
            case 1: outputs.osc1Morph += modValue; break;
            case 2: outputs.osc1Ratio += modValue; break;
            case 3: outputs.osc1Offset += modValue; break;
            case 4:
                if (slot.source >= 4 && slot.source <= 7) {
                    if (!voiceContext) continue;
                    outputs.oscAmpControllerActive[0] = true;
                    outputs.oscAmpController[0] = std::clamp(modValue, 0.0f, 1.0f);
                    continue;
                }
                outputs.osc1Amp += adjustAmpMod(modValue);
                break;
            // OSC 2
            case 5: outputs.osc2Pitch += modValue; break;
            case 6: outputs.osc2Morph += modValue; break;
            case 7: outputs.osc2Ratio += modValue; break;
            case 8: outputs.osc2Offset += modValue; break;
            case 9:
                if (slot.source >= 4 && slot.source <= 7) {
                    if (!voiceContext) continue;
                    outputs.oscAmpControllerActive[1] = true;
                    outputs.oscAmpController[1] = std::clamp(modValue, 0.0f, 1.0f);
                    continue;
                }
                outputs.osc2Amp += adjustAmpMod(modValue);
                break;
            // OSC 3
            case 10: outputs.osc3Pitch += modValue; break;
            case 11: outputs.osc3Morph += modValue; break;
            case 12: outputs.osc3Ratio += modValue; break;
            case 13: outputs.osc3Offset += modValue; break;
            case 14:
                if (slot.source >= 4 && slot.source <= 7) {
                    if (!voiceContext) continue;
                    outputs.oscAmpControllerActive[2] = true;
                    outputs.oscAmpController[2] = std::clamp(modValue, 0.0f, 1.0f);
                    continue;
                }
                outputs.osc3Amp += adjustAmpMod(modValue);
                break;
            // OSC 4
            case 15: outputs.osc4Pitch += modValue; break;
            case 16: outputs.osc4Morph += modValue; break;
            case 17: outputs.osc4Ratio += modValue; break;
            case 18: outputs.osc4Offset += modValue; break;
            case 19:
                if (slot.source >= 4 && slot.source <= 7) {
                    if (!voiceContext) continue;
                    outputs.oscAmpControllerActive[3] = true;
                    outputs.oscAmpController[3] = std::clamp(modValue, 0.0f, 1.0f);
                    continue;
                }
                outputs.osc4Amp += adjustAmpMod(modValue);
                break;
            // Filter
            case 20: outputs.filterCutoff += modValue; break;
            case 21: outputs.filterResonance += modValue; break;
            case 22: outputs.filterDrive += modValue; break;
            case 23: outputs.filterWidth += modValue; break;
            case 24: outputs.filterNotchFeedback += modValue; break;
            case 25: outputs.filterSpread += modValue; break;
            case 26: outputs.filterDryWet += modValue; break;
            // Reverb
            case 27: outputs.reverbMix += modValue; break;
            case 28: outputs.reverbSize += modValue; break;
            // SAMP 1
            case 29: outputs.samp1Pitch += modValue; break;
            case 30: outputs.samp1LoopStart += modValue; break;
            case 31: outputs.samp1LoopLength += modValue; break;
            case 32: outputs.samp1Crossfade += modValue; break;
            case 33:
                if (slot.source >= 4 && slot.source <= 7) {
                    if (!voiceContext) continue;
                    outputs.samplerAmpControllerActive[0] = true;
                    outputs.samplerAmpController[0] = std::clamp(modValue, 0.0f, 1.0f);
                    continue;
                }
                outputs.samp1Amp += adjustAmpMod(modValue);
                break;
            // SAMP 2
            case 34: outputs.samp2Pitch += modValue; break;
            case 35: outputs.samp2LoopStart += modValue; break;
            case 36: outputs.samp2LoopLength += modValue; break;
            case 37: outputs.samp2Crossfade += modValue; break;
            case 38:
                if (slot.source >= 4 && slot.source <= 7) {
                    if (!voiceContext) continue;
                    outputs.samplerAmpControllerActive[1] = true;
                    outputs.samplerAmpController[1] = std::clamp(modValue, 0.0f, 1.0f);
                    continue;
                }
                outputs.samp2Amp += adjustAmpMod(modValue);
                break;
            // SAMP 3
            case 39: outputs.samp3Pitch += modValue; break;
            case 40: outputs.samp3LoopStart += modValue; break;
            case 41: outputs.samp3LoopLength += modValue; break;
            case 42: outputs.samp3Crossfade += modValue; break;
            case 43:
                if (slot.source >= 4 && slot.source <= 7) {
                    if (!voiceContext) continue;
                    outputs.samplerAmpControllerActive[2] = true;
                    outputs.samplerAmpController[2] = std::clamp(modValue, 0.0f, 1.0f);
                    continue;
                }
                outputs.samp3Amp += adjustAmpMod(modValue);
                break;
            // SAMP 4
            case 44: outputs.samp4Pitch += modValue; break;
            case 45: outputs.samp4LoopStart += modValue; break;
            case 46: outputs.samp4LoopLength += modValue; break;
            case 47: outputs.samp4Crossfade += modValue; break;
            case 48:
                if (slot.source >= 4 && slot.source <= 7) {
                    if (!voiceContext) continue;
                    outputs.samplerAmpControllerActive[3] = true;
                    outputs.samplerAmpController[3] = std::clamp(modValue, 0.0f, 1.0f);
                    continue;
                }
                outputs.samp4Amp += adjustAmpMod(modValue);
                break;
            // LFO 1
            case 49: outputs.lfoPeriod[0] += modValue; break;
            case 50: outputs.lfoMorph[0] += modValue; break;
            case 51: outputs.lfoDuty[0] += modValue; break;
            // LFO 2
            case 52: outputs.lfoPeriod[1] += modValue; break;
            case 53: outputs.lfoMorph[1] += modValue; break;
            case 54: outputs.lfoDuty[1] += modValue; break;
            // LFO 3
            case 55: outputs.lfoPeriod[2] += modValue; break;
            case 56: outputs.lfoMorph[2] += modValue; break;
            case 57: outputs.lfoDuty[2] += modValue; break;
            // LFO 4
            case 58: outputs.lfoPeriod[3] += modValue; break;
            case 59: outputs.lfoMorph[3] += modValue; break;
            case 60: outputs.lfoDuty[3] += modValue; break;
            // Mixer
            case 61: outputs.mixerMasterVolume += modValue; break;
            case 62: outputs.mixerOscLevel[0] += modValue; break;
            case 63: outputs.mixerOscLevel[1] += modValue; break;
            case 64: outputs.mixerOscLevel[2] += modValue; break;
            case 65: outputs.mixerOscLevel[3] += modValue; break;
            case 66: outputs.mixerSamplerLevel[0] += modValue; break;
            case 67: outputs.mixerSamplerLevel[1] += modValue; break;
            case 68: outputs.mixerSamplerLevel[2] += modValue; break;
            case 69: outputs.mixerSamplerLevel[3] += modValue; break;
            // Sequencer phase drivers
            case 70: outputs.sequencerPhase[0] += modValue; break;
            case 71: outputs.sequencerPhase[1] += modValue; break;
            case 72: outputs.sequencerPhase[2] += modValue; break;
            case 73: outputs.sequencerPhase[3] += modValue; break;
            // Sampler phase drivers
            case 74: outputs.samplerPhase[0] += modValue; break;
            case 75: outputs.samplerPhase[1] += modValue; break;
            case 76: outputs.samplerPhase[2] += modValue; break;
            case 77: outputs.samplerPhase[3] += modValue; break;
        }
    }

    return outputs;
}

// ── Sampler Control Methods ─────────────────────────────────────────────────

void Synth::setSamplerSample(int samplerIndex, int sampleIndex) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    const SampleData* sample = sampleBank.getSample(sampleIndex);
    currentSampleIndices[samplerIndex] = sampleIndex;

    // Apply to all voices
    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setSample(sample);
    }
}

void Synth::setSamplerLoopStart(int samplerIndex, float normalized) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setLoopStart(normalized);
    }
}

void Synth::setSamplerLoopLength(int samplerIndex, float normalized) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setLoopLength(normalized);
    }
}

void Synth::setSamplerCrossfadeLength(int samplerIndex, float normalized) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setCrossfadeLength(normalized);
    }
}

void Synth::setSamplerPlaybackSpeed(int samplerIndex, float speed) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setPlaybackSpeed(speed);
    }
}

void Synth::setSamplerOctave(int samplerIndex, int octave) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    // Clamp octave to -5 to +5
    samplerOctaves[samplerIndex] = std::max(-5, std::min(5, octave));

    // Recalculate and update playback speed
    // Formula: 2^(octave + tune * 0.5)
    float speed = std::pow(2.0f, samplerOctaves[samplerIndex] + samplerTunes[samplerIndex] * 0.5f);
    setSamplerPlaybackSpeed(samplerIndex, speed);
}

void Synth::setSamplerTune(int samplerIndex, float tune) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    // Clamp tune to -1.0 to +1.0
    samplerTunes[samplerIndex] = std::max(-1.0f, std::min(1.0f, tune));

    // Recalculate and update playback speed
    // Formula: 2^(octave + tune * 0.5)
    float speed = std::pow(2.0f, samplerOctaves[samplerIndex] + samplerTunes[samplerIndex] * 0.5f);
    setSamplerPlaybackSpeed(samplerIndex, speed);
}

void Synth::setSamplerSyncMode(int samplerIndex, int mode) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    // Clamp sync mode to 0-3 (Off, On, Trip, Dot)
    samplerSyncModes[samplerIndex] = std::max(0, std::min(3, mode));

    // Tempo sync is implemented in sampler's calculateLoopBoundaries
}

void Synth::setSamplerNoteReset(int samplerIndex, bool enabled) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    samplerNoteResets[samplerIndex] = enabled;

    // TODO: Implement note reset functionality
}

void Synth::setSamplerTZFMDepth(int samplerIndex, float depth) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setTZFMDepth(depth);
    }
}

void Synth::setSamplerPlaybackMode(int samplerIndex, PlaybackMode mode) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setPlaybackMode(mode);
    }
}

void Synth::setSamplerLevel(int samplerIndex, float level) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setLevel(level);
    }
}

void Synth::setSamplerKeyMode(int samplerIndex, bool enabled) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    samplerKeyModes[samplerIndex] = enabled;

    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setKeyMode(enabled);
        voice.samplers[samplerIndex].stopPlayback();
    }
}

void Synth::saveSamplerPhase(int samplerIndex, uint64_t phase) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }
    // Only save if Note Reset is OFF (so phase persists across notes)
    if (!samplerNoteResets[samplerIndex]) {
        std::lock_guard<std::mutex> lock(samplerPhaseMutex);
        samplerLastPhases[samplerIndex] = phase;
    }
}

// Get sampler state (from first voice as they're all synced)
int Synth::getSamplerSampleIndex(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return -1;
    }
    return currentSampleIndices[samplerIndex];
}

float Synth::getSamplerLoopStart(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE || voices.empty()) {
        return 0.0f;
    }
    return voices[0].samplers[samplerIndex].getLoopStart();
}

float Synth::getSamplerLoopLength(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE || voices.empty()) {
        return 1.0f;
    }
    return voices[0].samplers[samplerIndex].getLoopLength();
}

float Synth::getSamplerCrossfadeLength(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE || voices.empty()) {
        return 0.1f;
    }
    return voices[0].samplers[samplerIndex].getCrossfadeLength();
}

float Synth::getSamplerPlaybackSpeed(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE || voices.empty()) {
        return 1.0f;
    }
    return voices[0].samplers[samplerIndex].getPlaybackSpeed();
}

float Synth::getSamplerTZFMDepth(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE || voices.empty()) {
        return 0.0f;
    }
    return voices[0].samplers[samplerIndex].getTZFMDepth();
}

PlaybackMode Synth::getSamplerPlaybackMode(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE || voices.empty()) {
        return PlaybackMode::FORWARD;
    }
    return voices[0].samplers[samplerIndex].getPlaybackMode();
}

float Synth::getSamplerLevel(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE || voices.empty()) {
        return 1.0f;
    }
    return voices[0].samplers[samplerIndex].getLevel();
}

bool Synth::getSamplerKeyMode(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return true;
    }
    return samplerKeyModes[samplerIndex];
}

int Synth::getSamplerOctave(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return 0;
    }
    return samplerOctaves[samplerIndex];
}

void Synth::setOscillatorOctave(int oscIndex, int octave) {
    if (oscIndex < 0 || oscIndex >= OSCILLATORS_PER_VOICE) {
        return;
    }
    // Clamp octave to -5 to +5 (same range as samplers)
    oscillatorOctaves[oscIndex] = std::max(-5, std::min(5, octave));
}

int Synth::getOscillatorOctave(int oscIndex) const {
    if (oscIndex < 0 || oscIndex >= OSCILLATORS_PER_VOICE) {
        return 0;
    }
    return oscillatorOctaves[oscIndex];
}

float Synth::getSamplerTune(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return 0.0f;
    }
    return samplerTunes[samplerIndex];
}

int Synth::getSamplerSyncMode(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return 0;
    }
    return samplerSyncModes[samplerIndex];
}

bool Synth::getSamplerNoteReset(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return true;
    }
    return samplerNoteResets[samplerIndex];
}

float Synth::getModulatedOscLevel(int index) const {
    if (index < 0 || index >= OSCILLATORS_PER_VOICE) {
        return 0.0f;
    }
    float baseLevel = oscillatorBaseLevels[index];
    float offset = lastGlobalModOutputs.mixerOscLevel[index];
    return std::clamp(baseLevel + offset, 0.0f, 1.0f);
}

float Synth::getMixerSamplerLevelMod(int index) const {
    if (index < 0 || index >= SAMPLERS_PER_VOICE) {
        return 0.0f;
    }
    return lastGlobalModOutputs.mixerSamplerLevel[index];
}

float Synth::getMixerOscLevelMod(int index) const {
    if (index < 0 || index >= OSCILLATORS_PER_VOICE) {
        return 0.0f;
    }
    return lastGlobalModOutputs.mixerOscLevel[index];
}

float Synth::getFMDepthMod(int target, int source) const {
    if (target < 0 || target >= kFMTargetCount ||
        source < 0 || source >= kFMSourceCount) {
        return 0.0f;
    }
    return lastGlobalModOutputs.fmDepth[target][source];
}

// Chaos generator control methods
void Synth::setChaosParameter(int chaosIndex, float value) {
    if (chaosIndex < 0 || chaosIndex >= 4) return;
    chaos[chaosIndex].setChaosParameter(value);
}

void Synth::setChaosClockFreq(int chaosIndex, float freq) {
    if (chaosIndex < 0 || chaosIndex >= 4) return;
    chaos[chaosIndex].setClockFrequency(freq);
    // Auto-enable FAST mode at high rates so chaos runs per-sample
    if (freq >= (sampleRate * 0.5f)) {
        chaos[chaosIndex].setFastMode(true);
    }
}

void Synth::setChaosFastMode(int chaosIndex, bool fast) {
    if (chaosIndex < 0 || chaosIndex >= 4) return;
    chaos[chaosIndex].setFastMode(fast);
}

void Synth::setChaosInterpMode(int chaosIndex, int mode) {
    if (chaosIndex < 0 || chaosIndex >= 4) return;
    chaos[chaosIndex].setInterpMode(mode);
}

void Synth::resetChaosGenerator(int chaosIndex) {
    if (chaosIndex < 0 || chaosIndex >= 4) return;
    chaos[chaosIndex].reset();
}

