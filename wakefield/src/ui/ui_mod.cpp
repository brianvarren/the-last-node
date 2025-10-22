#include "../ui.h"
#include <vector>
#include <string>

namespace {

struct ModOption {
    const char* displayName;  // Full name for menu
    const char* symbol;       // Abbreviated symbol for cell
};

// MOD matrix menu option lists with abbreviated symbols
const std::vector<ModOption> kModSources = {
    {"LFO 1", "LFO1"}, {"LFO 2", "LFO2"}, {"LFO 3", "LFO3"}, {"LFO 4", "LFO4"},
    {"ENV 1", "ENV1"}, {"ENV 2", "ENV2"}, {"ENV 3", "ENV3"}, {"ENV 4", "ENV4"},
    {"Velocity", "Vel"}, {"Aftertouch", "AT"}, {"Mod Wheel", "MW"}, {"Pitch Bend", "PB"}
};

const std::vector<ModOption> kModCurves = {
    {"Linear", "Lin"}, {"Exponential", "Exp"}, {"Logarithmic", "Log"}, {"S-Curve", "S"}
};

const std::vector<ModOption> kModDestinations = {
    {"OSC 1 Pitch", "O1:Pitch"}, {"OSC 1 Morph", "O1:Morph"}, {"OSC 1 Duty", "O1:Duty"},
    {"OSC 1 Ratio", "O1:Ratio"}, {"OSC 1 Offset", "O1:Offset"}, {"OSC 1 Level", "O1:Level"},
    {"OSC 2 Pitch", "O2:Pitch"}, {"OSC 2 Morph", "O2:Morph"}, {"OSC 2 Duty", "O2:Duty"},
    {"OSC 2 Ratio", "O2:Ratio"}, {"OSC 2 Offset", "O2:Offset"}, {"OSC 2 Level", "O2:Level"},
    {"OSC 3 Pitch", "O3:Pitch"}, {"OSC 3 Morph", "O3:Morph"}, {"OSC 3 Duty", "O3:Duty"},
    {"OSC 3 Ratio", "O3:Ratio"}, {"OSC 3 Offset", "O3:Offset"}, {"OSC 3 Level", "O3:Level"},
    {"OSC 4 Pitch", "O4:Pitch"}, {"OSC 4 Morph", "O4:Morph"}, {"OSC 4 Duty", "O4:Duty"},
    {"OSC 4 Ratio", "O4:Ratio"}, {"OSC 4 Offset", "O4:Offset"}, {"OSC 4 Level", "O4:Level"},
    {"Filter Cutoff", "Flt:Cut"}, {"Filter Resonance", "Flt:Res"},
    {"Reverb Mix", "Rvb:Mix"}, {"Reverb Size", "Rvb:Size"},
    {"SAMP 1 Pitch", "S1:Pitch"}, {"SAMP 1 Loop Start", "S1:LpSt"}, {"SAMP 1 Loop Length", "S1:LpLen"},
    {"SAMP 1 Crossfade", "S1:XFd"}, {"SAMP 1 Level", "S1:Level"},
    {"SAMP 2 Pitch", "S2:Pitch"}, {"SAMP 2 Loop Start", "S2:LpSt"}, {"SAMP 2 Loop Length", "S2:LpLen"},
    {"SAMP 2 Crossfade", "S2:XFd"}, {"SAMP 2 Level", "S2:Level"},
    {"SAMP 3 Pitch", "S3:Pitch"}, {"SAMP 3 Loop Start", "S3:LpSt"}, {"SAMP 3 Loop Length", "S3:LpLen"},
    {"SAMP 3 Crossfade", "S3:XFd"}, {"SAMP 3 Level", "S3:Level"},
    {"SAMP 4 Pitch", "S4:Pitch"}, {"SAMP 4 Loop Start", "S4:LpSt"}, {"SAMP 4 Loop Length", "S4:LpLen"},
    {"SAMP 4 Crossfade", "S4:XFd"}, {"SAMP 4 Level", "S4:Level"}
};

const std::vector<ModOption> kModTypes = {
    {"Unidirectional", "-->"}, {"Bidirectional", "<->"}
};

} // namespace

void UI::startModMatrixMenu() {
    modMatrixMenuColumn = modMatrixCursorCol;
    modMatrixMenuIndex = 0;  // Start at first option
    modMatrixMenuActive = true;
}

