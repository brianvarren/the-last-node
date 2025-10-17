#ifndef VOICE_H
#define VOICE_H

#include "envelope.h"
#include "brainwave_osc.h"

constexpr int OSCILLATORS_PER_VOICE = 4;

struct Voice {
    bool active;           // Is this voice currently playing?
    int note;             // MIDI note number
    int velocity;         // MIDI velocity (0-127)
    BrainwaveOscillator oscillators[OSCILLATORS_PER_VOICE]; // 4 oscillators per voice
    Envelope envelope;     // Amplitude envelope

    Voice(float sampleRate)
        : active(false)
        , note(-1)
        , velocity(64)
        , envelope(sampleRate)
        , sampleRate(sampleRate) {
    }

    // Generate one sample for this voice
    float generateSample() {
        if (!active) {
            return 0.0f;
        }

        // Get envelope level
        float envLevel = envelope.process();

        // If envelope finished, deactivate voice
        if (!envelope.isActive()) {
            active = false;
            return 0.0f;
        }

        // Mix all oscillators (simple average for now)
        float mixedSample = 0.0f;
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            mixedSample += oscillators[i].process(sampleRate);
        }
        mixedSample /= OSCILLATORS_PER_VOICE;

        // Apply envelope
        return mixedSample * envLevel;
    }

private:
    float sampleRate;
};

#endif // VOICE_H