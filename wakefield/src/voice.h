#ifndef VOICE_H
#define VOICE_H

#include "envelope.h"
#include "brainwave_osc.h"

// Forward declaration
struct SynthParameters;

constexpr int OSCILLATORS_PER_VOICE = 4;

struct Voice {
    bool active;           // Is this voice currently playing?
    int note;             // MIDI note number
    int velocity;         // MIDI velocity (0-127)
    BrainwaveOscillator oscillators[OSCILLATORS_PER_VOICE]; // 4 oscillators per voice
    Envelope envelope;     // Amplitude envelope
    SynthParameters* params;  // Pointer to FM matrix and other params

    // Modulation storage (set once per buffer by Synth::process)
    float pitchMod[OSCILLATORS_PER_VOICE];
    float morphMod[OSCILLATORS_PER_VOICE];
    float dutyMod[OSCILLATORS_PER_VOICE];
    float ratioMod[OSCILLATORS_PER_VOICE];
    float offsetMod[OSCILLATORS_PER_VOICE];
    float levelMod[OSCILLATORS_PER_VOICE];

    Voice(float sampleRate)
        : active(false)
        , note(-1)
        , velocity(64)
        , envelope(sampleRate)
        , sampleRate(sampleRate)
        , params(nullptr) {
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            lastOscOutputs[i] = 0.0f;
            pitchMod[i] = 0.0f;
            morphMod[i] = 0.0f;
            dutyMod[i] = 0.0f;
            ratioMod[i] = 0.0f;
            offsetMod[i] = 0.0f;
            levelMod[i] = 0.0f;
        }
    }

    // Generate one sample for this voice (implemented in voice.cpp)
    float generateSample();

    // Clear cached oscillator outputs (used when voice retriggers)
    void resetFMHistory();

private:
    float sampleRate;
    float lastOscOutputs[OSCILLATORS_PER_VOICE];
};

#endif // VOICE_H
