#ifndef VOICE_H
#define VOICE_H

#include "fm_constants.h"
#include "envelope.h"
#include "brainwave_osc.h"
#include "sampler.h"

// Forward declarations
struct SynthParameters;
class Synth;

constexpr int OSCILLATORS_PER_VOICE = 4;
constexpr int SAMPLERS_PER_VOICE = 4;

struct Voice {
    bool active;           // Is this voice currently playing?
    int note;             // MIDI note number
    int velocity;         // MIDI velocity (0-127)
    BrainwaveOscillator oscillators[OSCILLATORS_PER_VOICE]; // 4 oscillators per voice
    Sampler samplers[SAMPLERS_PER_VOICE]; // 4 samplers per voice
    Envelope envelope;     // Amplitude envelope
    SynthParameters* params;  // Pointer to FM matrix and other params
    Synth* synth;          // Pointer to synth for base level access

    // Oscillator modulation storage (set once per buffer by Synth::process)
    float pitchMod[OSCILLATORS_PER_VOICE];
    float morphMod[OSCILLATORS_PER_VOICE];
    float dutyMod[OSCILLATORS_PER_VOICE];
    float ratioMod[OSCILLATORS_PER_VOICE];
    float offsetMod[OSCILLATORS_PER_VOICE];
    float ampMod[OSCILLATORS_PER_VOICE];  // Renamed from levelMod

    // Sampler modulation storage
    float samplerPitchMod[SAMPLERS_PER_VOICE];
    float samplerLoopStartMod[SAMPLERS_PER_VOICE];
    float samplerLoopLengthMod[SAMPLERS_PER_VOICE];
    float samplerCrossfadeMod[SAMPLERS_PER_VOICE];
    float samplerLevelMod[SAMPLERS_PER_VOICE];
    float samplerPhaseDriver[SAMPLERS_PER_VOICE];

    // FM global depth modulation
    float fmGlobalDepthMod;
    float fmDepthMod[kFMTargetCount][kFMSourceCount];

    Voice(float sampleRate)
        : active(false)
        , note(-1)
        , velocity(64)
        , envelope(sampleRate)
        , sampleRate(sampleRate)
        , params(nullptr)
        , synth(nullptr)
        , envelopeValue(0.0f) {
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            lastOscOutputs[i] = 0.0f;
            pitchMod[i] = 0.0f;
            morphMod[i] = 0.0f;
            dutyMod[i] = 0.0f;
            ratioMod[i] = 0.0f;
            offsetMod[i] = 0.0f;
            ampMod[i] = 0.0f;
        }
        for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
            lastSamplerOutputs[i] = 0.0f;
            samplerPitchMod[i] = 0.0f;
            samplerLoopStartMod[i] = 0.0f;
            samplerLoopLengthMod[i] = 0.0f;
            samplerCrossfadeMod[i] = 0.0f;
            samplerLevelMod[i] = 0.0f;
            samplerPhaseDriver[i] = -1.0f;
        }
        fmGlobalDepthMod = 0.0f;
        for (int t = 0; t < kFMTargetCount; ++t) {
            for (int s = 0; s < kFMSourceCount; ++s) {
                fmDepthMod[t][s] = 0.0f;
            }
        }
    }

    // Generate one sample for this voice (implemented in voice.cpp)
    // frameIndex: current sample index within buffer (for per-sample modulation sources)
    float generateSample(unsigned int frameIndex = 0);

    // Clear cached oscillator outputs (used when voice retriggers)
    void resetFMHistory();

    // Forcefully silence voice and reset modulation history
    void forceSilence();

    // Get current envelope value (for modulation routing)
    float getEnvelopeValue() const {
        return envelopeValue;
    }

    const float* getLastOscOutputs() const { return lastOscOutputs; }
    const float* getLastSamplerOutputs() const { return lastSamplerOutputs; }

private:
    float sampleRate;
    float lastOscOutputs[OSCILLATORS_PER_VOICE];
    float lastSamplerOutputs[SAMPLERS_PER_VOICE];
    float envelopeValue;  // Current envelope output (cached for modulation)
};

#endif // VOICE_H
