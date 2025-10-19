#include "synth.h"
#include "ui.h"
#include <iostream>

Synth::Synth(float sampleRate)
    : sampleRate(sampleRate)
    , masterVolume(0.5f)
    , reverbEnabled(false)
    , filterEnabled(false)
    , currentFilterType(0)
    , ui(nullptr)
    , params(nullptr)
    , reverb(sampleRate)
    , filterL(sampleRate)
    , filterR(sampleRate) {
    
    // Initialize shelf filters
    highShelfL.setSampleRate(sampleRate);
    highShelfR.setSampleRate(sampleRate);
    lowShelfL.setSampleRate(sampleRate);
    lowShelfR.setSampleRate(sampleRate);
    
    // Initialize voices with sample rate
    for (int i = 0; i < MAX_VOICES; ++i) {
        voices.emplace_back(sampleRate);
    }
}

float Synth::midiNoteToFrequency(int midiNote) {
    // MIDI note 69 = A4 = 440 Hz
    // Formula: f = 440 * 2^((n-69)/12)
    return 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
}

int Synth::findFreeVoice() {
    // Find first inactive voice
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!voices[i].active) {
            return i;
        }
    }
    
    // No free voice found
    return -1;
}

void Synth::updateEnvelopeParameters(float attack, float decay, float sustain, float release) {
    // Update all voice envelopes with new parameters
    for (auto& voice : voices) {
        voice.envelope.setAttack(attack);
        voice.envelope.setDecay(decay);
        voice.envelope.setSustain(sustain);
        voice.envelope.setRelease(release);
    }
}

void Synth::updateBrainwaveParameters(BrainwaveMode mode, int shape, float freq, float morph, float duty,
                                      float ratio, float offsetHz,
                                      bool flipPolarity, float level) {
    // Update all voice oscillators with new parameters
    // For now, update all 4 oscillators in each voice the same way
    // TODO: Make this per-oscillator once UI supports it
    BrainwaveShape shapeEnum = (shape == 0) ? BrainwaveShape::SAW : BrainwaveShape::PULSE;

    for (auto& voice : voices) {
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            voice.oscillators[i].setMode(mode);
            voice.oscillators[i].setShape(shapeEnum);
            voice.oscillators[i].setFrequency(freq);
            voice.oscillators[i].setMorph(morph);
            voice.oscillators[i].setDuty(duty);
            voice.oscillators[i].setRatio(ratio);
            voice.oscillators[i].setOffset(offsetHz);
            voice.oscillators[i].setFlipPolarity(flipPolarity);
            voice.oscillators[i].setLevel(level);
        }
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

void Synth::updateFilterParameters(int type, float cutoff, float gain) {
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
}

void Synth::noteOn(int midiNote, int velocity) {
    // Find a free voice
    int voiceIndex = findFreeVoice();

    if (voiceIndex == -1) {
        // No free voices - drop the note for now
        return;
    }

    // Activate the voice
    Voice& voice = voices[voiceIndex];
    voice.active = true;
    voice.note = midiNote;
    voice.velocity = velocity;

    float frequency = midiNoteToFrequency(midiNote);
    // Set note frequency for KEY mode for all oscillators
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        voice.oscillators[i].setNoteFrequency(frequency);
        voice.oscillators[i].reset();  // Reset phase for new note
    }
    voice.resetFMHistory();

    voice.envelope.noteOn();  // Trigger envelope attack
}

void Synth::noteOff(int midiNote) {
    // Find all voices playing this note and trigger their release
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].active && voices[i].note == midiNote) {
            voices[i].envelope.noteOff();  // Trigger release
        }
    }
}

int Synth::getActiveVoiceCount() const {
    int count = 0;
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].active) {
            count++;
        }
    }
    return count;
}

