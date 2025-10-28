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
        if (paramId >= 0 && paramId < 50) {
            mappedCC = params->parameterCCMap[paramId].load();
        }

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

        // Show MIDI CC mapping indicator if mapped
        if (mappedCC >= 0) {
            attron(COLOR_PAIR(3));  // Yellow
            mvprintw(row, col + 36, "[CC%d]", mappedCC);
            attroff(COLOR_PAIR(3));
        }

        if (isModulated) {
            attroff(COLOR_PAIR(2));
        }

        if (paramId == selectedParameterId) {
            attroff(COLOR_PAIR(5) | A_BOLD);
        }

        row++;
    }

    // Show MIDI Learn status if active
    if (params->midiLearnActive.load()) {
        int learnParamId = params->midiLearnParameterId.load();
        // Only show if learning for a parameter in this list
        if (std::find(paramIds.begin(), paramIds.end(), learnParamId) != paramIds.end()) {
            row += 1;  // Add spacing
            attron(COLOR_PAIR(3) | A_BOLD);
            mvprintw(row++, col, ">>> MIDI LEARN ACTIVE <<<");
            attroff(COLOR_PAIR(3) | A_BOLD);

            InlineParameter* learnParam = getParameter(learnParamId);
            if (learnParam) {
                mvprintw(row++, col, "Move a MIDI controller to assign");
                mvprintw(row++, col, "it to: %s", learnParam->name.c_str());
            }
        }
    }
}

void UI::drawParametersPage(int startRow, int startCol) {
    drawParameterList(startRow, startCol, getParameterIdsForPage(currentPage));
}
