#include "track.h"

Track::Track(int trackId, int patternLength, Subdivision subdivision)
    : id(trackId)
    , name("Track " + std::to_string(trackId + 1))
    , pattern(patternLength, subdivision)
    , constraints()
    , markovChain()
    , euclideanPattern(7, 16, 0)
    , muted(false)
    , solo(false)
{
    // Default track settings
    constraints.setScale(Scale::PHRYGIAN);
    constraints.setRootNote(2);  // D
    constraints.setOctaveRange(2, 4);
    constraints.setContour(Contour::ORBITING);
    constraints.setGravityNote(50);  // D3
    constraints.setDensity(0.6f);

    // Initialize Markov chain
    std::vector<int> legalNotes = constraints.getLegalNotes();
    if (!legalNotes.empty()) {
        markovChain.initialize(legalNotes);
        markovChain.setOrbitingPattern(constraints.getGravityNote());
    }
}

void Track::generatePattern() {
    // Update Markov chain with current legal notes
    std::vector<int> legalNotes = constraints.getLegalNotes();
    if (!legalNotes.empty()) {
        markovChain.initialize(legalNotes);

        // Re-apply contour mode
        switch (constraints.getContour()) {
            case Contour::RANDOM_WALK:
                markovChain.setRandomWalk();
                break;
            case Contour::ORBITING:
                markovChain.setOrbitingPattern(constraints.getGravityNote());
                break;
            case Contour::ASCENDING:
                markovChain.setAscending();
                break;
            case Contour::DESCENDING:
                markovChain.setDescending();
                break;
            case Contour::DRONE:
                markovChain.setDronePattern();
                break;
        }
    }

    pattern.generateFromConstraints(constraints, markovChain, euclideanPattern);
}

void Track::regenerateUnlocked() {
    pattern.regenerateUnlocked(constraints, markovChain, euclideanPattern);
}

void Track::mutate(float amount) {
    pattern.mutate(amount);
}

void Track::setSubdivision(Subdivision subdiv) {
    pattern.setResolution(subdiv);
}

Subdivision Track::getSubdivision() const {
    return pattern.getResolution();
}
