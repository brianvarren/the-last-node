#ifndef CLOCK_H
#define CLOCK_H

#include <cstdint>
#include <cmath>

// Musical subdivisions
enum class Subdivision {
    WHOLE = 1,
    HALF = 2,
    QUARTER = 4,
    EIGHTH = 8,
    SIXTEENTH = 16,
    THIRTYSECOND = 32,
    SIXTYFOURTH = 64
};

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
    double tempo;                  // BPM (20-300)
    double samplesPerBeat;         // Samples in one quarter note
    uint64_t sampleCounter;        // Global sample position
    uint64_t lastStepSample[7];    // Last trigger sample for each subdivision

    bool playing;
    bool loopEnabled;
    bool externalSync;

    int loopStartStep;
    int loopEndStep;
    Subdivision loopSubdivision;

    // Helper to convert subdivision enum to integer
    int subdivisionToInt(Subdivision subdiv) const {
        return static_cast<int>(subdiv);
    }

    // Helper to get subdivision index (for array access)
    int getSubdivIndex(Subdivision subdiv) const;
};

#endif // CLOCK_H
