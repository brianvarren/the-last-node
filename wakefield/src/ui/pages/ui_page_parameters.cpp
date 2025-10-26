#include "../../ui.h"
#include <string>
#include <vector>

void UI::drawParameterList(int startRow, int startCol, const std::vector<int>& paramIds) {
    int row = startRow;
    int col = startCol;

    for (int paramId : paramIds) {
        InlineParameter* param = getParameter(paramId);
        if (!param) continue;

        // Check if parameter is modulated
        bool isModulated = isParameterModulated(paramId);

        // Highlight selected parameter
        if (paramId == selectedParameterId) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(row, col, ">");
        } else {
            mvprintw(row, col, " ");
        }

        // Use green color for modulated parameters
        if (isModulated) {
            attron(COLOR_PAIR(2));  // Green
        }

        // Parameter name and value
        std::string displayValue = getParameterDisplayString(paramId);
        mvprintw(row, col + 2, "%-18s: %s", param->name.c_str(), displayValue.c_str());

        if (isModulated) {
            attroff(COLOR_PAIR(2));
        }

        if (paramId == selectedParameterId) {
            attroff(COLOR_PAIR(5) | A_BOLD);
        }

        row++;
    }
}

void UI::drawParametersPage(int startRow, int startCol) {
    drawParameterList(startRow, startCol, getParameterIdsForPage(currentPage));
}
