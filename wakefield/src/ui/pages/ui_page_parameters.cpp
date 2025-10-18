#include "../../ui.h"
#include <string>
#include <vector>

void UI::drawParametersPage(int startRow, int startCol) {
    int row = startRow;
    int col = startCol;
    std::vector<int> pageParams = getParameterIdsForPage(currentPage);

    // Draw parameters inline with left/right control indicators
    for (int paramId : pageParams) {
        InlineParameter* param = getParameter(paramId);
        if (!param) continue;

        // Highlight selected parameter
        if (paramId == selectedParameterId) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(row, col, ">");
        } else {
            mvprintw(row, col, " ");
        }

        // Parameter name and value
        std::string displayValue = getParameterDisplayString(paramId);
        mvprintw(row, col + 2, "%-18s: %s", param->name.c_str(), displayValue.c_str());

        if (paramId == selectedParameterId) {
            attroff(COLOR_PAIR(5) | A_BOLD);
        }

        row++;
    }
}
