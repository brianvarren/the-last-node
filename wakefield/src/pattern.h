#ifndef PATTERN_H
#define PATTERN_H

#include <vector>
#include <string>
#include "clock.h"
#include "constraint.h"
#include "markov.h"
#include "euclidean.h"

// A single step in the pattern
struct PatternStep {
    bool active;           // Is this step triggered?
    bool locked;           // User locked (immune to regeneration)
    int midiNote;          // 0-127
    int velocity;          // 0-127
    float gateLength;      // 0.0-2.0 (1.0 = full step, 2.0 = legato overlap)
    float probability;     // 0.0-1.0 (chance of triggering)

    // Parameter automation per step (-1 = not set/don't change)
    float filterCutoff;    // 20-20000 Hz
    float reverbMix;       // 0.0-1.0
    float brainwaveMorph;  // 0.0-1.0

    PatternStep()
        : active(false)
        , locked(false)
        , midiNote(60)
        , velocity(80)
        , gateLength(0.8f)
        , probability(1.0f)
        , filterCutoff(-1.0f)
        , reverbMix(-1.0f)
        , brainwaveMorph(-1.0f)
    {}
};

class Pattern {
public:
    Pattern(int length = 16, Subdivision resolution = Subdivision::SIXTEENTH);

    // Configuration
    void setLength(int len);
    int getLength() const { return length; }

    void setResolution(Subdivision res) { resolution = res; }
    Subdivision getResolution() const { return resolution; }

    // Step access
    PatternStep& getStep(int index);
    const PatternStep& getStep(int index) const;

    // Generation
    void generateFromConstraints(
        MusicalConstraints& constraints,
        MarkovChain& markov,
        const EuclideanPattern& rhythm
    );

    // Regenerate only unlocked steps
    void regenerateUnlocked(
        MusicalConstraints& constraints,
        MarkovChain& markov,
        const EuclideanPattern& rhythm
    );

    // Mutation (slight variation of existing pattern)
    void mutate(float amount);  // 0.0-1.0 (how much to change)

    // Pattern rotation (phase shift)
    void rotate(int steps);  // Positive = forward, negative = backward
    int getRotation() const { return rotation; }
    void setRotation(int rot) { rotation = rot; }

    // Editing
    void lockStep(int step);
    void unlockStep(int step);
    void toggleLock(int step);
    bool isStepLocked(int step) const;

    void setNote(int step, int midiNote);
    void setVelocity(int step, int velocity);
    void setGateLength(int step, float gate);
    void setProbability(int step, float prob);

    // Clear pattern
    void clear();

    // Randomization
    void randomizeVelocities(int minVel = 60, int maxVel = 100);
    void randomizeProbabilities(float minProb = 0.6f, float maxProb = 1.0f);

    // Serialization
    void saveToFile(const std::string& path) const;
    void loadFromFile(const std::string& path);

    // Get pattern statistics (for UI display)
    int getActiveStepCount() const;
    int getLockedStepCount() const;
    std::string getPatternString() const;  // X = active, · = rest, 🔒 = locked

private:
    std::vector<PatternStep> steps;
    int length;
    Subdivision resolution;
    int rotation;  // Pattern rotation offset

    // Helper for generation
    void generateStep(int stepIndex, MusicalConstraints& constraints,
                      MarkovChain& markov, bool euclideanTrigger);
};

#endif // PATTERN_H
