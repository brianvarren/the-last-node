#include "../../ui.h"
#include "../../sequencer.h"
#include "../../loop_manager.h"
#include "pattern.h"
#include "track.h"
#include "constraint.h"
#include <algorithm>

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
    {UI::SequencerInfoField::LOCKED_COUNT,"Locked",       false}
};

void UI::ensureSequencerSelectionInRange() {
    if (!sequencer) return;

    Pattern& pattern = sequencer->getPattern();
    int maxRows = std::min(16, pattern.getLength());
    if (maxRows <= 0) {
        maxRows = 1;
    }

    if (sequencerSelectedRow < 0) {
        sequencerSelectedRow = maxRows - 1;
    } else if (sequencerSelectedRow >= maxRows) {
        sequencerSelectedRow = 0;
    }

    int maxColumn = static_cast<int>(SequencerTrackerColumn::PROBABILITY);
    if (sequencerSelectedColumn < static_cast<int>(SequencerTrackerColumn::LOCK)) {
        sequencerSelectedColumn = static_cast<int>(SequencerTrackerColumn::LOCK);
    } else if (sequencerSelectedColumn > maxColumn) {
        sequencerSelectedColumn = maxColumn;
    }

    if (sequencerRightSelection < 0) {
        sequencerRightSelection = 0;
    }
    if (!kSequencerInfoEntries.empty() && sequencerRightSelection >= static_cast<int>(kSequencerInfoEntries.size())) {
        sequencerRightSelection = static_cast<int>(kSequencerInfoEntries.size()) - 1;
    }
}

void UI::cancelSequencerNumericInput() {
    numericInputIsSequencer = false;
    sequencerNumericContext = SequencerNumericContext();
}
