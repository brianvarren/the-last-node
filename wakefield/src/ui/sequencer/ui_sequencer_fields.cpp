#include "../../ui.h"
#include "../ui_utils.h"
#include "../../sequencer.h"
#include "../../loop_manager.h"
#include "pattern.h"
#include "track.h"
#include "constraint.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

// External references to global objects from main.cpp
extern LoopManager* loopManager;
extern Sequencer* sequencer;

struct SequencerInfoEntryDef {
    UI::SequencerInfoField field;
    const char* label;
    bool editable;
};

static const std::vector<SequencerInfoEntryDef> kSequencerInfoEntries = {
    {UI::SequencerInfoField::TEMPO,       "Tempo",        true},
    {UI::SequencerInfoField::ROOT,        "Root",         true},
    {UI::SequencerInfoField::SCALE,       "Scale",        true},
    {UI::SequencerInfoField::EUCLID_HITS, "Euclid Hits",  true},
    {UI::SequencerInfoField::EUCLID_STEPS,"Euclid Steps", true},
    {UI::SequencerInfoField::SUBDIVISION, "Subdivision",  true},
    {UI::SequencerInfoField::MUTATE_AMOUNT, "Mutate %",   true},
    {UI::SequencerInfoField::MUTED,       "Muted",        true},
    {UI::SequencerInfoField::SOLO,        "Solo",         true},
    {UI::SequencerInfoField::ACTIVE_COUNT,"Active",       false},
    {UI::SequencerInfoField::PHASE_SOURCE,"Phase Src",    true}
};

void UI::startSequencerScaleMenu() {
    if (!sequencer) return;

    numericInputActive = false;
    numericInputBuffer.clear();

    Track& track = sequencer->getCurrentTrack();
    MusicalConstraints& constraints = track.getConstraints();
    Scale current = constraints.getScale();
    auto it = std::find(UIUtils::kScaleOrder.begin(), UIUtils::kScaleOrder.end(), current);
    if (it == UIUtils::kScaleOrder.end()) {
        sequencerScaleMenuIndex = 0;
    } else {
        sequencerScaleMenuIndex = static_cast<int>(std::distance(UIUtils::kScaleOrder.begin(), it));
    }
    sequencerScaleMenuActive = true;
}

void UI::finishSequencerScaleMenu(bool applySelection) {
    if (!sequencerScaleMenuActive) return;

    if (applySelection && sequencer && !UIUtils::kScaleOrder.empty()) {
        int clampedIndex = std::clamp(sequencerScaleMenuIndex, 0,
                                      static_cast<int>(UIUtils::kScaleOrder.size()) - 1);
        Track& track = sequencer->getCurrentTrack();
        MusicalConstraints& constraints = track.getConstraints();
        Scale chosen = UIUtils::kScaleOrder[clampedIndex];
        if (constraints.getScale() != chosen) {
            constraints.setScale(chosen);
            sequencer->regenerateUnlocked();
        }
    }

    sequencerScaleMenuActive = false;
}

void UI::handleSequencerScaleMenuInput(int ch) {
    if (!sequencerScaleMenuActive) return;

    int optionCount = static_cast<int>(UIUtils::kScaleOrder.size());
    if (optionCount <= 0) {
        finishSequencerScaleMenu(false);
        return;
    }

    switch (ch) {
        case KEY_UP:
        case 'k':
        case 'K':
            sequencerScaleMenuIndex =
                (sequencerScaleMenuIndex - 1 + optionCount) % optionCount;
            break;
        case KEY_DOWN:
        case 'j':
        case 'J':
            sequencerScaleMenuIndex =
                (sequencerScaleMenuIndex + 1) % optionCount;
            break;
        case '\n':
        case KEY_ENTER:
            finishSequencerScaleMenu(true);
            break;
        case 27:  // Escape
            finishSequencerScaleMenu(false);
            break;
        default:
            break;
    }
}

void UI::adjustSequencerTrackerField(int row, int column, int direction) {
    if (!sequencer) return;
    Pattern& pattern = sequencer->getPattern();
    if (row < 0 || row >= pattern.getLength()) return;

    PatternStep& step = pattern.getStep(row);
    SequencerTrackerColumn col = static_cast<SequencerTrackerColumn>(column);

    direction = (direction >= 0) ? 1 : -1;

    switch (col) {
        case SequencerTrackerColumn::LOCK: {
            step.locked = (direction > 0);
            break;
        }
        case SequencerTrackerColumn::NOTE: {
            step.active = true;
            int midi = std::clamp(step.midiNote + direction, 0, 127);
            step.midiNote = midi;
            break;
        }
        case SequencerTrackerColumn::VELOCITY: {
            step.active = true;
            int velocity = std::clamp(step.velocity + direction * 5, 1, 127);
            step.velocity = velocity;
            break;
        }
        case SequencerTrackerColumn::GATE: {
            step.active = true;
            float gate = step.gateLength + static_cast<float>(direction) * 0.05f;
            gate = std::max(0.05f, std::min(2.0f, gate));
            step.gateLength = gate;
            break;
        }
        case SequencerTrackerColumn::PROBABILITY: {
            step.active = true;
            float prob = step.probability + static_cast<float>(direction) * 0.05f;
            prob = std::max(0.0f, std::min(1.0f, prob));
            step.probability = prob;
            break;
        }
    }

    ensureSequencerSelectionInRange();
}

