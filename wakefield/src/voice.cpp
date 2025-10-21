#include "voice.h"
#include "synth.h"  // For Synth::getOscillatorBaseLevel()
#include "ui.h"    // For SynthParameters definition

void Voice::resetFMHistory() {
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        lastOscOutputs[i] = 0.0f;
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
    float fmInputs[OSCILLATORS_PER_VOICE] = {0.0f};
    if (params) {
        for (int target = 0; target < OSCILLATORS_PER_VOICE; ++target) {
            float totalFM = 0.0f;
            for (int source = 0; source < OSCILLATORS_PER_VOICE; ++source) {
                float fmDepth = params->getFMDepth(target, source);
                if (fmDepth != 0.0f) {
                    totalFM += lastOscOutputs[source] * (fmDepth * 100.0f);
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

    // Mix all oscillators with base level + modulation
    // Simple additive mixing (not weighted average)
    float mixedSample = 0.0f;
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        // Get base level from synth (control-rate) and add level modulation
        float baseLevel = synth ? synth->getOscillatorBaseLevel(i) : 0.0f;
        float modulatedLevel = std::min(std::max(baseLevel + levelMod[i], -1.0f), 1.0f);

        // Simple multiply: level acts as amplitude control
        mixedSample += currentOutputs[i] * modulatedLevel;
    }

    // Process all samplers and mix
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        // FM input for samplers can come from oscillator outputs (simple sum)
        float fmInput = 0.0f;
        for (int j = 0; j < OSCILLATORS_PER_VOICE; ++j) {
            fmInput += currentOutputs[j];
        }
        // Normalize FM input
        fmInput /= OSCILLATORS_PER_VOICE;

        // Process sampler with modulation
        float samplerOut = samplers[i].process(sampleRate, fmInput,
                                              samplerPitchMod[i],
                                              samplerLoopStartMod[i],
                                              samplerLoopLengthMod[i],
                                              samplerCrossfadeMod[i],
                                              samplerLevelMod[i]);
        mixedSample += samplerOut;
    }

    // Cache outputs for next sample's FM routing
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        lastOscOutputs[i] = currentOutputs[i];
    }

    // Return mixed sample WITHOUT envelope multiplication
    // Envelope is now routed through modulation matrix to oscillator levels
    return mixedSample;
}
