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
        return 0.0f;
    }

    // Get envelope level
    float envLevel = envelope.process();

    // If envelope finished, deactivate voice and clear FM history
    if (!envelope.isActive()) {
        active = false;
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
    float mixedSample = 0.0f;
    float totalWeight = 0.0f;
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        // Get base level from synth (control-rate) and add level modulation
        float baseLevel = synth ? synth->getOscillatorBaseLevel(i) : 0.0f;
        float modulatedLevel = std::min(std::max(baseLevel + levelMod[i], 0.0f), 1.0f);

        if (modulatedLevel <= 0.0f) {
            continue;
        }
        mixedSample += currentOutputs[i] * modulatedLevel;
        totalWeight += modulatedLevel;
    }
    if (totalWeight > 0.0f) {
        mixedSample /= totalWeight;
    }

    // Cache outputs for next sample's FM routing
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        lastOscOutputs[i] = currentOutputs[i];
    }

    // Apply envelope
    return mixedSample * envLevel;
}