void UI::adjustSequencerInfoField(int infoIndex, int direction) {
    if (!sequencer) return;
    if (infoIndex < 0 || infoIndex >= static_cast<int>(kSequencerInfoEntries.size())) return;
    const auto& entry = kSequencerInfoEntries[infoIndex];
    if (!entry.editable) return;

    direction = (direction >= 0) ? 1 : -1;

    Track& track = sequencer->getCurrentTrack();
    Pattern& pattern = track.getPattern();
    MusicalConstraints& constraints = track.getConstraints();
    EuclideanPattern& euclidean = track.getEuclideanPattern();

    switch (entry.field) {
        case SequencerInfoField::TEMPO: {
            double tempo = sequencer->getTempo();
            tempo = std::clamp(tempo + direction * 1.0, 0.1, 999.0);
            sequencer->setTempo(tempo);
            break;
        }
        case SequencerInfoField::ROOT: {
            int root = constraints.getRootNote();
            root = (root + direction + 12) % 12;
            constraints.setRootNote(root);
            int gravityOctave = constraints.getOctaveMin();
            constraints.setGravityNote(gravityOctave * 12 + root);
            sequencer->regenerateUnlocked();
            break;
        }
        case SequencerInfoField::SCALE: {
            Scale current = constraints.getScale();
            auto it = std::find(UIUtils::kScaleOrder.begin(), UIUtils::kScaleOrder.end(), current);
            if (it == UIUtils::kScaleOrder.end()) it = UIUtils::kScaleOrder.begin();
            int index = static_cast<int>(std::distance(UIUtils::kScaleOrder.begin(), it));
            index = (index + direction + static_cast<int>(UIUtils::kScaleOrder.size())) % static_cast<int>(UIUtils::kScaleOrder.size());
            constraints.setScale(UIUtils::kScaleOrder[index]);
            sequencer->regenerateUnlocked();
            break;
        }
        case SequencerInfoField::EUCLID_HITS: {
            int hits = std::clamp(euclidean.getHits() + direction, 0, euclidean.getSteps());
            euclidean.setHits(hits);
            sequencer->regenerateUnlocked();
            break;
        }
        case SequencerInfoField::EUCLID_STEPS: {
            int steps = std::clamp(euclidean.getSteps() + direction, 1, 64);
            euclidean.setSteps(steps);
            pattern.setLength(steps);
            sequencerSelectedRow = std::min(sequencerSelectedRow, steps - 1);
            sequencer->regenerateUnlocked();
            break;
        }
        case SequencerInfoField::SUBDIVISION: {
            Subdivision current = pattern.getResolution();
            auto it = std::find(UIUtils::kSubdivisionOrder.begin(), UIUtils::kSubdivisionOrder.end(), current);
            if (it == UIUtils::kSubdivisionOrder.end()) it = UIUtils::kSubdivisionOrder.begin();
            int index = static_cast<int>(std::distance(UIUtils::kSubdivisionOrder.begin(), it));
            index = (index + direction + static_cast<int>(UIUtils::kSubdivisionOrder.size())) % static_cast<int>(UIUtils::kSubdivisionOrder.size());
            Subdivision newSubdiv = UIUtils::kSubdivisionOrder[index];
            track.setSubdivision(newSubdiv);
            break;
        }
        case SequencerInfoField::MUTATE_AMOUNT: {
            sequencerMutateAmount = std::clamp(sequencerMutateAmount + direction * 5.0f, 0.0f, 100.0f);
            break;
        }
        case SequencerInfoField::MUTED: {
            track.setMuted(!track.isMuted());
            break;
        }
        case SequencerInfoField::SOLO: {
            track.setSolo(!track.isSolo());
            break;
        }
        case SequencerInfoField::PHASE_SOURCE: {
            if (sequencer) {
                auto newDriver = sequencer->getCurrentTrackPhaseDriver();
                if (direction > 0) {
                    newDriver = Sequencer::PhaseDriver::MODULATION;
                } else if (direction < 0) {
                    newDriver = Sequencer::PhaseDriver::CLOCK;
                } else {
                    newDriver = (newDriver == Sequencer::PhaseDriver::CLOCK)
                        ? Sequencer::PhaseDriver::MODULATION
                        : Sequencer::PhaseDriver::CLOCK;
                }
                sequencer->setCurrentTrackPhaseDriver(newDriver);
            }
            break;
        }
        default:
            break;
    }

    ensureSequencerSelectionInRange();
}

