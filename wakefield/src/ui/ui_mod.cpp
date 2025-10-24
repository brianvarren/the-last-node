#include "../ui.h"
#include <algorithm>
#include <vector>
#include <string>
#include "ui_mod_data.h"

void UI::startModMatrixMenu() {
    modMatrixMenuColumn = modMatrixCursorCol;
    modMatrixMenuActive = true;

    const ModulationSlot& slot = modulationSlots[modMatrixCursorRow];

    if (modMatrixMenuColumn == 0) {
        modMatrixMenuIndex = (slot.source >= 0) ? slot.source : 0;
    } else if (modMatrixMenuColumn == 1) {
        modMatrixMenuIndex = (slot.curve >= 0) ? slot.curve : 0;
    } else if (modMatrixMenuColumn == 4) {
        modMatrixMenuIndex = (slot.type >= 0) ? slot.type : 0;
    } else if (modMatrixMenuColumn == 3) {
        const auto& modules = getModDestinationModules();
        if (!modules.empty()) {
            int moduleIndex = 0;
            int paramIndex = 0;
            if (slot.destination >= 0) {
                getModuleParamFromDestinationIndex(slot.destination, moduleIndex, paramIndex);
            }
            moduleIndex = std::clamp(moduleIndex, 0, static_cast<int>(modules.size()) - 1);
            int paramCount = static_cast<int>(modules[moduleIndex].options.size());
            if (paramCount == 0) {
                paramIndex = 0;
            } else {
                paramIndex = std::clamp(paramIndex, 0, paramCount - 1);
            }
            modMatrixDestinationModuleIndex = moduleIndex;
            modMatrixDestinationParamIndex = paramIndex;
            modMatrixDestinationFocusColumn = (paramCount > 0) ? 1 : 0; // default focus
        } else {
            modMatrixDestinationModuleIndex = 0;
            modMatrixDestinationParamIndex = 0;
            modMatrixDestinationFocusColumn = 1;
        }
    } else {
        modMatrixMenuIndex = 0;
    }
}

void UI::handleModMatrixMenuInput(int ch) {
    if (!modMatrixMenuActive) return;

    if (modMatrixMenuColumn == 3) {
        const auto& modules = getModDestinationModules();
        if (modules.empty()) {
            finishModMatrixMenu(false);
            return;
        }

        auto clampModule = [&]() {
            modMatrixDestinationModuleIndex = std::clamp(
                modMatrixDestinationModuleIndex, 0, static_cast<int>(modules.size()) - 1);
            int paramCount = static_cast<int>(modules[modMatrixDestinationModuleIndex].options.size());
            if (paramCount == 0) {
                modMatrixDestinationParamIndex = 0;
            } else {
                modMatrixDestinationParamIndex = std::clamp(
                    modMatrixDestinationParamIndex, 0, paramCount - 1);
            }
        };

        clampModule();

        const auto& params = modules[modMatrixDestinationModuleIndex].options;
        int paramCount = static_cast<int>(params.size());

        switch (ch) {
            case KEY_LEFT:
            case 'h':
            case 'H':
                modMatrixDestinationFocusColumn = 0;
                break;
            case KEY_RIGHT:
            case 'l':
            case 'L':
                if (paramCount > 0) {
                    modMatrixDestinationFocusColumn = 1;
                }
                break;
            case KEY_UP:
            case 'k':
            case 'K':
                if (modMatrixDestinationFocusColumn == 0) {
                    modMatrixDestinationModuleIndex =
                        (modMatrixDestinationModuleIndex - 1 + modules.size()) % modules.size();
                    clampModule();
                } else if (paramCount > 0) {
                    modMatrixDestinationParamIndex =
                        (modMatrixDestinationParamIndex - 1 + paramCount) % paramCount;
                }
                break;
            case KEY_DOWN:
            case 'j':
            case 'J':
                if (modMatrixDestinationFocusColumn == 0) {
                    modMatrixDestinationModuleIndex =
                        (modMatrixDestinationModuleIndex + 1) % static_cast<int>(modules.size());
                    clampModule();
                } else if (paramCount > 0) {
                    modMatrixDestinationParamIndex =
                        (modMatrixDestinationParamIndex + 1) % paramCount;
                }
                break;
            case '\n':
            case KEY_ENTER:
                if (paramCount > 0) {
                    finishModMatrixMenu(true);
                }
                break;
            case 27:  // Escape
            case 'q':
            case 'Q':
                finishModMatrixMenu(false);
                break;
            default:
                break;
        }
        return;
    }

    // Get the appropriate option list for the current column
    const std::vector<ModOption>* options = nullptr;
    if (modMatrixMenuColumn == 0) {
        options = &getModSourceOptions();
    } else if (modMatrixMenuColumn == 1) {
        options = &getModCurveOptions();
    } else if (modMatrixMenuColumn == 4) {
        options = &getModTypeOptions();
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
            int destinationIndex = getDestinationIndexFromModule(
                modMatrixDestinationModuleIndex, modMatrixDestinationParamIndex);
            if (destinationIndex >= 0) {
                slot.destination = destinationIndex;
            }
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
