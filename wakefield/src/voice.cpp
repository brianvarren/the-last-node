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

float Voice::generateSample() {
    if (!active) {
        envelopeValue = 0.0f;
        return 0.0f;
    }

    // Get envelope level and cache for modulation routing
    envelopeValue = envelope.process();

    // If envelope finished, deactivate voice and clear FM history
    if (!envelope.isActive()) {
        active = false;
        envelopeValue = 0.0f;
        resetFMHistory();
        return 0.0f;
    }

    // Determine FM input for each oscillator using previous outputs (1-sample delay)
    // FM matrix is 8x8: OSC1-4 are indices 0-3, SAMP1-4 are indices 4-7
    float fmInputs[OSCILLATORS_PER_VOICE] = {0.0f};
    if (params) {
        for (int target = 0; target < OSCILLATORS_PER_VOICE; ++target) {
            float totalFM = 0.0f;
            // Oscillator sources (0-3)
            for (int source = 0; source < OSCILLATORS_PER_VOICE; ++source) {
                float fmDepth = params->getFMDepth(target, source);
                if (fmDepth != 0.0f) {
                    totalFM += lastOscOutputs[source] * (fmDepth * 100.0f);
                }
            }
            // Sampler sources (4-7)
            for (int source = 0; source < SAMPLERS_PER_VOICE; ++source) {
                float fmDepth = params->getFMDepth(target, 4 + source);
                if (fmDepth != 0.0f) {
                    totalFM += lastSamplerOutputs[source] * (fmDepth * 100.0f);
                }
            }
            fmInputs[target] = totalFM;
        }
    }

    // Generate current oscillator outputs with FM and modulation applied
    float currentOutputs[OSCILLATORS_PER_VOICE];
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        currentOutputs[i] = oscillators[i].process(sampleRate, fmInputs[i],
                                                    pitchMod[i], morphMod[i], dutyMod[i],
                                                    ratioMod[i], offsetMod[i]);
    }

    // Check if any oscillators or samplers are solo'd
    bool anySolo = false;
    if (params) {
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            if (params->oscSolo[i].load()) {
                anySolo = true;
                break;
            }
        }
        if (!anySolo) {
            for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
                if (params->samplerSolo[i].load()) {
                    anySolo = true;
                    break;
                }
            }
        }
    }

    // Mix all oscillators with (amp + ampMod) × level × mute/solo
    // Amp is the modulation target, Level is the static mixer
    float mixedSample = 0.0f;
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        // Get base amp and level from synth (control-rate)
        float baseAmp = synth ? synth->getOscillatorBaseAmp(i) : 1.0f;
        float baseLevel = synth ? synth->getOscillatorBaseLevel(i) : 0.0f;

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

        // Mix with final gain
        mixedSample += currentOutputs[i] * finalGain;
    }

    // Determine FM input for each sampler using previous outputs (1-sample delay)
    float samplerFMInputs[SAMPLERS_PER_VOICE] = {0.0f};
    if (params) {
        for (int target = 0; target < SAMPLERS_PER_VOICE; ++target) {
            float totalFM = 0.0f;
            // Oscillator sources (0-3)
            for (int source = 0; source < OSCILLATORS_PER_VOICE; ++source) {
                float fmDepth = params->getFMDepth(4 + target, source);
                if (fmDepth != 0.0f) {
                    totalFM += lastOscOutputs[source] * (fmDepth * 100.0f);
                }
            }
            // Sampler sources (4-7)
            for (int source = 0; source < SAMPLERS_PER_VOICE; ++source) {
                float fmDepth = params->getFMDepth(4 + target, 4 + source);
                if (fmDepth != 0.0f) {
                    totalFM += lastSamplerOutputs[source] * (fmDepth * 100.0f);
                }
            }
            samplerFMInputs[target] = totalFM;
        }
    }

    // Process all samplers and mix
    float currentSamplerOutputs[SAMPLERS_PER_VOICE] = {0.0f};
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        if (!samplers[i].isKeyMode()) {
            currentSamplerOutputs[i] = 0.0f;
            continue;
        }

        // Process sampler with FM and modulation (pass MIDI note for KEY mode tracking)
        float samplerOut = samplers[i].process(sampleRate, samplerFMInputs[i],
                                              samplerPitchMod[i],
                                              samplerLoopStartMod[i],
                                              samplerLoopLengthMod[i],
                                              samplerCrossfadeMod[i],
                                              samplerLevelMod[i],
                                              note);
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

        mixedSample += samplerOut;
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