void UI::startSequencerNumericInput(int row, int column) {
    if (!sequencer) return;
    sequencerNumericContext = SequencerNumericContext();
    sequencerNumericContext.row = row;
    sequencerNumericContext.column = column;

    SequencerTrackerColumn col = static_cast<SequencerTrackerColumn>(column);
    switch (col) {
        case SequencerTrackerColumn::NOTE:
            sequencerNumericContext.field = SequencerNumericField::NOTE;
            break;
        case SequencerTrackerColumn::VELOCITY:
            sequencerNumericContext.field = SequencerNumericField::VELOCITY;
            break;
        case SequencerTrackerColumn::GATE:
            sequencerNumericContext.field = SequencerNumericField::GATE;
            break;
        case SequencerTrackerColumn::PROBABILITY:
            sequencerNumericContext.field = SequencerNumericField::PROBABILITY;
            break;
        case SequencerTrackerColumn::LOCK:
        default:
            sequencerNumericContext.field = SequencerNumericField::NONE;
            break;
    }

    if (sequencerNumericContext.field == SequencerNumericField::NONE) {
        return;
    }

    numericInputActive = true;
    numericInputIsSequencer = true;
    numericInputBuffer.clear();
    selectedParameterId = -1;
}

void UI::startSequencerInfoNumericInput(int infoIndex) {
    if (!sequencer) return;
    if (infoIndex < 0 || infoIndex >= static_cast<int>(kSequencerInfoEntries.size())) return;
    const auto& entry = kSequencerInfoEntries[infoIndex];
    if (!entry.editable) return;

    sequencerNumericContext = SequencerNumericContext();
    sequencerNumericContext.infoIndex = infoIndex;

    switch (entry.field) {
        case SequencerInfoField::TEMPO:
            sequencerNumericContext.field = SequencerNumericField::TEMPO;
            break;
        case SequencerInfoField::SCALE:
            sequencerNumericContext.field = SequencerNumericField::SCALE;
            break;
        case SequencerInfoField::ROOT:
            sequencerNumericContext.field = SequencerNumericField::ROOT;
            break;
        case SequencerInfoField::EUCLID_HITS:
            sequencerNumericContext.field = SequencerNumericField::EUCLID_HITS;
            break;
        case SequencerInfoField::EUCLID_STEPS:
            sequencerNumericContext.field = SequencerNumericField::EUCLID_STEPS;
            break;
        case SequencerInfoField::SUBDIVISION:
            sequencerNumericContext.field = SequencerNumericField::SUBDIVISION;
            break;
        case SequencerInfoField::MUTATE_AMOUNT:
            sequencerNumericContext.field = SequencerNumericField::MUTATE_AMOUNT;
            break;
        case SequencerInfoField::MUTED:
            sequencerNumericContext.field = SequencerNumericField::MUTED;
            break;
        case SequencerInfoField::SOLO:
            sequencerNumericContext.field = SequencerNumericField::SOLO;
            break;
        case SequencerInfoField::PHASE_SOURCE:
            sequencerNumericContext.field = SequencerNumericField::NONE;
            break;
        default:
            sequencerNumericContext.field = SequencerNumericField::NONE;
            break;
    }

    if (sequencerNumericContext.field == SequencerNumericField::NONE) {
        return;
    }

    numericInputActive = true;
    numericInputIsSequencer = true;
    numericInputBuffer.clear();
    selectedParameterId = -1;
}

