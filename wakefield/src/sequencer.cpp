#include "sequencer.h"
#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>

Sequencer::Sequencer(Clock* clockSource, Synth* synth)
    : clock(clockSource)
    , currentTrackIndex(0)
    , synth(synth)
{
    if (!clock) {
        throw std::runtime_error("Sequencer requires a valid Clock pointer");
    }

    // Create 4 tracks by default
    for (int i = 0; i < 4; ++i) {
        tracks.emplace_back(i, 16, Subdivision::SIXTEENTH);
        lastTriggeredStep.push_back(-1);
        currentSteps.push_back(0);
        trackPhaseSource.push_back(kClockModSourceIndex);
        trackPhaseType.push_back(0);
    }

    // Set default tempo
    clock->setTempo(90.0);  // Slow, ambient tempo
}

void Sequencer::play() {
    if (clock) {
        clock->play();
    }
}

void Sequencer::stop() {
    if (clock) {
        clock->stop();
    }
    allNotesOff();
}

void Sequencer::reset() {
    if (clock) {
        clock->reset();
    }
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
    if (clock) {
        activeNote.startSample = clock->getCurrentStep(pattern.getResolution()) *
                                 clock->getSamplesPerStep(pattern.getResolution());
    } else {
        activeNote.startSample = 0;
    }
    activeNote.gateLength = patternStep.gateLength;
    activeNote.subdivision = pattern.getResolution();
    activeNotes.push_back(activeNote);
}

void Sequencer::updateGates() {
    if (!synth || activeNotes.empty() || !clock) return;

    // Check each active note
    auto it = activeNotes.begin();
    while (it != activeNotes.end()) {
        uint64_t currentSample = clock->getCurrentStep(it->subdivision) *
                                 clock->getSamplesPerStep(it->subdivision);
        double samplesPerStep = clock->getSamplesPerStep(it->subdivision);
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

void Sequencer::refreshTrackPhaseDrivers() {
    if (trackPhaseSource.size() != tracks.size()) {
        trackPhaseSource.assign(tracks.size(), kClockModSourceIndex);
    }
    if (trackPhaseType.size() != tracks.size()) {
        trackPhaseType.assign(tracks.size(), 0);
    }

    std::fill(trackPhaseSource.begin(), trackPhaseSource.end(), kClockModSourceIndex);
    std::fill(trackPhaseType.begin(), trackPhaseType.end(), 0);

    if (!synth) {
        return;
    }

    for (int slotIdx = 0; slotIdx < kModulationSlotCount; ++slotIdx) {
        const ModulationSlot* slot = synth->getModulationSlot(slotIdx);
        if (!slot || !slot->isComplete()) {
            continue;
        }

        if (slot->destination >= kClockTargetSequencerBase &&
            slot->destination < kClockTargetSequencerBase + static_cast<int>(tracks.size())) {
            int trackIdx = slot->destination - kClockTargetSequencerBase;
            trackPhaseSource[trackIdx] = slot->source >= 0 ? slot->source : kClockModSourceIndex;
            trackPhaseType[trackIdx] = slot->type >= 0 ? slot->type : 0;
        }
    }
}

void Sequencer::process(unsigned int nFrames) {
    if (!clock || !clock->isPlaying()) {
        return;
    }

    refreshTrackPhaseDrivers();

    Synth::ModulationOutputs modOutputs;
    if (synth) {
        modOutputs = synth->processModulationMatrix();
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
        if (clock->checkStepTrigger(nFrames, subdiv, stepIndex)) {
            int patternLength = pattern.getLength();
            if (patternLength <= 0) {
                continue;
            }

            int trackStep = 0;
            if (trackPhaseSource[trackIdx] == kClockModSourceIndex || trackPhaseSource[trackIdx] < 0) {
                trackStep = stepIndex % patternLength;
            } else {
                float driverValue = modOutputs.sequencerPhase[trackIdx];
                float normalized = 0.0f;
                if (trackPhaseType[trackIdx] == 0) {
                    normalized = std::clamp(driverValue, 0.0f, 1.0f);
                } else {
                    normalized = std::clamp((driverValue + 1.0f) * 0.5f, 0.0f, 1.0f);
                }
                trackStep = static_cast<int>(normalized * patternLength);
                if (trackStep >= patternLength) {
                    trackStep = patternLength - 1;
                }
                if (trackStep < 0) {
                    trackStep = 0;
                }
            }

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
    if (clock) {
        clock->advance(nFrames);
    }
}
