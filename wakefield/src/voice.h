#ifndef VOICE_H
#define VOICE_H

#include "envelope.h"
#include "brainwave_osc.h"

struct Voice {
    bool active;           // Is this voice currently playing?
    int note;             // MIDI note number
    BrainwaveOscillator oscillator; // Brainwave wavetable oscillator
    Envelope envelope;     // Amplitude envelope
    
    Voice(float sampleRate) 
        : active(false)
        , note(-1)
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
        
        // Generate oscillator sample and apply envelope
        float sample = oscillator.process(sampleRate);
        return sample * envLevel;
    }
    
private:
    float sampleRate;
};

#endif // VOICE_H