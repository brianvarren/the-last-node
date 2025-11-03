#ifndef SAMPLER_H
#define SAMPLER_H

#include <cstdint>
#include <cmath>
#include <atomic>

// Playback modes
enum class PlaybackMode {
    FORWARD = 0,
    REVERSE = 1,
    ALTERNATE = 2  // Ping-pong
};

// Forward declaration for sample data
struct SampleData;

// Individual voice for crossfading
struct SamplerVoice {
    uint64_t phase_q32_32;      // Q32.32 phase accumulator (32-bit integer + 32-bit fractional)
    uint32_t loop_start;        // Loop start position (samples)
    uint32_t loop_end;          // Loop end position (samples, exclusive)
    float amplitude;            // Current amplitude (0-1) for crossfading
    bool active;                // Is this voice currently playing?
};

class Sampler {
public:
    Sampler();

    // Copy constructor and assignment operator (needed because of atomic member)
    Sampler(const Sampler& other);
    Sampler& operator=(const Sampler& other);

    // Main processing function - generates one sample
    // Parameters:
    //   sampleRate: Output sample rate
    //   fmInput: FM modulation input (for TZFM)
    //   pitchMod: Pitch modulation (-1 to +1, in octaves)
    //   loopStartMod: Loop start modulation (-1 to +1)
    //   loopLengthMod: Loop length modulation (-1 to +1)
    //   crossfadeMod: Crossfade length modulation (-1 to +1)
    //   levelMod: Level modulation (-1 to +1)
    //   levelOffset: Additional mixer level offset (-1 to +1)
    //   midiNote: MIDI note number (0-127) for KEY mode pitch tracking
    float process(float sampleRate, float fmInput, float pitchMod,
                  float loopStartMod, float loopLengthMod,
                  float crossfadeMod, float levelMod, float levelOffset,
                  float phaseDriver, int midiNote = 60);

    // Parameter setters
    void setSample(const SampleData* sample);
    void setLoopStart(float normalized);    // 0.0 to 1.0
    void setLoopLength(float normalized);   // 0.0 to 1.0 (of available range)
    void setCrossfadeLength(float normalized); // 0.0 to 1.0 (of loop length)
    void setPlaybackSpeed(float speed);     // 0.5 = half speed, 2.0 = double speed
    void setTZFMDepth(float depth);         // 0.0 to 1.0
    void setPlaybackMode(PlaybackMode mode);
    void setLevel(float level);             // 0.0 to 1.0
    void setKeyMode(bool enabled);

    // Parameter getters
    float getLoopStart() const { return loopStartNorm; }
    float getLoopLength() const { return loopLengthNorm; }
    float getCrossfadeLength() const { return crossfadeLengthNorm; }
    float getPlaybackSpeed() const { return playbackSpeed; }
    float getTZFMDepth() const { return tzfmDepth; }
    PlaybackMode getPlaybackMode() const { return mode; }
    float getLevel() const { return level; }
    bool isKeyMode() const { return keyMode; }

    // Get current playback position (0.0 to 1.0)
    float getPlayheadPosition() const;

    // Phase management for Note Reset
    uint64_t getCurrentPhase() const;
    void restorePhase(uint64_t phase);

    // Reset phase to beginning of loop
    void reset();
    void requestRestart();          // Force playback to restart at next process call
    void stopPlayback();            // Immediately silence output (used when leaving FREE mode)

    // Get sample name (for UI)
    const char* getSampleName() const;

private:
    // Sample data (atomic to prevent race conditions between audio and UI threads)
    std::atomic<const SampleData*> currentSample;

    // Playback parameters
    float loopStartNorm;        // 0.0 to 1.0
    float loopLengthNorm;       // 0.0 to 1.0
    float crossfadeLengthNorm;  // 0.0 to 1.0
    float playbackSpeed;        // Speed multiplier
    float tzfmDepth;            // TZFM depth (0.0 to 1.0)
    float level;                // Output level (0.0 to 1.0)
    PlaybackMode mode;          // Playback direction (Forward/Reverse/Alternate)
    bool keyMode;               // true = respond to MIDI notes, false = free-run

    // Dual-voice crossfading state
    SamplerVoice voiceA;
    SamplerVoice voiceB;
    SamplerVoice* primaryVoice;
    SamplerVoice* secondaryVoice;

    // Crossfade state
    bool crossfading;
    uint32_t crossfadeSamplesTotal;
    uint32_t crossfadeSamplesRemaining;

    // Pending loop parameters (calculated once per process call)
    uint32_t pendingStart;
    uint32_t pendingEnd;
    bool pendingLoopValid;
    bool restartRequested;

    // Zone detection (prevents retriggering crossfade)
    bool wasInZoneLastSample;

    // Ping-pong direction state (for ALTERNATE mode)
    bool playingReverse;

    // TZFM smoothing
    float modulatorSmoothed;
    static constexpr float MODULATOR_SMOOTHING = 0.85f;

    float lastPhaseDriver;

    // MIDI note to pitch ratio lookup table (128 MIDI notes, note 60 = 1.0)
    // Avoids expensive std::pow(2.0, (note - 60) / 12.0) calculations
    static const float MIDI_PITCH_TABLE[128];

    // Equal-power crossfade lookup table (256 entries for smooth curves)
    // [0] = cos(pi/2 * t), [1] = sin(pi/2 * t) where t = [0..1]
    // Avoids expensive std::cos/sin calls during crossfading
    static constexpr int XFADE_TABLE_SIZE = 256;
    static const float XFADE_TABLE_COS[XFADE_TABLE_SIZE];
    static const float XFADE_TABLE_SIN[XFADE_TABLE_SIZE];

    // Helper functions
    void calculateLoopBoundaries(const SampleData* sample, float startMod, float lengthMod);
    void ensurePendingLoop(const SampleData* sample, float startMod, float lengthMod);
    void applyPendingLoopToVoice(SamplerVoice* voice);
    bool wrapPhase(SamplerVoice* voice) const;
    int16_t getSample(const SampleData* sample, const SamplerVoice* voice, bool isReverse) const;
    int64_t calculateIncrement(const SampleData* sample, float sampleRate, float fmInput, float pitchMod, bool isReverse, int midiNote);
    bool isInCrossfadeZone(uint64_t phase, uint32_t loopStart, uint32_t loopEnd,
                          uint32_t xfadeLen, bool isReverse) const;
    void setupCrossfade(uint32_t xfadeLen, uint32_t xfadeSamples, bool isReverse);
    void applyPhaseDriver(const SampleData* sample, float normalized);

    // Interpolation helper
    static inline int16_t interpolate(int16_t x, int16_t y, uint8_t mu);

    // Fast 2^x approximation for pitch modulation (accurate to <0.15% over [-1, 1])
    // Uses 5th-order minimax polynomial
    static inline float fastPow2(float x);
};

#endif // SAMPLER_H
