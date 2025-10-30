#include "../../ui.h"
#include <string>
#include <vector>
#include <algorithm>

void UI::drawParameterList(int startRow, int startCol, const std::vector<int>& paramIds) {
    int row = startRow;
    int col = startCol;

    for (int paramId : paramIds) {
        InlineParameter* param = getParameter(paramId);
        if (!param) continue;

        // Check if parameter is modulated
        bool isModulated = isParameterModulated(paramId);

        // Check if parameter has MIDI CC mapping
        int mappedCC = -1;
        if (paramId >= 0 && paramId < SynthParameters::kMaxParamMap) {
            mappedCC = params->parameterCCMap[paramId].load();
        }

        // Highlight selected parameter with cursor
        bool isSelected = (paramId == selectedParameterId);
        if (isSelected) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(row, col, ">");
            attroff(COLOR_PAIR(5) | A_BOLD);
        } else {
            mvprintw(row, col, " ");
        }

        // Color priority: Locked (red) > Modulated (green), but selection highlight persists
        bool locked = !param->randomizable;
        if (isSelected) {
            attron(A_REVERSE);  // Use reverse video for selection
        }
        if (locked) {
            attron(COLOR_PAIR(4));  // Red for locked
        } else if (isModulated) {
            attron(COLOR_PAIR(2));  // Green for modulated
        }

        // Parameter name and value
        std::string displayValue = getParameterDisplayString(paramId);
        mvprintw(row, col + 2, "%-18s: %s", param->name.c_str(), displayValue.c_str());

        // Show lock and CC indicators
        int indicCol = col + 36;
        if (locked) {
            attron(COLOR_PAIR(4));
            mvprintw(row, indicCol, "[LOCK]");
            attroff(COLOR_PAIR(4));
            indicCol += 7;
        }
        if (mappedCC >= 0) {
            attron(COLOR_PAIR(3));
            mvprintw(row, indicCol, "[CC%d]", mappedCC);
            attroff(COLOR_PAIR(3));
        }

        if (locked) {
            attroff(COLOR_PAIR(4));
        } else if (isModulated) {
            attroff(COLOR_PAIR(2));
        }
        if (isSelected) {
            attroff(A_REVERSE);
        }

        row++;
    }

    // MIDI Learn status is now shown as a centered popup (see ui_drawing.cpp)
}

void UI::drawParametersPage(int startRow, int startCol) {
    drawParameterList(startRow, startCol, getParameterIdsForPage(currentPage));
}
