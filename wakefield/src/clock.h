#ifndef CLOCK_H
#define CLOCK_H

#include <cstdint>
#include <cmath>

// Tempo multiplier (replaces old subdivision enum)
// Multiplier relative to quarter note:
//   4.0 = 1/16 note (4x faster than quarter)
//   2.0 = 1/8 note (2x faster)
//   1.0 = 1/4 note (quarter note, baseline)
//   0.5 = 1/2 note (half as fast)
//   0.25 = whole note (quarter as fast)
//   Supports fractional values for unconventional subdivisions
using Subdivision = float;

// Common subdivision constants
namespace Subdivisions {
    constexpr Subdivision WHOLE = 0.25f;
    constexpr Subdivision HALF = 0.5f;
    constexpr Subdivision QUARTER = 1.0f;
    constexpr Subdivision EIGHTH = 2.0f;
    constexpr Subdivision SIXTEENTH = 4.0f;
    constexpr Subdivision THIRTYSECOND = 8.0f;
    constexpr Subdivision SIXTYFOURTH = 16.0f;
}

class Clock {
public:
    Clock(float sampleRate);

    // Transport control
    void play() { playing = true; }
    void stop() { playing = false; }
    void reset();

    // Tempo control
    void setTempo(double bpm);
    double getTempo() const { return tempo; }

    // Advance clock by nFrames samples
    void advance(unsigned int nFrames);

    // Check if we crossed a step boundary this buffer
    // Returns true and sets stepIndex if a step was triggered
    bool checkStepTrigger(unsigned int nFrames, Subdivision subdiv, int& stepIndex);

    // Get current phase (0.0-1.0) for a subdivision
    double getPhase(Subdivision subdiv) const;

    // Get current step for a subdivision
    int getCurrentStep(Subdivision subdiv) const;

    // Get samples per step for a subdivision
    double getSamplesPerStep(Subdivision subdiv) const;

    // Loop points (for pattern looping)
    void setLoopPoints(int startStep, int endStep, Subdivision subdiv);
    void enableLoop(bool enabled) { loopEnabled = enabled; }
    bool isLoopEnabled() const { return loopEnabled; }

    // External sync (future MIDI clock support)
    void enableExternalSync(bool enabled) { externalSync = enabled; }
    bool isExternalSync() const { return externalSync; }

    bool isPlaying() const { return playing; }

private:
    float sampleRate;
    double tempo;                  // BPM (0.1-999)
    double samplesPerBeat;         // Samples in one quarter note
    uint64_t sampleCounter;        // Global sample position

    bool playing;
    bool loopEnabled;
    bool externalSync;

    int loopStartStep;
    int loopEndStep;
    Subdivision loopSubdivision;
};

#endif // CLOCK_H
