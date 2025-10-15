#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <vector>
#include <map>
#include "clock.h"
#include "pattern.h"
#include "constraint.h"
#include "markov.h"
#include "euclidean.h"
#include "track.h"
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

    // Multi-track support
    int getTrackCount() const { return tracks.size(); }
    Track& getCurrentTrack() { return tracks[currentTrackIndex]; }
    const Track& getCurrentTrack() const { return tracks[currentTrackIndex]; }
    Track& getTrack(int index) { return tracks[index]; }
    const Track& getTrack(int index) const { return tracks[index]; }

    int getCurrentTrackIndex() const { return currentTrackIndex; }
    void setCurrentTrack(int index);
    void nextTrack();
    void prevTrack();

    // Pattern generation (operates on current track)
    void generatePattern();
    void regenerateUnlocked();
    void mutatePattern(float amount);
    void clearPattern();

    // Pattern rotation (synchronized)
    void rotatePattern(int steps);

    // Configuration (current track)
    MusicalConstraints& getConstraints() { return getCurrentTrack().getConstraints(); }
    EuclideanPattern& getEuclideanPattern() { return getCurrentTrack().getEuclideanPattern(); }
    MarkovChain& getMarkovChain() { return getCurrentTrack().getMarkovChain(); }
    Pattern& getPattern() { return getCurrentTrack().getPattern(); }
    const Pattern& getPattern() const { return getCurrentTrack().getPattern(); }

    void setMarkovMode(int mode);  // 0=random, 1=orbit, 2=ascend, 3=descend, 4=drone
    void setEuclideanPattern(int hits, int steps, int rotation);

    int getCurrentStep() const {
        return (!currentSteps.empty() && currentTrackIndex >= 0 && currentTrackIndex < static_cast<int>(currentSteps.size()))
                   ? currentSteps[currentTrackIndex]
                   : 0;
    }
    int getCurrentStep(int trackIndex) const {
        if (trackIndex >= 0 && trackIndex < static_cast<int>(currentSteps.size())) {
            return currentSteps[trackIndex];
        }
        return 0;
    }

    // Process audio (called from audio callback)
    void process(unsigned int nFrames);

    // Note management
    void allNotesOff();

private:
    Clock clock;
    std::vector<Track> tracks;
    int currentTrackIndex;

    Synth* synth;
    std::vector<int> currentSteps;  // Per-track playback position
    std::vector<int> lastTriggeredStep;  // Per-track

    // Track active notes for gate length management
    struct ActiveNote {
        int midiNote;
        uint64_t startSample;
        float gateLength;  // In steps
        Subdivision subdivision;  // Track subdivision for gate timing
    };
    std::vector<ActiveNote> activeNotes;

    // Trigger a step
    void triggerTrackStep(size_t trackIndex, int step);

    // Check and release notes based on gate length
    void updateGates();
};

#endif // SEQUENCER_H
