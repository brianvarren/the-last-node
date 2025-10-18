#include "../../ui.h"
#include <algorithm>
#include <cmath>

void UI::drawFMPage() {
    int row = 2;

    // Title
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row, 2, "FM MATRIX");
    attroff(COLOR_PAIR(1) | A_BOLD);
    row += 3;

    // Column labels (targets)
    mvprintw(row, 10, "TARGET");
    row++;
    const int cellStartCol = 16;
    const int cellStride = 4;
    mvprintw(row, cellStartCol, "  1   2   3   4");
    row += 2;

    // Draw matrix grid (4x4)
    const char* sourceLabels[] = {"OSC1", "OSC2", "OSC3", "OSC4"};

    for (int source = 0; source < 4; ++source) {
        // Row label
        mvprintw(row, 4, "%s", sourceLabels[source]);

        // Draw cells for each target
        for (int target = 0; target < 4; ++target) {
            int cellCol = cellStartCol + (target * cellStride);

            // Get FM depth value
            float depth = params->getFMDepth(target, source);
            int depthPercent = static_cast<int>(std::round(depth * 100.0f));
            depthPercent = std::max(-99, std::min(99, depthPercent));

            // Check if this cell is selected
            bool isSelected = (fmMatrixCursorRow == source && fmMatrixCursorCol == target);

            bool isNonZero = (depthPercent != 0);

            if (isNonZero) {
                attron(COLOR_PAIR(5));
            }
            if (isSelected) {
                attron(A_REVERSE);
            }

            mvprintw(row, cellCol, "%3d", depthPercent);

            if (isSelected) {
                attroff(A_REVERSE);
            }
            if (isNonZero) {
                attroff(COLOR_PAIR(5));
            }
        }
        row++;
    }
}
