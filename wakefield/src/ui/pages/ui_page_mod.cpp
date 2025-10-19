#include "../../ui.h"
#include <algorithm>
#include <string>
#include <vector>

namespace {

// MOD matrix menu option lists (same as in ui_mod.cpp)
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

        const char* placeholders[columnCount] = {"--", "--", "--", "--", "--"};
        for (int col = 0; col < columnCount; ++col) {
            bool selected = (slot == modMatrixCursorRow && col == modMatrixCursorCol);
            if (selected) {
                attron(A_REVERSE);  // Just reverse video, no color
            }
            mvprintw(row, headerCols[col + 1], "%-*s", colWidths[col + 1], placeholders[col]);
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
        const std::vector<std::string>* options = nullptr;
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
                mvprintw(optRow, menuX, "| %-*s |", menuWidth - 4, (*options)[i].c_str());
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