void Synth::process(float* output, unsigned int nFrames, unsigned int nChannels) {
    // Clear the output buffer first
    for (unsigned int i = 0; i < nFrames * nChannels; ++i) {
        output[i] = 0.0f;
    }

    // Process modulation matrix once per buffer
    ModulationOutputs modOutputs = processModulationMatrix();

    // Copy modulation values to active voices (used per-sample by oscillators)
    for (int v = 0; v < MAX_VOICES; ++v) {
        if (!voices[v].active) {
            continue;
        }

        Voice& voice = voices[v];

        // Set modulation values for all oscillators (in octaves for pitch)
        voice.pitchMod[0] = modOutputs.osc1Pitch;
        voice.pitchMod[1] = modOutputs.osc2Pitch;
        voice.pitchMod[2] = modOutputs.osc3Pitch;
        voice.pitchMod[3] = modOutputs.osc4Pitch;

        voice.morphMod[0] = modOutputs.osc1Morph;
        voice.morphMod[1] = modOutputs.osc2Morph;
        voice.morphMod[2] = modOutputs.osc3Morph;
        voice.morphMod[3] = modOutputs.osc4Morph;

        voice.dutyMod[0] = modOutputs.osc1Duty;
        voice.dutyMod[1] = modOutputs.osc2Duty;
        voice.dutyMod[2] = modOutputs.osc3Duty;
        voice.dutyMod[3] = modOutputs.osc4Duty;

        voice.ratioMod[0] = modOutputs.osc1Ratio;
        voice.ratioMod[1] = modOutputs.osc2Ratio;
        voice.ratioMod[2] = modOutputs.osc3Ratio;
        voice.ratioMod[3] = modOutputs.osc4Ratio;

        voice.offsetMod[0] = modOutputs.osc1Offset;
        voice.offsetMod[1] = modOutputs.osc2Offset;
        voice.offsetMod[2] = modOutputs.osc3Offset;
        voice.offsetMod[3] = modOutputs.osc4Offset;

        voice.levelMod[0] = modOutputs.osc1Level;
        voice.levelMod[1] = modOutputs.osc2Level;
        voice.levelMod[2] = modOutputs.osc3Level;
        voice.levelMod[3] = modOutputs.osc4Level;
    }

    // Process each active voice and mix into output
    for (int v = 0; v < MAX_VOICES; ++v) {
        if (!voices[v].active) {
            continue;
        }

        // Generate samples for this voice
        for (unsigned int i = 0; i < nFrames; ++i) {
            float sample = voices[v].generateSample();
            
            // Write to UI oscilloscope buffer if this is the first active voice
            if (v == 0 && ui) {
                ui->writeToWaveformBuffer(sample);
            }

            // Mix into all channels with master volume
            // Scale by 0.5 to prevent clipping when multiple voices play
            for (unsigned int ch = 0; ch < nChannels; ++ch) {
                output[i * nChannels + ch] += sample * 0.5f * masterVolume;
            }
        }
    }
    
    // Apply filter if enabled (stereo processing)
    if (filterEnabled && nChannels == 2) {
        for (unsigned int i = 0; i < nFrames; ++i) {
            float left = output[i * 2];
            float right = output[i * 2 + 1];
            
            // Apply selected filter type
            if (currentFilterType == 0) {  // Lowpass
                auto [lp_l, hp_l] = filterL.process(left);
                auto [lp_r, hp_r] = filterR.process(right);
                output[i * 2] = lp_l;
                output[i * 2 + 1] = lp_r;
            } else if (currentFilterType == 1) {  // Highpass
                auto [lp_l, hp_l] = filterL.process(left);
                auto [lp_r, hp_r] = filterR.process(right);
                output[i * 2] = hp_l;
                output[i * 2 + 1] = hp_r;
            } else if (currentFilterType == 2) {  // High shelf
                output[i * 2] = highShelfL.process(left);
                output[i * 2 + 1] = highShelfR.process(right);
            } else if (currentFilterType == 3) {  // Low shelf
                output[i * 2] = lowShelfL.process(left);
                output[i * 2 + 1] = lowShelfR.process(right);
            }
        }
    }
    
    // Apply reverb if enabled (stereo processing)
    if (reverbEnabled && nChannels == 2) {
        // Create temporary buffers for left and right channels
        std::vector<float> leftChannel(nFrames);
        std::vector<float> rightChannel(nFrames);
        
        // De-interleave
        for (unsigned int i = 0; i < nFrames; ++i) {
            leftChannel[i] = output[i * 2];
            rightChannel[i] = output[i * 2 + 1];
        }
        
        // Process reverb
        reverb.process(leftChannel.data(), rightChannel.data(), nFrames);
        
        // Re-interleave
        for (unsigned int i = 0; i < nFrames; ++i) {
            output[i * 2] = leftChannel[i];
            output[i * 2 + 1] = rightChannel[i];
        }
    }
}

void Synth::updateLFOParameters(int lfoIndex, float period, int syncMode, int shape, float morph,
                                 float duty, bool flip, bool resetOnNote, float tempo) {
    if (lfoIndex < 0 || lfoIndex >= 4) return;

    lfos[lfoIndex].setPeriod(period);
    lfos[lfoIndex].setSyncMode(static_cast<LFOSyncMode>(syncMode));
    lfos[lfoIndex].setShape(shape);
    lfos[lfoIndex].setMorph(morph);
    lfos[lfoIndex].setDuty(duty);
    lfos[lfoIndex].setFlip(flip);
    lfos[lfoIndex].setResetOnNote(resetOnNote);
    lfos[lfoIndex].setTempo(tempo);
}

void Synth::processLFOs(float sampleRate) {
    // Process all 4 LFOs once per audio buffer
    for (int i = 0; i < 4; ++i) {
        float value = lfos[i].process(sampleRate);
        if (params) {
            params->setLfoVisualState(i, value, lfos[i].getPhase());
        }
        // Write to UI's LFO history buffer for rolling scope view
        if (ui) {
            ui->writeToLFOHistory(i, value);
        }
    }
}