void UI::applySequencerNumericInput(const std::string& text) {
    if (!sequencer) return;

    auto trim = [](const std::string& in) {
        size_t start = 0;
        while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start]))) ++start;
        size_t end = in.size();
        while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1]))) --end;
        return in.substr(start, end - start);
    };

    std::string trimmed = trim(text);
    if (trimmed.empty()) return;

    Track& track = sequencer->getCurrentTrack();
    Pattern& pattern = track.getPattern();
    MusicalConstraints& constraints = track.getConstraints();
    EuclideanPattern& euclidean = track.getEuclideanPattern();

    switch (sequencerNumericContext.field) {
        case SequencerNumericField::NOTE: {
            int row = sequencerNumericContext.row;
            if (row < 0 || row >= pattern.getLength()) break;
            PatternStep& step = pattern.getStep(row);
            int midi = -1;
            if (UIUtils::parseNoteText(trimmed, midi)) {
                step.active = true;
                step.midiNote = std::clamp(midi, 0, 127);
            }
            break;
        }
        case SequencerNumericField::VELOCITY: {
            int row = sequencerNumericContext.row;
            if (row < 0 || row >= pattern.getLength()) break;
            PatternStep& step = pattern.getStep(row);
            try {
                int value = std::stoi(trimmed);
                value = std::clamp(value, 1, 127);
                step.active = true;
                step.velocity = value;
            } catch (...) {}
            break;
        }
        case SequencerNumericField::GATE: {
            int row = sequencerNumericContext.row;
            if (row < 0 || row >= pattern.getLength()) break;
            PatternStep& step = pattern.getStep(row);
            try {
                float value = std::stof(trimmed);
                if (value > 2.0f) {
                    value /= 100.0f;
                }
                value = std::max(0.05f, std::min(2.0f, value));
                step.active = true;
                step.gateLength = value;
            } catch (...) {}
            break;
        }
        case SequencerNumericField::PROBABILITY: {
            int row = sequencerNumericContext.row;
            if (row < 0 || row >= pattern.getLength()) break;
            PatternStep& step = pattern.getStep(row);
            try {
                float value = std::stof(trimmed);
                if (value > 1.0f) {
                    value /= 100.0f;
                }
                value = std::max(0.0f, std::min(1.0f, value));
                step.active = true;
                step.probability = value;
            } catch (...) {}
            break;
        }
        case SequencerNumericField::TEMPO: {
            try {
                double tempo = std::stod(trimmed);
                tempo = std::clamp(tempo, 0.1, 999.0);
                sequencer->setTempo(tempo);
            } catch (...) {}
            break;
        }
        case SequencerNumericField::SCALE: {
            Scale newScale;
            if (UIUtils::parseScaleText(trimmed, newScale)) {
                constraints.setScale(newScale);
                sequencer->regenerateUnlocked();
            }
            break;
        }
        case SequencerNumericField::ROOT: {
            int root;
            if (UIUtils::parseRootText(trimmed, root)) {
                constraints.setRootNote(root);
                int gravityOctave = constraints.getOctaveMin();
                constraints.setGravityNote(gravityOctave * 12 + root);
                sequencer->regenerateUnlocked();
            }
            break;
        }
        case SequencerNumericField::EUCLID_HITS: {
            try {
                int hits = std::stoi(trimmed);
                hits = std::clamp(hits, 0, euclidean.getSteps());
                euclidean.setHits(hits);
                sequencer->regenerateUnlocked();
            } catch (...) {}
            break;
        }
        case SequencerNumericField::EUCLID_STEPS: {
            try {
                int steps = std::stoi(trimmed);
                steps = std::clamp(steps, 1, 64);
                euclidean.setSteps(steps);
                pattern.setLength(steps);
                sequencerSelectedRow = std::min(sequencerSelectedRow, steps - 1);
                sequencer->regenerateUnlocked();
            } catch (...) {}
            break;
        }
        case SequencerNumericField::EUCLID_ROTATION: {
            try {
                int rot = std::stoi(trimmed);
                euclidean.setRotation(rot);
            } catch (...) {}
            break;
        }
        case SequencerNumericField::SUBDIVISION: {
            Subdivision subdiv;
            if (UIUtils::parseSubdivisionText(trimmed, subdiv)) {
                track.setSubdivision(subdiv);
            }
            break;
        }
        case SequencerNumericField::MUTATE_AMOUNT: {
            try {
                float value = std::stof(trimmed);
                sequencerMutateAmount = std::clamp(value, 0.0f, 100.0f);
            } catch (...) {}
            break;
        }
        case SequencerNumericField::MUTED: {
            std::string lower;
            lower.reserve(trimmed.size());
            for (char c : trimmed) {
                if (!std::isspace(static_cast<unsigned char>(c))) {
                    lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                }
            }
            if (lower == "1" || lower == "on" || lower == "yes" || lower == "true") {
                track.setMuted(true);
            } else if (lower == "0" || lower == "off" || lower == "no" || lower == "false") {
                track.setMuted(false);
            }
            break;
        }
        case SequencerNumericField::SOLO: {
            std::string lower;
            lower.reserve(trimmed.size());
            for (char c : trimmed) {
                if (!std::isspace(static_cast<unsigned char>(c))) {
                    lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                }
            }
            if (lower == "1" || lower == "on" || lower == "yes" || lower == "true") {
                track.setSolo(true);
            } else if (lower == "0" || lower == "off" || lower == "no" || lower == "false") {
                track.setSolo(false);
            }
            break;
        }
        default:
            break;
    }

    ensureSequencerSelectionInRange();
}
