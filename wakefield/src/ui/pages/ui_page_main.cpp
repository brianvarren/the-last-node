#include "../../ui.h"
#include <string>
#include <vector>

void UI::drawMainPage(int activeVoices) {
    int row = 3;
    std::vector<int> pageParams = getParameterIdsForPage(currentPage);

    // Draw parameters inline with left/right control indicators
    for (int paramId : pageParams) {
        InlineParameter* param = getParameter(paramId);
        if (!param) continue;

        // Highlight selected parameter
        if (paramId == selectedParameterId) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(row, 2, ">");
        } else {
            mvprintw(row, 2, " ");
        }

        // Parameter name and value
        std::string displayValue = getParameterDisplayString(paramId);
        mvprintw(row, 4, "%-15s: %s", param->name.c_str(), displayValue.c_str());

        // Show arrows for selected parameter
        if (paramId == selectedParameterId) {
            mvprintw(row, 40, "< >");
            attroff(COLOR_PAIR(5) | A_BOLD);
        }

        row++;
    }

    row += 2;

    // Voice info
    attron(A_BOLD);
    mvprintw(row++, 2, "VOICES");
    attroff(A_BOLD);
    mvprintw(row++, 2, "Active: %d/8", activeVoices);
}
