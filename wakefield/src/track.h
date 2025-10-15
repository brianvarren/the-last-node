#ifndef TRACK_H
#define TRACK_H

#include "pattern.h"
#include "constraint.h"
#include "markov.h"
#include "euclidean.h"
#include <string>

// A single track in the sequencer (independent pattern + constraints)
class Track {
public:
    Track(int trackId, int patternLength = 16, Subdivision subdivision = Subdivision::SIXTEENTH);

    // Track identification
    int getId() const { return id; }
    std::string getName() const { return name; }
    void setName(const std::string& n) { name = n; }

    // Pattern access
    Pattern& getPattern() { return pattern; }
    const Pattern& getPattern() const { return pattern; }

    // Constraints access
    MusicalConstraints& getConstraints() { return constraints; }
    const MusicalConstraints& getConstraints() const { return constraints; }

    // Markov chain access
    MarkovChain& getMarkovChain() { return markovChain; }
    const MarkovChain& getMarkovChain() const { return markovChain; }

    // Euclidean pattern access
    EuclideanPattern& getEuclideanPattern() { return euclideanPattern; }
    const EuclideanPattern& getEuclideanPattern() const { return euclideanPattern; }

    // Generation
    void generatePattern();
    void regenerateUnlocked();
    void mutate(float amount);

    // Track settings
    bool isMuted() const { return muted; }
    void setMuted(bool m) { muted = m; }

    bool isSolo() const { return solo; }
    void setSolo(bool s) { solo = s; }

    // Subdivision (can be different per track)
    void setSubdivision(Subdivision subdiv);
    Subdivision getSubdivision() const;

private:
    int id;
    std::string name;
    Pattern pattern;
    MusicalConstraints constraints;
    MarkovChain markovChain;
    EuclideanPattern euclideanPattern;
    bool muted;
    bool solo;
};

#endif // TRACK_H