float Synth::getLFOOutput(int lfoIndex) const {
    if (lfoIndex < 0 || lfoIndex >= 4) return 0.0f;
    return lfos[lfoIndex].getCurrentValue();
}

float Synth::getModulationSource(int sourceIndex) {
    // Source indices from ui_mod.cpp:
    // 0-3: LFO 1-4
    // 4-7: ENV 1-4
    // 8: Velocity, 9: Aftertouch, 10: Mod Wheel, 11: Pitch Bend

    if (sourceIndex >= 0 && sourceIndex <= 3) {
        // LFO 1-4
        return getLFOOutput(sourceIndex);
    } else if (sourceIndex >= 4 && sourceIndex <= 7) {
        // ENV 1-4 - TODO: implement envelope outputs
        return 0.0f;
    } else if (sourceIndex == 8) {
        // Velocity - TODO: implement
        return 0.0f;
    } else if (sourceIndex == 9) {
        // Aftertouch - TODO: implement
        return 0.0f;
    } else if (sourceIndex == 10) {
        // Mod Wheel - TODO: implement
        return 0.0f;
    } else if (sourceIndex == 11) {
        // Pitch Bend - TODO: implement
        return 0.0f;
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

Synth::ModulationOutputs Synth::processModulationMatrix() {
    ModulationOutputs outputs;

    if (!ui) return outputs;

    // Process all 16 modulation slots
    for (int i = 0; i < 16; ++i) {
        const ModulationSlot& slot = ui->modulationSlots[i];

        // Skip empty or incomplete slots
        if (!slot.isComplete()) continue;

        // Get source value (-1 to +1)
        float sourceValue = getModulationSource(slot.source);

        // Apply curve shaping
        float shapedValue = applyModCurve(sourceValue, slot.curve);

        // Apply amount scaling (-99 to +99 maps to a reasonable modulation range)
        float amount = static_cast<float>(slot.amount) / 99.0f;
        float modValue = shapedValue * amount;

        // Handle bidirectional vs unidirectional
        // Type 0 = Unidirectional (-->), Type 1 = Bidirectional (<->)
        if (slot.type == 0) {
            // Unidirectional: map -1..+1 to 0..+1
            modValue = (modValue + 1.0f) * 0.5f * amount;
        }

        // Apply to destination
        // Destination indices from ui_mod.cpp:
        // 0-5: OSC 1 Pitch/Morph/Duty/Ratio/Offset/Level
        // 6-11: OSC 2 Pitch/Morph/Duty/Ratio/Offset/Level
        // 12-17: OSC 3 Pitch/Morph/Duty/Ratio/Offset/Level
        // 18-23: OSC 4 Pitch/Morph/Duty/Ratio/Offset/Level
        // 24-25: Filter Cutoff/Resonance
        // 26-27: Reverb Mix/Size

        switch (slot.destination) {
            // OSC 1
            case 0: outputs.osc1Pitch += modValue; break;
            case 1: outputs.osc1Morph += modValue; break;
            case 2: outputs.osc1Duty += modValue; break;
            case 3: outputs.osc1Ratio += modValue; break;
            case 4: outputs.osc1Offset += modValue; break;
            case 5: outputs.osc1Level += modValue; break;
            // OSC 2
            case 6: outputs.osc2Pitch += modValue; break;
            case 7: outputs.osc2Morph += modValue; break;
            case 8: outputs.osc2Duty += modValue; break;
            case 9: outputs.osc2Ratio += modValue; break;
            case 10: outputs.osc2Offset += modValue; break;
            case 11: outputs.osc2Level += modValue; break;
            // OSC 3
            case 12: outputs.osc3Pitch += modValue; break;
            case 13: outputs.osc3Morph += modValue; break;
            case 14: outputs.osc3Duty += modValue; break;
            case 15: outputs.osc3Ratio += modValue; break;
            case 16: outputs.osc3Offset += modValue; break;
            case 17: outputs.osc3Level += modValue; break;
            // OSC 4
            case 18: outputs.osc4Pitch += modValue; break;
            case 19: outputs.osc4Morph += modValue; break;
            case 20: outputs.osc4Duty += modValue; break;
            case 21: outputs.osc4Ratio += modValue; break;
            case 22: outputs.osc4Offset += modValue; break;
            case 23: outputs.osc4Level += modValue; break;
            // Filter
            case 24: outputs.filterCutoff += modValue; break;
            case 25: outputs.filterResonance += modValue; break;
            // Reverb
            case 26: outputs.reverbMix += modValue; break;
            case 27: outputs.reverbSize += modValue; break;
        }
    }

    return outputs;
}

void Synth::setParams(SynthParameters* params_ptr) {
    params = params_ptr;
    // Update all voice param pointers for FM matrix access
    for (auto& voice : voices) {
        voice.params = params_ptr;
    }
}
