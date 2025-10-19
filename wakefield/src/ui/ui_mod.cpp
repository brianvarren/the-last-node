#include "../ui.h"
#include <vector>
#include <string>

namespace {

// MOD matrix menu option lists
const std::vector<std::string> kModSources = {
    "LFO 1", "LFO 2", "LFO 3", "LFO 4",
    "ENV 1", "ENV 2", "ENV 3", "ENV 4",
    "Velocity", "Aftertouch", "Mod Wheel", "Pitch Bend"
};

const std::vector<std::string> kModCurves = {
    "Linear", "Exponential", "Logarithmic", "S-Curve"
};

const std::vector<std::string> kModDestinations = {
    "OSC 1 Pitch", "OSC 1 Morph", "OSC 1 Level",
    "OSC 2 Pitch", "OSC 2 Morph", "OSC 2 Level",
    "OSC 3 Pitch", "OSC 3 Morph", "OSC 3 Level",
    "OSC 4 Pitch", "OSC 4 Morph", "OSC 4 Level",
    "Filter Cutoff", "Filter Resonance",
    "Reverb Mix", "Reverb Size"
};

const std::vector<std::string> kModTypes = {
    "Bipolar", "Unipolar"
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
    const std::vector<std::string>* options = nullptr;
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
        // TODO: Apply the selected value to the modulation matrix
        // For now, this is a placeholder until the actual mod matrix backend is implemented
    }

    modMatrixMenuActive = false;
}

void UI::startModMatrixAmountInput() {
    numericInputActive = true;
    numericInputIsMod = true;
    numericInputBuffer.clear();
}

void UI::finishModMatrixAmountInput() {
    if (!numericInputActive || !numericInputIsMod) return;

    if (!numericInputBuffer.empty()) {
        try {
            int amount = std::stoi(numericInputBuffer);
            amount = std::max(-99, std::min(99, amount));  // Clamp to -99..99
            // TODO: Apply amount to modulation matrix
        } catch (...) {
            // Invalid input, ignore
        }
    }

    numericInputActive = false;
    numericInputIsMod = false;
    numericInputBuffer.clear();
}