void UI::handleModMatrixMenuInput(int ch) {
    if (!modMatrixMenuActive) return;

    // Get the appropriate option list for the current column
    const std::vector<ModOption>* options = nullptr;
    if (modMatrixMenuColumn == 0) {
        options = &kModSources;
    } else if (modMatrixMenuColumn == 1) {
        options = &kModCurves;
    } else if (modMatrixMenuColumn == 3) {
        options = &kModDestinations;
    } else if (modMatrixMenuColumn == 4) {
        options = &kModTypes;
    }

    if (!options || options->empty()) {
        finishModMatrixMenu(false);
        return;
    }

    int optionCount = static_cast<int>(options->size());

    switch (ch) {
        case KEY_UP:
        case 'k':
        case 'K':
            modMatrixMenuIndex = (modMatrixMenuIndex - 1 + optionCount) % optionCount;
            break;
        case KEY_DOWN:
        case 'j':
        case 'J':
            modMatrixMenuIndex = (modMatrixMenuIndex + 1) % optionCount;
            break;
        case '\n':
        case KEY_ENTER:
            finishModMatrixMenu(true);
            break;
        case 27:  // Escape
        case 'q':
        case 'Q':
            finishModMatrixMenu(false);
            break;
        default:
            break;
    }
}

void UI::finishModMatrixMenu(bool applySelection) {
    if (!modMatrixMenuActive) return;

    if (applySelection) {
        ModulationSlot& slot = modulationSlots[modMatrixCursorRow];

        // Apply the selected value to the appropriate field
        if (modMatrixMenuColumn == 0) {
            slot.source = modMatrixMenuIndex;
        } else if (modMatrixMenuColumn == 1) {
            slot.curve = modMatrixMenuIndex;
        } else if (modMatrixMenuColumn == 3) {
            slot.destination = modMatrixMenuIndex;
        } else if (modMatrixMenuColumn == 4) {
            slot.type = modMatrixMenuIndex;
        }

        modMatrixMenuActive = false;

        // Check if slot is still incomplete and advance to next empty field ("bus" workflow)
        if (!slot.isComplete()) {
            // Find next empty field in order: Source -> Curve -> Amount -> Dest -> Type
            if (slot.source == -1) {
                modMatrixCursorCol = 0;
                startModMatrixMenu();
                return;
            } else if (slot.curve == -1) {
                modMatrixCursorCol = 1;
                startModMatrixMenu();
                return;
            } else if (slot.amount == 0 && modMatrixMenuColumn != 2) {
                modMatrixCursorCol = 2;
                startModMatrixAmountInput();
                return;
            } else if (slot.destination == -1) {
                modMatrixCursorCol = 3;
                startModMatrixMenu();
                return;
            } else if (slot.type == -1) {
                modMatrixCursorCol = 4;
                startModMatrixMenu();
                return;
            }
        }
    } else {
        modMatrixMenuActive = false;
    }
}

void UI::startModMatrixAmountInput() {
    numericInputActive = true;
    numericInputIsMod = true;
    numericInputBuffer.clear();
}

void UI::finishModMatrixAmountInput() {
    if (!numericInputActive || !numericInputIsMod) return;

    ModulationSlot& slot = modulationSlots[modMatrixCursorRow];

    if (!numericInputBuffer.empty()) {
        try {
            int amount = std::stoi(numericInputBuffer);
            slot.amount = std::max(-99, std::min(99, amount));  // Clamp to -99..99
        } catch (...) {
            // Invalid input, use default of 0
            slot.amount = 0;
        }
    }

    numericInputActive = false;
    numericInputIsMod = false;
    numericInputBuffer.clear();

    // Continue bus workflow if slot still incomplete
    if (!slot.isComplete()) {
        if (slot.source == -1) {
            modMatrixCursorCol = 0;
            startModMatrixMenu();
            return;
        } else if (slot.curve == -1) {
            modMatrixCursorCol = 1;
            startModMatrixMenu();
            return;
        } else if (slot.destination == -1) {
            modMatrixCursorCol = 3;
            startModMatrixMenu();
            return;
        } else if (slot.type == -1) {
            modMatrixCursorCol = 4;
            startModMatrixMenu();
            return;
        }
    }
}
