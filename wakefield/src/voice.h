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

    Voice(float sampleRate)
        : active(false)
        , note(-1)
        , velocity(64)
        , envelope(sampleRate)
        , sampleRate(sampleRate)
        , params(nullptr) {
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            lastOscOutputs[i] = 0.0f;
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
