#include "sequencer.h"
#include <iostream>
#include <cstdlib>

Sequencer::Sequencer(float sampleRate, Synth* synth)
    : clock(sampleRate)
    , currentTrackIndex(0)
    , synth(synth)
    , currentStep(0)
{
    // Create 4 tracks by default
    for (int i = 0; i < 4; ++i) {
        tracks.emplace_back(i, 16, Subdivision::SIXTEENTH);
        lastTriggeredStep.push_back(-1);
    }

    // Generate initial patterns for all tracks
    for (auto& track : tracks) {
        track.generatePattern();
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
    currentStep = 0;
    for (auto& step : lastTriggeredStep) {
        step = -1;
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

void Sequencer::triggerStep(int step) {
    if (!synth) return;

    // Process all tracks
    for (size_t trackIdx = 0; trackIdx < tracks.size(); ++trackIdx) {
        Track& track = tracks[trackIdx];

        // Skip muted tracks
        if (track.isMuted()) continue;

        // Check if any track is soloed
        bool anySolo = false;
        for (const auto& t : tracks) {
            if (t.isSolo()) {
                anySolo = true;
                break;
            }
        }

        // If solo mode is active, only play soloed tracks
        if (anySolo && !track.isSolo()) continue;

        Pattern& pattern = track.getPattern();
        PatternStep& patternStep = pattern.getStep(step);

        if (!patternStep.active) continue;

        // Probability check
        float r = static_cast<float>(rand()) / RAND_MAX;
        if (r > patternStep.probability) {
            continue;  // Skip this trigger
        }

        // Trigger note
        synth->noteOn(patternStep.midiNote, patternStep.velocity);

        // Track active note for gate management
        ActiveNote activeNote;
        activeNote.midiNote = patternStep.midiNote;
        activeNote.startSample = clock.getCurrentStep(pattern.getResolution()) *
                                 clock.getSamplesPerStep(pattern.getResolution());
        activeNote.gateLength = patternStep.gateLength;
        activeNotes.push_back(activeNote);
    }
}

void Sequencer::updateGates() {
    if (!synth || activeNotes.empty()) return;

    // Use current track's subdivision for sample calculation (could be per-track in future)
    uint64_t currentSample = clock.getCurrentStep(getCurrentTrack().getPattern().getResolution()) *
                             clock.getSamplesPerStep(getCurrentTrack().getPattern().getResolution());
    double samplesPerStep = clock.getSamplesPerStep(getCurrentTrack().getPattern().getResolution());

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

    // Process each track with its own subdivision
    for (size_t trackIdx = 0; trackIdx < tracks.size(); ++trackIdx) {
        Track& track = tracks[trackIdx];
        Pattern& pattern = track.getPattern();
        Subdivision subdiv = pattern.getResolution();

        int stepIndex;
        if (clock.checkStepTrigger(nFrames, subdiv, stepIndex)) {
            // Wrap step to pattern length
            int trackStep = stepIndex % pattern.getLength();

            // Trigger the step if it wasn't just triggered
            if (trackStep != lastTriggeredStep[trackIdx]) {
                // Only trigger from track 0 for now (simplification)
                if (trackIdx == 0) {
                    currentStep = trackStep;
                }
                lastTriggeredStep[trackIdx] = trackStep;
            }
        }
    }

    // Trigger current step (processes all tracks)
    static int lastProcessedStep = -1;
    if (currentStep != lastProcessedStep) {
        triggerStep(currentStep);
        lastProcessedStep = currentStep;
    }

    // Update gate lengths
    updateGates();

    // Advance clock
    clock.advance(nFrames);
}
