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
        bool locked = !param->randomizable;

        if (isSelected) {
            attron(COLOR_SELECTION | A_BOLD);
            mvprintw(row, col, ">");
        } else {
            mvprintw(row, col, " ");
        }

        // Apply color based on state, but selection (white-on-blue) overrides all
        if (isSelected) {
            // Keep COLOR_SELECTION active for entire line (white on blue)
        } else if (locked) {
            attron(COLOR_LOCKED);  // Red for locked
        } else if (isModulated) {
            attron(COLOR_MODULATED);  // Green for modulated
        }

        // Parameter name and value
        std::string displayValue = getParameterDisplayString(paramId);
        mvprintw(row, col + 2, "%-18s: %s", param->name.c_str(), displayValue.c_str());

        // Show lock and CC indicators (keep in their own colors even when selected)
        int indicCol = col + 36;
        if (locked) {
            if (isSelected) attroff(COLOR_SELECTION | A_BOLD);
            attron(COLOR_LOCKED);
            mvprintw(row, indicCol, "[LOCK]");
            attroff(COLOR_LOCKED);
            if (isSelected) attron(COLOR_SELECTION | A_BOLD);
            indicCol += 7;
        }
        if (mappedCC >= 0) {
            if (isSelected) attroff(COLOR_SELECTION | A_BOLD);
            attron(COLOR_MIDI_MAPPED);
            mvprintw(row, indicCol, "[CC%d]", mappedCC);
            attroff(COLOR_MIDI_MAPPED);
            if (isSelected) attron(COLOR_SELECTION | A_BOLD);
        }

        // Turn off colors at end of line
        if (isSelected) {
            attroff(COLOR_SELECTION | A_BOLD);
        } else if (locked) {
            attroff(COLOR_LOCKED);
        } else if (isModulated) {
            attroff(COLOR_MODULATED);
        }

        row++;
    }

    // MIDI Learn status is now shown as a centered popup (see ui_drawing.cpp)
}

void UI::drawParametersPage(int startRow, int startCol) {
    drawParameterList(startRow, startCol, getParameterIdsForPage(currentPage));
}
