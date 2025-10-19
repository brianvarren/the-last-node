#include "../../ui.h"
#include <algorithm>
#include <string>
#include <vector>

namespace {

struct ModOption {
    const char* displayName;
    const char* symbol;
};

// MOD matrix menu option lists (same as in ui_mod.cpp)
const std::vector<ModOption> kModSources = {
    {"LFO 1", "LFO1"}, {"LFO 2", "LFO2"}, {"LFO 3", "LFO3"}, {"LFO 4", "LFO4"},
    {"ENV 1", "ENV1"}, {"ENV 2", "ENV2"}, {"ENV 3", "ENV3"}, {"ENV 4", "ENV4"},
    {"Velocity", "Vel"}, {"Aftertouch", "AT"}, {"Mod Wheel", "MW"}, {"Pitch Bend", "PB"}
};

const std::vector<ModOption> kModCurves = {
    {"Linear", "Lin"}, {"Exponential", "Exp"}, {"Logarithmic", "Log"}, {"S-Curve", "S"}
};

const std::vector<ModOption> kModDestinations = {
    {"OSC 1 Pitch", "O1:Pitch"}, {"OSC 1 Morph", "O1:Morph"}, {"OSC 1 Level", "O1:Level"},
    {"OSC 2 Pitch", "O2:Pitch"}, {"OSC 2 Morph", "O2:Morph"}, {"OSC 2 Level", "O2:Level"},
    {"OSC 3 Pitch", "O3:Pitch"}, {"OSC 3 Morph", "O3:Morph"}, {"OSC 3 Level", "O3:Level"},
    {"OSC 4 Pitch", "O4:Pitch"}, {"OSC 4 Morph", "O4:Morph"}, {"OSC 4 Level", "O4:Level"},
    {"Filter Cutoff", "Flt:Cut"}, {"Filter Resonance", "Flt:Res"},
    {"Reverb Mix", "Rvb:Mix"}, {"Reverb Size", "Rvb:Size"}
};

const std::vector<ModOption> kModTypes = {
    {"Unidirectional", "-->"}, {"Bidirectional", "<->"}
};

} // namespace

void UI::drawModPage() {
    static const char* headers[] = {"Slot", "Source", "Curve", "Amount", "Destination", "Type"};
    static const int headerCols[] = {2, 10, 28, 40, 56, 74};
    static const int colWidths[] = {4, 16, 10, 12, 18, 8};
    constexpr int slotCount = 16;
    constexpr int columnCount = 5;  // Source..Type

    int row = 3;

    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row, 2, "MODULATION MATRIX");
    attroff(COLOR_PAIR(1) | A_BOLD);
    row += 2;

    for (int h = 0; h < 6; ++h) {
        attron(COLOR_PAIR(1));
        mvprintw(row, headerCols[h], "%s", headers[h]);
        attroff(COLOR_PAIR(1));
    }
    row++;
    mvhline(row, 2, '-', 78);
    row += 2;

    for (int slot = 0; slot < slotCount; ++slot) {
        mvprintw(row, headerCols[0], "%02d", slot + 1);

        const ModulationSlot& modSlot = modulationSlots[slot];
        std::string cellValues[columnCount];

        // Format cell values based on stored data
        cellValues[0] = (modSlot.source >= 0 && modSlot.source < kModSources.size())
                        ? kModSources[modSlot.source].symbol : "--";
        cellValues[1] = (modSlot.curve >= 0 && modSlot.curve < kModCurves.size())
                        ? kModCurves[modSlot.curve].symbol : "--";
        cellValues[2] = (modSlot.amount != 0 || modSlot.isComplete())
                        ? std::to_string(static_cast<int>(modSlot.amount)) : "--";
        cellValues[3] = (modSlot.destination >= 0 && modSlot.destination < kModDestinations.size())
                        ? kModDestinations[modSlot.destination].symbol : "--";
        cellValues[4] = (modSlot.type >= 0 && modSlot.type < kModTypes.size())
                        ? kModTypes[modSlot.type].symbol : "--";

        for (int col = 0; col < columnCount; ++col) {
            bool selected = (slot == modMatrixCursorRow && col == modMatrixCursorCol);
            if (selected) {
                attron(A_REVERSE);
            }
            mvprintw(row, headerCols[col + 1], "%-*s", colWidths[col + 1], cellValues[col].c_str());
            if (selected) {
                attroff(A_REVERSE);
            }
        }
        row++;
    }

    row += 1;
    attron(COLOR_PAIR(6));
    mvprintw(row++, 2, "Arrow keys navigate. Enter to select. Esc to cancel.");
    attroff(COLOR_PAIR(6));

    // Draw selection menu if active
    if (modMatrixMenuActive) {
        const std::vector<ModOption>* options = nullptr;
        const char* title = "";

        if (modMatrixMenuColumn == 0) {
            options = &kModSources;
            title = "Select Source";
        } else if (modMatrixMenuColumn == 1) {
            options = &kModCurves;
            title = "Select Curve";
        } else if (modMatrixMenuColumn == 3) {
            options = &kModDestinations;
            title = "Select Destination";
        } else if (modMatrixMenuColumn == 4) {
            options = &kModTypes;
            title = "Select Type";
        }

        if (options && !options->empty()) {
            int menuWidth = 30;
            int menuHeight = std::min(12, static_cast<int>(options->size()) + 2);
            int menuX = 25;
            int menuY = 8;

            // Draw menu box
            attron(COLOR_PAIR(1) | A_BOLD);
            mvprintw(menuY, menuX, "+%s+", std::string(menuWidth - 2, '-').c_str());
            mvprintw(menuY + 1, menuX, "| %-*s |", menuWidth - 4, title);
            mvhline(menuY + 2, menuX + 1, '-', menuWidth - 2);
            attroff(COLOR_PAIR(1) | A_BOLD);

            // Draw menu options
            int firstOption = std::max(0, modMatrixMenuIndex - (menuHeight - 4) / 2);
            int lastOption = std::min(static_cast<int>(options->size()), firstOption + menuHeight - 3);

            for (int i = firstOption; i < lastOption; ++i) {
                int optRow = menuY + 3 + (i - firstOption);
                if (i == modMatrixMenuIndex) {
                    attron(A_REVERSE);
                }
                mvprintw(optRow, menuX, "| %-*s |", menuWidth - 4, (*options)[i].displayName);
                if (i == modMatrixMenuIndex) {
                    attroff(A_REVERSE);
                }
            }

            // Draw bottom of box
            attron(COLOR_PAIR(1));
            mvprintw(menuY + menuHeight - 1, menuX, "+%s+", std::string(menuWidth - 2, '-').c_str());
            attroff(COLOR_PAIR(1));
        }
    }
}
