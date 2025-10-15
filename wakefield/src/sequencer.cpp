#include "sequencer.h"
#include <iostream>
#include <cstdlib>

Sequencer::Sequencer(float sampleRate, Synth* synth)
    : clock(sampleRate)
    , currentPattern(16, Subdivision::SIXTEENTH)
    , constraints()
    , markovChain()
    , euclideanRhythm(7, 16, 0)  // Default: 7 hits in 16 steps
    , synth(synth)
    , currentStep(0)
    , lastTriggeredStep(-1)
{
    // Set default constraints (D Phrygian, octaves 2-4)
    constraints.setScale(Scale::PHRYGIAN);
    constraints.setRootNote(2);  // D
    constraints.setOctaveRange(2, 4);
    constraints.setContour(Contour::ORBITING);
    constraints.setGravityNote(50);  // D3
    constraints.setDensity(0.6f);

    // Initialize Markov chain
    std::vector<int> legalNotes = constraints.getLegalNotes();
    markovChain.initialize(legalNotes);
    markovChain.setOrbitingPattern(constraints.getGravityNote());

    // Generate initial pattern
    generatePattern();

    // Set default tempo
    clock.setTempo(90.0);  // Slow, ambient tempo
}

void Sequencer::play() {
    clock.play();
}

void Sequencer::stop() {
    clock.stop();
    allNotesOff();
}

void Sequencer::reset() {
    clock.reset();
    currentStep = 0;
    lastTriggeredStep = -1;
    allNotesOff();
}

void Sequencer::setEuclideanPattern(int hits, int steps, int rotation) {
    euclideanRhythm = EuclideanPattern(hits, steps, rotation);
}

void Sequencer::setMarkovMode(int mode) {
    switch (mode) {
        case 0:  // Random walk
            markovChain.setRandomWalk();
            break;
        case 1:  // Orbiting
            markovChain.setOrbitingPattern(constraints.getGravityNote());
            break;
        case 2:  // Ascending
            markovChain.setAscending();
            break;
        case 3:  // Descending
            markovChain.setDescending();
            break;
        case 4:  // Drone
            markovChain.setDronePattern();
            break;
        default:
            markovChain.setRandomWalk();
            break;
    }
}

void Sequencer::generatePattern() {
    // Update Markov chain with current legal notes
    std::vector<int> legalNotes = constraints.getLegalNotes();
    if (!legalNotes.empty()) {
        markovChain.initialize(legalNotes);

        // Re-apply current Markov mode based on contour
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

    // Generate pattern using current constraints
    currentPattern.generateFromConstraints(constraints, markovChain, euclideanRhythm);
}

void Sequencer::regenerateUnlocked() {
    // Regenerate only unlocked steps
    currentPattern.regenerateUnlocked(constraints, markovChain, euclideanRhythm);
}

void Sequencer::mutatePattern(float amount) {
    currentPattern.mutate(amount);
}

void Sequencer::clearPattern() {
    currentPattern.clear();
    allNotesOff();
}

void Sequencer::allNotesOff() {
    if (!synth) return;

    // Send note-off for all active notes
    for (const auto& note : activeNotes) {
        synth->noteOff(note.midiNote);
    }
    activeNotes.clear();
}

void Sequencer::triggerStep(int step) {
    if (!synth) return;

    PatternStep& patternStep = currentPattern.getStep(step);

    if (!patternStep.active) return;

    // Probability check
    float r = static_cast<float>(rand()) / RAND_MAX;
    if (r > patternStep.probability) {
        return;  // Skip this trigger
    }

    // Trigger note
    synth->noteOn(patternStep.midiNote, patternStep.velocity);

    // Track active note for gate management
    ActiveNote activeNote;
    activeNote.midiNote = patternStep.midiNote;
    activeNote.startSample = clock.getCurrentStep(currentPattern.getResolution()) *
                             clock.getSamplesPerStep(currentPattern.getResolution());
    activeNote.gateLength = patternStep.gateLength;
    activeNotes.push_back(activeNote);

    // TODO: Apply parameter automation if set
    // if (patternStep.filterCutoff > 0) { ... }
    // if (patternStep.reverbMix >= 0) { ... }
    // if (patternStep.brainwaveMorph >= 0) { ... }
}

void Sequencer::updateGates() {
    if (!synth || activeNotes.empty()) return;

    uint64_t currentSample = clock.getCurrentStep(currentPattern.getResolution()) *
                             clock.getSamplesPerStep(currentPattern.getResolution());
    double samplesPerStep = clock.getSamplesPerStep(currentPattern.getResolution());

    // Check each active note
    auto it = activeNotes.begin();
    while (it != activeNotes.end()) {
        uint64_t elapsed = currentSample - it->startSample;
        double gateTimeInSamples = samplesPerStep * it->gateLength;

        if (elapsed >= static_cast<uint64_t>(gateTimeInSamples)) {
            // Gate time expired, send note-off
            synth->noteOff(it->midiNote);
            it = activeNotes.erase(it);
        } else {
            ++it;
        }
    }
}

void Sequencer::process(unsigned int nFrames) {
    if (!clock.isPlaying()) {
        return;
    }

    // Check if we crossed a step boundary
    int stepIndex;
    if (clock.checkStepTrigger(nFrames, currentPattern.getResolution(), stepIndex)) {
        // Wrap step to pattern length
        currentStep = stepIndex % currentPattern.getLength();

        // Trigger the step if it wasn't just triggered
        if (currentStep != lastTriggeredStep) {
            triggerStep(currentStep);
            lastTriggeredStep = currentStep;
        }
    }

    // Update gate lengths
    updateGates();

    // Advance clock
    clock.advance(nFrames);
}
