#include "../../ui.h"
#include "../ui_utils.h"
#include "../../sequencer.h"
#include "../../loop_manager.h"
#include "pattern.h"
#include "track.h"
#include "constraint.h"
#include <algorithm>
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

void UI::drawSequencerPage() {
    if (!sequencer) {
        mvprintw(3, 2, "Sequencer not initialized");
        return;
    }

    ensureSequencerSelectionInRange();

    int leftCol = 2;
    int row = 3;

    Track& track = sequencer->getCurrentTrack();
    Pattern& pattern = track.getPattern();
    MusicalConstraints& constraints = track.getConstraints();
    EuclideanPattern& euclidean = track.getEuclideanPattern();

    int trackIdx = sequencer->getCurrentTrackIndex();
    double tempo = sequencer->getTempo();
    bool playing = sequencer->isPlaying();
    int patternLength = std::max(1, pattern.getLength());
    int currentStep = sequencer->getCurrentStep(trackIdx);
    currentStep = (currentStep % patternLength + patternLength) % patternLength;

    int rightCol = leftCol + 35;  // Place right pane closer to tracker

    // Status indicators at top
    int statusAttr = playing ? (COLOR_PAIR(2) | A_BOLD) : (COLOR_PAIR(4) | A_BOLD);
    attron(statusAttr);
    mvprintw(row, leftCol, "[%s]", playing ? "PLAYING" : "STOPPED");
    attroff(statusAttr);
    mvprintw(row, leftCol + 12, "Step: %d/%d", currentStep + 1, patternLength);
    row++;

    attron(A_BOLD);
    mvprintw(row, leftCol, "SEQUENCER - Track %d", trackIdx + 1);
    attroff(A_BOLD);
    row += 2;

    // Draw actions section FIRST at top of right pane
    int actionsStartRow = 3;
    attron(A_BOLD);
    mvprintw(actionsStartRow, rightCol, "ACTIONS");
    attroff(A_BOLD);
    actionsStartRow++;

    // Generate and Clear buttons
    bool generateSelected = sequencerFocusActionsPane && sequencerActionsRow == 0;
    if (generateSelected) {
        attron(COLOR_PAIR(5) | A_BOLD);
    }
    mvprintw(actionsStartRow, rightCol, "[Generate]");
    if (generateSelected) {
        attroff(COLOR_PAIR(5) | A_BOLD);
    }

    bool clearSelected = sequencerFocusActionsPane && sequencerActionsRow == 1;
    if (clearSelected) {
        attron(COLOR_PAIR(5) | A_BOLD);
    }
    mvprintw(actionsStartRow, rightCol + 12, "[Clear]");
    if (clearSelected) {
        attroff(COLOR_PAIR(5) | A_BOLD);
    }
    actionsStartRow += 2;

    // Actions grid header
    mvprintw(actionsStartRow, rightCol, "            All  Note Vel  Gate Prob");
    actionsStartRow++;

    // Actions grid rows (Randomize, Mutate, Rotate only)
    const char* actionLabels[] = {"Randomize", "Mutate   ", "Rotate   "};
    for (int aRow = 0; aRow < 3; ++aRow) {
        mvprintw(actionsStartRow, rightCol, "%-10s", actionLabels[aRow]);
        for (int aCol = 0; aCol < 5; ++aCol) {
            bool cellSelected = sequencerFocusActionsPane &&
                                sequencerActionsRow == (aRow + 2) &&
                                sequencerActionsColumn == aCol;
            int xPos = rightCol + 12 + aCol * 5;
            if (cellSelected) {
                attron(COLOR_PAIR(5) | A_BOLD);
            }
            mvprintw(actionsStartRow, xPos, "[ ]");
            if (cellSelected) {
                attroff(COLOR_PAIR(5) | A_BOLD);
            }
        }
        actionsStartRow++;
    }

    int infoRow = actionsStartRow + 1;
    attron(A_BOLD);
    mvprintw(infoRow, rightCol, "PARAMETERS");
    attroff(A_BOLD);
    infoRow += 2;

    // Draw right-side info panel
    struct InfoValue {
        const SequencerInfoEntryDef* def;
        std::string text;
        int attr;
        bool highlight;
    };

    std::vector<InfoValue> infoValues;
    infoValues.reserve(kSequencerInfoEntries.size());

    auto pushInfo = [&](const SequencerInfoEntryDef& def, const std::string& value, int attr, bool highlight) {
        infoValues.push_back({&def, value, attr, highlight});
    };

    for (size_t i = 0; i < kSequencerInfoEntries.size(); ++i) {
        const auto& def = kSequencerInfoEntries[i];
        bool highlight = sequencerFocusRightPane && sequencerRightSelection == static_cast<int>(i);
        int attr = 0;
        std::string text;

        switch (def.field) {
            case SequencerInfoField::TEMPO: {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.1f BPM", tempo);
                text = buf;
                break;
            }
            case SequencerInfoField::ROOT: {
                text = UIUtils::rootNoteToString(constraints.getRootNote());
                break;
            }
            case SequencerInfoField::SCALE: {
                // Extract scale name without root
                std::string fullName = constraints.getScaleName();
                size_t spacePos = fullName.find(' ');
                if (spacePos != std::string::npos) {
                    text = fullName.substr(spacePos + 1);  // Skip "Root " part
                } else {
                    text = fullName;
                }
                break;
            }
            case SequencerInfoField::EUCLID_HITS: {
                text = std::to_string(euclidean.getHits());
                break;
            }
            case SequencerInfoField::EUCLID_STEPS: {
                text = std::to_string(euclidean.getSteps());
                break;
            }
            case SequencerInfoField::SUBDIVISION: {
                text = UIUtils::subdivisionToString(pattern.getResolution());
                break;
            }
            case SequencerInfoField::MUTATE_AMOUNT: {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.0f%%", sequencerMutateAmount);
                text = buf;
                break;
            }
            case SequencerInfoField::MUTED: {
                bool muted = track.isMuted();
                text = muted ? "YES" : "NO";
                attr = muted ? (COLOR_PAIR(4) | A_BOLD) : 0;
                break;
            }
            case SequencerInfoField::SOLO: {
                bool solo = track.isSolo();
                text = solo ? "YES" : "NO";
                attr = solo ? (COLOR_PAIR(3) | A_BOLD) : 0;
                break;
            }
            case SequencerInfoField::ACTIVE_COUNT: {
                text = std::to_string(pattern.getActiveStepCount());
                break;
            }
            case SequencerInfoField::PHASE_SOURCE: {
                Sequencer::PhaseDriver mode = sequencer->getTrackPhaseDriver(trackIdx);
                bool usingMod = (mode == Sequencer::PhaseDriver::MODULATION);
                text = usingMod ? "Modulation" : "Clock";
                attr = usingMod ? (COLOR_PAIR(3) | A_BOLD) : 0;
                break;
            }
            default:
                text.clear();
                break;
        }

        pushInfo(def, text, attr, highlight);
    }

    for (const auto& info : infoValues) {
        int attr = info.highlight ? (COLOR_PAIR(5) | A_BOLD) : info.attr;
        if (attr != 0) {
            attron(attr);
        }
        mvprintw(infoRow, rightCol, "%-12s: %s", info.def->label, info.text.c_str());
        if (attr != 0) {
            attroff(attr);
        }
        ++infoRow;
    }

    // Simple tracker display - cap at 16 steps
    int displayRows = std::min(16, patternLength);

    // Draw header
    attron(A_BOLD);
    mvprintw(row, leftCol, " # L Note   Vel Gate Prob");
    attroff(A_BOLD);
    row++;

    // Draw separator
    mvprintw(row, leftCol, "----------------------------");
    row++;

    // Draw each step
    for (int i = 0; i < displayRows; ++i) {
        const PatternStep& step = pattern.getStep(i);
        bool rowSelected = (!sequencerFocusRightPane && !sequencerFocusActionsPane && sequencerSelectedRow == i);
        bool isCurrentStep = (currentStep == i);

        // Reset attributes and clear the line
        attrset(A_NORMAL);
        mvhline(row, leftCol, ' ', 30);  // Clear 30 chars for the row
        move(row, leftCol);

        // Apply cyan color to entire row if it's the current step
        if (isCurrentStep) {
            attron(COLOR_PAIR(1) | A_BOLD);
        }

        // Step number (1-based)
        mvprintw(row, leftCol, "%2d", i + 1);

        // Lock indicator - with selection highlight
        bool lockSelected = rowSelected && sequencerSelectedColumn == static_cast<int>(SequencerTrackerColumn::LOCK);
        if (!isCurrentStep && lockSelected) {
            attroff(COLOR_PAIR(1) | A_BOLD);  // Turn off cyan if needed
            attron(COLOR_PAIR(5) | A_BOLD);
        }
        if (step.locked) {
            mvprintw(row, leftCol + 3, "L");
        } else {
            mvprintw(row, leftCol + 3, " ");
        }
        if (!isCurrentStep && lockSelected) {
            attroff(COLOR_PAIR(5) | A_BOLD);
            attron(COLOR_PAIR(1) | A_BOLD);  // Restore cyan if needed
        }

        // Note field
        bool noteSelected = rowSelected && sequencerSelectedColumn == static_cast<int>(SequencerTrackerColumn::NOTE);
        if (!isCurrentStep && noteSelected) {
            attroff(COLOR_PAIR(1) | A_BOLD);
            attron(COLOR_PAIR(5) | A_BOLD);
        }
        if (step.active) {
            mvprintw(row, leftCol + 5, "%-6s", UIUtils::midiNoteToString(step.midiNote).c_str());
        } else {
            mvprintw(row, leftCol + 5, "---   ");
        }
        if (!isCurrentStep && noteSelected) {
            attroff(COLOR_PAIR(5) | A_BOLD);
            attron(COLOR_PAIR(1) | A_BOLD);
        }

        // Velocity field
        bool velSelected = rowSelected && sequencerSelectedColumn == static_cast<int>(SequencerTrackerColumn::VELOCITY);
        if (!isCurrentStep && velSelected) {
            attroff(COLOR_PAIR(1) | A_BOLD);
            attron(COLOR_PAIR(5) | A_BOLD);
        }
        if (step.active) {
            mvprintw(row, leftCol + 12, "%3d", step.velocity);
        } else {
            mvprintw(row, leftCol + 12, "---");
        }
        if (!isCurrentStep && velSelected) {
            attroff(COLOR_PAIR(5) | A_BOLD);
            attron(COLOR_PAIR(1) | A_BOLD);
        }

        // Gate field
        bool gateSelected = rowSelected && sequencerSelectedColumn == static_cast<int>(SequencerTrackerColumn::GATE);
        if (!isCurrentStep && gateSelected) {
            attroff(COLOR_PAIR(1) | A_BOLD);
            attron(COLOR_PAIR(5) | A_BOLD);
        }
        if (step.active) {
            mvprintw(row, leftCol + 16, "%3d%%", static_cast<int>(step.gateLength * 100.0f));
        } else {
            mvprintw(row, leftCol + 16, "--- ");
        }
        if (!isCurrentStep && gateSelected) {
            attroff(COLOR_PAIR(5) | A_BOLD);
            attron(COLOR_PAIR(1) | A_BOLD);
        }

        // Probability field
        bool probSelected = rowSelected && sequencerSelectedColumn == static_cast<int>(SequencerTrackerColumn::PROBABILITY);
        if (!isCurrentStep && probSelected) {
            attroff(COLOR_PAIR(1) | A_BOLD);
            attron(COLOR_PAIR(5) | A_BOLD);
        }
        if (step.active) {
            mvprintw(row, leftCol + 21, "%3d%%", static_cast<int>(step.probability * 100.0f));
        } else {
            mvprintw(row, leftCol + 21, "--- ");
        }
        if (!isCurrentStep && probSelected) {
            attroff(COLOR_PAIR(5) | A_BOLD);
            attron(COLOR_PAIR(1) | A_BOLD);
        }

        // Reset attributes at end of row
        attrset(A_NORMAL);

        row++;
    }
}
