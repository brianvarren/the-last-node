#include "../../ui.h"
#include <algorithm>
#include <cmath>

void UI::drawFMPage() {
    int row = 2;

    // Title
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row, 2, "FM MATRIX - Audio-Rate Phase Modulation (16 sources × 8 targets)");
    attroff(COLOR_PAIR(1) | A_BOLD);
    row += 2;

    // Column labels (targets) with clear abbreviations
    mvprintw(row, 2, "SOURCE");
    attron(COLOR_PAIR(1));
    mvprintw(row, 12, "TARGETS:");
    attroff(COLOR_PAIR(1));
    row++;
    const int cellStartCol = 12;
    const int cellStride = 5;
    attron(COLOR_PAIR(1));
    mvprintw(row, cellStartCol, " O1  O2  O3  O4  S1  S2  S3  S4");
    attroff(COLOR_PAIR(1));
    row += 2;

    // Draw matrix grid (16 sources × 8 targets)
    const char* sourceLabels[] = {
        "OSC1", "OSC2", "OSC3", "OSC4",
        "SMP1", "SMP2", "SMP3", "SMP4",
        "C1X ", "C1Y ", "C2X ", "C2Y ",
        "C3X ", "C3Y ", "C4X ", "C4Y "
    };

    for (int source = 0; source < 16; ++source) {
        // Row label
        if (source == 8) {
            // Visual separator before chaos sources
            attron(COLOR_PAIR(1));
            mvhline(row, 0, '-', 80);
            attroff(COLOR_PAIR(1));
            row++;
        }

        mvprintw(row, 6, "%s", sourceLabels[source]);

        // Draw cells for each target
        for (int target = 0; target < 8; ++target) {
            int cellCol = cellStartCol + (target * cellStride);

            // Get FM depth value
            float depth = params->getFMDepth(target, source);
            int depthPercent = static_cast<int>(std::round(depth * 100.0f));
            depthPercent = std::max(-99, std::min(99, depthPercent));

            // Check if this cell is selected
            bool isSelected = (fmMatrixCursorRow == source && fmMatrixCursorCol == target);

            bool isNonZero = (depthPercent != 0);

            bool isLocked = fmMatrixLocked[target][source];

            if (isLocked) {
                attron(COLOR_PAIR(4));
            } else if (isNonZero) {
                attron(COLOR_PAIR(5));
            }
            if (isSelected) {
                attron(A_REVERSE);
            }

            if (isLocked) {
                mvprintw(row, cellCol, "%3dL", depthPercent);
            } else {
                mvprintw(row, cellCol, "%4d", depthPercent);
            }

            if (isSelected) {
                attroff(A_REVERSE);
            }
            if (isLocked) {
                attroff(COLOR_PAIR(4));
            } else if (isNonZero) {
                attroff(COLOR_PAIR(5));
            }
        }
        row++;
    }

    // Legend
    attron(COLOR_PAIR(3));
    mvprintw(row + 1, 2, "Legend: red = locked (immune to G/M/R)  |  'l' toggles lock");
    attroff(COLOR_PAIR(3));
}
