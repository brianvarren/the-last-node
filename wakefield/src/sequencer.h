#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <vector>
#include <map>
#include "clock.h"
#include "pattern.h"
#include "constraint.h"
#include "markov.h"
#include "euclidean.h"
#include "synth.h"

class Sequencer {
public:
    Sequencer(float sampleRate, Synth* synth);

    // Transport control
    void play();
    void stop();
    void reset();
    bool isPlaying() const { return clock.isPlaying(); }

    // Tempo control
    void setTempo(double bpm) { clock.setTempo(bpm); }
    double getTempo() const { return clock.getTempo(); }

    // Pattern generation
    void generatePattern();
    void regenerateUnlocked();
    void mutatePattern(float amount);
    void clearPattern();

    // Configuration
    void setConstraints(const MusicalConstraints& c) { constraints = c; }
    MusicalConstraints& getConstraints() { return constraints; }

    void setEuclideanPattern(int hits, int steps, int rotation);
    EuclideanPattern& getEuclideanPattern() { return euclideanRhythm; }

    void setMarkovMode(int mode);  // 0=random, 1=orbit, 2=ascend, 3=descend, 4=drone
    MarkovChain& getMarkovChain() { return markovChain; }

    // Pattern access
    Pattern& getPattern() { return currentPattern; }
    const Pattern& getPattern() const { return currentPattern; }

    int getCurrentStep() const { return currentStep; }

    // Process audio (called from audio callback)
    void process(unsigned int nFrames);

    // Note management
    void allNotesOff();

private:
    Clock clock;
    Pattern currentPattern;
    MusicalConstraints constraints;
    MarkovChain markovChain;
    EuclideanPattern euclideanRhythm;

    Synth* synth;
    int currentStep;
    int lastTriggeredStep;

    // Track active notes for gate length management
    struct ActiveNote {
        int midiNote;
        uint64_t startSample;
        float gateLength;  // In steps
    };
    std::vector<ActiveNote> activeNotes;

    // Trigger a step
    void triggerStep(int step);

    // Check and release notes based on gate length
    void updateGates();
};

#endif // SEQUENCER_H
