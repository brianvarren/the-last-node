#include "../../ui.h"
#include "../../sequencer.h"
#include "pattern.h"
#include "track.h"
#include <algorithm>

// External references to global objects from main.cpp
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

bool UI::handleSequencerInput(int ch) {
    if (!sequencer) {
        return false;
    }

    Pattern& pattern = sequencer->getPattern();
    int patternLength = pattern.getLength();
    if (patternLength <= 0) {
        return false;
    }

    ensureSequencerSelectionInRange();

    int maxRows = patternLength;
    if (maxRows <= 0) {
        maxRows = 1;
    }

    auto wrapIndex = [](int value, int max) {
        if (max <= 0) return 0;
        if (value < 0) {
            value = (value % max + max) % max;
        } else if (value >= max) {
            value %= max;
        }
        return value;
    };

    switch (ch) {
        case KEY_SR:
        case KEY_UP:
            if (sequencerFocusActionsPane) {
                if (sequencerActionsRow > 0) {
                    --sequencerActionsRow;
                }
            } else if (sequencerFocusRightPane) {
                if (sequencerRightSelection > 0) {
                    --sequencerRightSelection;
                } else {
                    // Move from parameters back to actions (bottom row)
                    sequencerFocusRightPane = false;
                    sequencerFocusActionsPane = true;
                    sequencerActionsRow = 4;   // Rotate row
                    sequencerActionsColumn = std::clamp(sequencerActionsColumn, 0, 4);
                }
            } else {
                sequencerSelectedRow = wrapIndex(sequencerSelectedRow - 1, maxRows);
                const int visibleRows = 16;
                if (sequencerSelectedRow < sequencerScrollOffset) {
                    sequencerScrollOffset = std::max(0, sequencerSelectedRow);
                } else if (sequencerSelectedRow >= sequencerScrollOffset + visibleRows) {
                    sequencerScrollOffset = std::max(0, sequencerSelectedRow - visibleRows + 1);
                }
            }
            return true;

        case KEY_SF:
        case KEY_DOWN:
            if (sequencerFocusActionsPane) {
                if (sequencerActionsRow < 4) {
                    ++sequencerActionsRow;
                } else {
                    // Move from actions to first parameter
                    if (!kSequencerInfoEntries.empty()) {
                        sequencerFocusActionsPane = false;
                        sequencerFocusRightPane = true;
                        sequencerRightSelection = 0;
                    }
                }
            } else if (sequencerFocusRightPane) {
                if (sequencerRightSelection < static_cast<int>(kSequencerInfoEntries.size()) - 1) {
                    ++sequencerRightSelection;
                }
            } else {
                sequencerSelectedRow = wrapIndex(sequencerSelectedRow + 1, maxRows);
                const int visibleRows = 16;
                if (sequencerSelectedRow < sequencerScrollOffset) {
                    sequencerScrollOffset = std::max(0, sequencerSelectedRow);
                } else if (sequencerSelectedRow >= sequencerScrollOffset + visibleRows) {
                    sequencerScrollOffset = std::max(0, sequencerSelectedRow - visibleRows + 1);
                }
            }
            return true;

        case KEY_SLEFT:
        case KEY_LEFT:
            if (sequencerFocusActionsPane) {
                if (sequencerActionsRow >= 2 && sequencerActionsColumn > 0) {
                    --sequencerActionsColumn;
                } else if (sequencerActionsRow == 1 && sequencerActionsColumn > 0) {
                    --sequencerActionsColumn;
                } else {
                    // Move from actions to tracker
                    sequencerFocusActionsPane = false;
                }
            } else if (sequencerFocusRightPane) {
                sequencerFocusRightPane = false;
            } else if (sequencerSelectedColumn > static_cast<int>(SequencerTrackerColumn::LOCK)) {
                --sequencerSelectedColumn;
            }
            return true;

        case KEY_SRIGHT:
        case KEY_RIGHT:
            if (sequencerFocusActionsPane) {
                if (sequencerActionsRow >= 2 && sequencerActionsColumn < 4) {
                    ++sequencerActionsColumn;
                } else if (sequencerActionsRow == 0) {
                    // Only one button on row 0
                } else if (sequencerActionsRow == 1) {
                    // Toggle between [Clear] and [Lock All]
                    sequencerActionsColumn = std::min(1, sequencerActionsColumn + 1);
                }
            } else if (!sequencerFocusRightPane) {
                if (sequencerSelectedColumn < static_cast<int>(SequencerTrackerColumn::PROBABILITY)) {
                    ++sequencerSelectedColumn;
                } else {
                    sequencerFocusActionsPane = true;
                    sequencerFocusRightPane = false;
                    sequencerActionsRow = 0;
                    sequencerActionsColumn = 0;
                }
            }
            return true;

        case '+':
        case '=': {
            if (sequencerFocusActionsPane) {
                // No adjustment in actions pane
            } else if (sequencerFocusRightPane) {
                adjustSequencerInfoField(sequencerRightSelection, 1, ch == '+');
            } else {
                adjustSequencerTrackerField(sequencerSelectedRow, sequencerSelectedColumn, 1, ch == '+');
            }
            return true;
        }

        case '-':
        case '_': {
            if (sequencerFocusActionsPane) {
                // No adjustment in actions pane
            } else if (sequencerFocusRightPane) {
                adjustSequencerInfoField(sequencerRightSelection, -1, ch == '_');
            } else {
                adjustSequencerTrackerField(sequencerSelectedRow, sequencerSelectedColumn, -1, ch == '_');
            }
            return true;
        }

        case '\n':
        case KEY_ENTER:
            if (sequencerFocusActionsPane) {
                executeSequencerAction(sequencerActionsRow, sequencerActionsColumn);
            } else if (sequencerFocusRightPane) {
                if (sequencerRightSelection >= 0 && sequencerRightSelection < static_cast<int>(kSequencerInfoEntries.size())) {
                    const auto& entry = kSequencerInfoEntries[sequencerRightSelection];
                    if (entry.editable) {
                        // Boolean fields toggle directly without input
                        if (entry.field == SequencerInfoField::MUTED) {
                            Track& track = sequencer->getCurrentTrack();
                            track.setMuted(!track.isMuted());
                        } else if (entry.field == SequencerInfoField::SOLO) {
                            Track& track = sequencer->getCurrentTrack();
                            track.setSolo(!track.isSolo());
                        } else if (entry.field == SequencerInfoField::PHASE_SOURCE) {
                            auto mode = sequencer->getCurrentTrackPhaseDriver();
                            auto nextMode = (mode == Sequencer::PhaseDriver::CLOCK)
                                ? Sequencer::PhaseDriver::MODULATION
                                : Sequencer::PhaseDriver::CLOCK;
                            sequencer->setCurrentTrackPhaseDriver(nextMode);
                        } else if (entry.field == SequencerInfoField::SCALE) {
                            startSequencerScaleMenu();
                        } else {
                            startSequencerInfoNumericInput(sequencerRightSelection);
                        }
                    }
                }
            } else {
                if (sequencerSelectedColumn == static_cast<int>(SequencerTrackerColumn::LOCK)) {
                    PatternStep& step = pattern.getStep(sequencerSelectedRow);
                    step.locked = !step.locked;
                } else {
                    startSequencerNumericInput(sequencerSelectedRow, sequencerSelectedColumn);
                }
            }
            return true;

        case KEY_PPAGE: { // Page Up: scroll up view
            const int visibleRows = 16;
            sequencerScrollOffset = std::max(0, sequencerScrollOffset - visibleRows);
            return true;
        }
        case KEY_NPAGE: { // Page Down: scroll down view
            const int visibleRows = 16;
            sequencerScrollOffset = std::min(std::max(0, patternLength - visibleRows), sequencerScrollOffset + visibleRows);
            return true;
        }

        default:
            break;
    }

    return false;
}
