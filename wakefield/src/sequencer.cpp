#include "sequencer.h"
#include <iostream>
#include <cstdlib>

Sequencer::Sequencer(float sampleRate, Synth* synth)
    : clock(sampleRate)
    , currentTrackIndex(0)
    , synth(synth)
{
    // Create 4 tracks by default
    for (int i = 0; i < 4; ++i) {
        tracks.emplace_back(i, 16, Subdivision::SIXTEENTH);
        lastTriggeredStep.push_back(-1);
        currentSteps.push_back(0);
    }

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
    for (size_t i = 0; i < lastTriggeredStep.size(); ++i) {
        lastTriggeredStep[i] = -1;
    }
    for (size_t i = 0; i < currentSteps.size(); ++i) {
        currentSteps[i] = 0;
    }
    allNotesOff();
}

void Sequencer::setCurrentTrack(int index) {
    if (index >= 0 && index < static_cast<int>(tracks.size())) {
        currentTrackIndex = index;
    }
}

void Sequencer::nextTrack() {
    currentTrackIndex = (currentTrackIndex + 1) % tracks.size();
}

void Sequencer::prevTrack() {
    currentTrackIndex = (currentTrackIndex - 1 + tracks.size()) % tracks.size();
}

void Sequencer::setEuclideanPattern(int hits, int steps, int rotation) {
    getCurrentTrack().getEuclideanPattern() = EuclideanPattern(hits, steps, rotation);
}

void Sequencer::setMarkovMode(int mode) {
    MarkovChain& markov = getCurrentTrack().getMarkovChain();
    MusicalConstraints& constraints = getCurrentTrack().getConstraints();

    switch (mode) {
        case 0:  // Random walk
            markov.setRandomWalk();
            break;
        case 1:  // Orbiting
            markov.setOrbitingPattern(constraints.getGravityNote());
            break;
        case 2:  // Ascending
            markov.setAscending();
            break;
        case 3:  // Descending
            markov.setDescending();
            break;
        case 4:  // Drone
            markov.setDronePattern();
            break;
        default:
            markov.setRandomWalk();
            break;
    }
}

void Sequencer::generatePattern() {
    getCurrentTrack().generatePattern();
}

void Sequencer::regenerateUnlocked() {
    getCurrentTrack().regenerateUnlocked();
}

void Sequencer::mutatePattern(float amount) {
    getCurrentTrack().mutate(amount);
}

void Sequencer::clearPattern() {
    getCurrentTrack().getPattern().clear();
    allNotesOff();
}

void Sequencer::rotatePattern(int steps) {
    getCurrentTrack().getPattern().rotate(steps);
}

void Sequencer::allNotesOff() {
    if (!synth) return;

    // Send note-off for all active notes
    for (const auto& note : activeNotes) {
        synth->noteOff(note.midiNote);
    }
    activeNotes.clear();
}

void Sequencer::triggerTrackStep(size_t trackIndex, int step) {
    if (!synth || trackIndex >= tracks.size()) {
        return;
    }

    Track& track = tracks[trackIndex];
    Pattern& pattern = track.getPattern();
    PatternStep& patternStep = pattern.getStep(step);

    if (!patternStep.active) {
        return;
    }

    // Skip muted / solo logic handled by caller

    // Probability check
    float r = static_cast<float>(rand()) / RAND_MAX;
    if (r > patternStep.probability) {
        return;  // Skip this trigger
    }

    synth->noteOn(patternStep.midiNote, patternStep.velocity);

    ActiveNote activeNote{};
    activeNote.midiNote = patternStep.midiNote;
    activeNote.startSample = clock.getCurrentStep(pattern.getResolution()) *
                             clock.getSamplesPerStep(pattern.getResolution());
    activeNote.gateLength = patternStep.gateLength;
    activeNote.subdivision = pattern.getResolution();
    activeNotes.push_back(activeNote);
}

void Sequencer::updateGates() {
    if (!synth || activeNotes.empty()) return;

    // Check each active note
    auto it = activeNotes.begin();
    while (it != activeNotes.end()) {
        uint64_t currentSample = clock.getCurrentStep(it->subdivision) *
                                 clock.getSamplesPerStep(it->subdivision);
        double samplesPerStep = clock.getSamplesPerStep(it->subdivision);
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

    bool anySolo = false;
    for (const auto& track : tracks) {
        if (track.isSolo()) {
            anySolo = true;
            break;
        }
    }

    // Process each track with its own subdivision
    for (size_t trackIdx = 0; trackIdx < tracks.size(); ++trackIdx) {
        Track& track = tracks[trackIdx];
        Pattern& pattern = track.getPattern();
        Subdivision subdiv = pattern.getResolution();

        int stepIndex;
        if (clock.checkStepTrigger(nFrames, subdiv, stepIndex)) {
            int trackStep = stepIndex % pattern.getLength();
            currentSteps[trackIdx] = trackStep;

            if (trackStep != lastTriggeredStep[trackIdx]) {
                lastTriggeredStep[trackIdx] = trackStep;

                bool muted = track.isMuted();
                bool skipForSolo = anySolo && !track.isSolo();
                if (!muted && !skipForSolo) {
                    triggerTrackStep(trackIdx, trackStep);
                }
            }
        }
    }

    updateGates();

    // Advance clock
    clock.advance(nFrames);
}
