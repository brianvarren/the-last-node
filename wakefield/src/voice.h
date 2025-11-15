#ifndef VOICE_H
#define VOICE_H

#include <algorithm>
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
    uint64_t startTime;   // Voice start timestamp (for voice stealing priority)
    int noteSource;       // Which source triggered this voice (NoteSource enum value)
    BrainwaveOscillator oscillators[OSCILLATORS_PER_VOICE]; // 4 oscillators per voice
    Sampler samplers[SAMPLERS_PER_VOICE]; // 4 samplers per voice
    Envelope envelopes[4];     // 4 per-voice envelopes (ENV1..ENV4)
    SynthParameters* params;  // Pointer to FM matrix and other params
    Synth* synth;          // Pointer to synth for base level access

    // Global FM inputs provided by Synth (nullptr until updated each buffer)
    const float* globalFmInputs;
    const float* globalSamplerFmInputs;

    // Oscillator modulation storage (set once per buffer by Synth::process)
    // CRITICAL params use one-pole smoothing (target + smoothed state)
    // NON-CRITICAL params use direct block-rate values (no smoothing)

    // Critical: Smoothed for zipper prevention
    float pitchMod[OSCILLATORS_PER_VOICE];           // Target pitch mod (updated per buffer)
    float smoothedPitchMod[OSCILLATORS_PER_VOICE];   // Smoothed state (updated per sample)
    float ampMod[OSCILLATORS_PER_VOICE];             // Target amplitude mod
    float smoothedAmpMod[OSCILLATORS_PER_VOICE];     // Smoothed amplitude
    float cachedOscLevel[OSCILLATORS_PER_VOICE];     // Target oscillator level
    float smoothedOscLevel[OSCILLATORS_PER_VOICE];   // Smoothed level

    // Non-critical: Direct block-rate (oscillator handles internal smoothing)
    float morphMod[OSCILLATORS_PER_VOICE];
    float ratioMod[OSCILLATORS_PER_VOICE];
    float offsetMod[OSCILLATORS_PER_VOICE];

    // Sampler modulation storage
    // Critical: Smoothed
    float samplerPitchMod[SAMPLERS_PER_VOICE];
    float smoothedSamplerPitchMod[SAMPLERS_PER_VOICE];
    float samplerLevelMod[SAMPLERS_PER_VOICE];
    float smoothedSamplerLevelMod[SAMPLERS_PER_VOICE];

    // Non-critical: Direct block-rate
    float samplerLoopStartMod[SAMPLERS_PER_VOICE];
    float samplerLoopLengthMod[SAMPLERS_PER_VOICE];
    float samplerCrossfadeMod[SAMPLERS_PER_VOICE];
    float samplerPhaseDriver[SAMPLERS_PER_VOICE];

    // Cached sampler level modulation (non-critical)
    float cachedSamplerLevelMod[SAMPLERS_PER_VOICE];

    // Buffer size for audio-rate interpolation (set per buffer)
    unsigned int currentBufferSize;

    // Linear interpolation increments for critical modulation (per-sample deltas)
    float pitchModIncrement[OSCILLATORS_PER_VOICE];
    float ampModIncrement[OSCILLATORS_PER_VOICE];
    float oscLevelIncrement[OSCILLATORS_PER_VOICE];
    float samplerPitchModIncrement[SAMPLERS_PER_VOICE];
    float samplerLevelModIncrement[SAMPLERS_PER_VOICE];

    // One-pole smoothing coefficient (alpha) - DEPRECATED, now using linear interpolation
    // NOTE: All critical modulation (envelopes, pitch, amp, level) use linear interpolation
    // Kept for reference only
    static constexpr float kSmoothingAlpha = 0.1f;
    static constexpr float kAmpGateSilenceThreshold = 1e-4f;

    // Block-level caches to avoid per-sample atomics
    int cachedActiveOscCount = OSCILLATORS_PER_VOICE;
    int cachedActiveSamplerCount = SAMPLERS_PER_VOICE;
    bool cachedAnySolo = false;
    bool cachedOscSolo[OSCILLATORS_PER_VOICE]{};
    bool cachedOscMuted[OSCILLATORS_PER_VOICE]{};
    bool cachedSamplerSolo[SAMPLERS_PER_VOICE]{};
    bool cachedSamplerMuted[SAMPLERS_PER_VOICE]{};
    bool cachedChaosSolo[4]{};

    float envelopeTargets[4]{};
    float envelopeIncrements[4]{};  // Per-sample increments for linear interpolation
    float ampGateValue = 0.0f;
    float ampGateTarget = 0.0f;
    int freeRunningOscIndex = -1;
    int freeRunningSamplerIndex = -1;

    // === Half-Rate Processing State ===
    // When enabled, oscillators and samplers run at half the sample rate (e.g., 24kHz @ 48kHz)
    // Provides ~30-35% CPU savings at the cost of bandwidth above ~12kHz
    bool halfRateEnabled = false;              // Enable half-rate mode
    bool halfRateTick = false;                 // Alternates true/false each sample
    float cachedOscOutputs[OSCILLATORS_PER_VOICE]{};   // Previous osc samples for interpolation
    float cachedSamplerOutputs[SAMPLERS_PER_VOICE]{};  // Previous sampler samples for interpolation

    Voice(float sampleRate)
        : active(false)
        , note(-1)
        , velocity(64)
        , startTime(0)
        , noteSource(0)
        , envelopes{ Envelope(sampleRate), Envelope(sampleRate), Envelope(sampleRate), Envelope(sampleRate) }
        , sampleRate(sampleRate)
        , globalFmInputs(nullptr)
        , globalSamplerFmInputs(nullptr)
        , params(nullptr)
        , synth(nullptr)
        {
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            lastOscOutputs[i] = 0.0f;
            // Critical (smoothed)
            pitchMod[i] = 0.0f;
            smoothedPitchMod[i] = 0.0f;
            ampMod[i] = 0.0f;
            smoothedAmpMod[i] = 0.0f;
            cachedOscLevel[i] = 0.0f;
            smoothedOscLevel[i] = 0.0f;
            // Non-critical (direct)
            morphMod[i] = 0.0f;
            ratioMod[i] = 0.0f;
            offsetMod[i] = 0.0f;
        }
        for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
            lastSamplerOutputs[i] = 0.0f;
            // Critical (smoothed)
            samplerPitchMod[i] = 0.0f;
            smoothedSamplerPitchMod[i] = 0.0f;
            samplerLevelMod[i] = 0.0f;
            smoothedSamplerLevelMod[i] = 0.0f;
            // Non-critical (direct)
            samplerLoopStartMod[i] = 0.0f;
            samplerLoopLengthMod[i] = 0.0f;
            samplerCrossfadeMod[i] = 0.0f;
            samplerPhaseDriver[i] = -1.0f;
            cachedSamplerLevelMod[i] = 0.0f;
        }
        currentBufferSize = 256;  // Default buffer size
        for (int i = 0; i < 4; ++i) envelopeValues[i] = 0.0f;
        ampGateValue = 0.0f;
        ampGateTarget = 0.0f;
        freeRunningOscIndex = -1;
        freeRunningSamplerIndex = -1;
        halfRateEnabled = false;
        halfRateTick = false;
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) cachedOscOutputs[i] = 0.0f;
        for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) cachedSamplerOutputs[i] = 0.0f;
    }

    // Generate one sample for this voice (implemented in voice.cpp)
    // frameIndex: current sample index within buffer (for per-sample modulation sources)
    float generateSample(unsigned int frameIndex = 0);

    // Clear cached oscillator outputs (used when voice retriggers)
    void resetFMHistory();

    // Forcefully silence voice and reset modulation history
    void forceSilence();

    // Get current envelope value (for modulation routing)
    float getEnvelopeValue() const { return envelopeValues[0]; }
    float getEnvelopeValue(int index) const {
        int idx = (index < 0) ? 0 : (index > 3 ? 0 : index);
        return envelopeValues[idx];
    }
    bool isGateReleasing() const { return ampGateTarget <= 0.0f; }
    float getCurrentAmpLevel() const { return ampGateValue; }

    const float* getLastOscOutputs() const { return lastOscOutputs; }
    const float* getLastSamplerOutputs() const { return lastSamplerOutputs; }

    void setGlobalFmInputs(const float* oscFm, const float* samplerFm) {
        globalFmInputs = oscFm;
        globalSamplerFmInputs = samplerFm;
    }

    void clearGlobalFmInputs() {
        globalFmInputs = nullptr;
        globalSamplerFmInputs = nullptr;
    }

    void contributeToGlobalFm(float* globalOsc, float* globalSampler) const {
        if (!globalOsc || !globalSampler) {
            return;
        }
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            globalOsc[i] += lastOscOutputs[i];
        }
        for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
            globalSampler[i] += lastSamplerOutputs[i];
        }
    }

private:
    float sampleRate;
    float lastOscOutputs[OSCILLATORS_PER_VOICE];
    float lastSamplerOutputs[SAMPLERS_PER_VOICE];
    float envelopeValues[4];  // Current envelope outputs (cached for modulation)
};

#endif // VOICE_H
