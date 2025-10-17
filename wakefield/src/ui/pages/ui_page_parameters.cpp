#include "../../ui.h"
#include <string>
#include <vector>

void UI::drawParametersPage() {
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
        mvprintw(row, 4, "%-18s: %s", param->name.c_str(), displayValue.c_str());

        // Show control hints for selected parameter
        if (paramId == selectedParameterId) {
            mvprintw(row, 45, "← →");
            if (param->supports_midi_learn) {
                mvprintw(row, 50, "[L]");
            }
            mvprintw(row, 55, "[Enter]");
            attroff(COLOR_PAIR(5) | A_BOLD);
        }

        row++;
    }
}
