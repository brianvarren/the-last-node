#include "../../ui.h"
#include <algorithm>
#include <cmath>

void UI::drawFMPage() {
    int row = 2;

    // Title
    attron(COLOR_PAGE_TITLE | A_BOLD);
    mvprintw(row, 2, "FM MATRIX - Audio-Rate Phase Modulation (8 sources × 8 targets)");
    attroff(COLOR_PAGE_TITLE | A_BOLD);
    // Actions (right side): Lock All
    // Square brackets signify a button across pages
    int maxX = getmaxx(stdscr);
    int actionX = std::max(2, maxX - 14); // place near right edge
    if (fmFocusArea == 1) {
        attron(COLOR_SELECTION | A_BOLD);
        mvprintw(row, actionX, "[Lock All]");
        attroff(COLOR_SELECTION | A_BOLD);
    } else {
        mvprintw(row, actionX, "[Lock All]");
    }
    row += 2;

    // Column labels (targets) with clear abbreviations
    mvprintw(row, 2, "SOURCE");
    const int cellStartCol = 12;
    const int cellStride = 6; // widen cells for clearer labels/values
    const char* targetLabels[] = {"OSC1","OSC2","OSC3","OSC4","SMP1","SMP2","SMP3","SMP4"};
    // Draw header "TARGETS:" and individual labels aligned with cells
    attron(COLOR_SECTION_HEADER);
    mvprintw(row, cellStartCol, "TARGETS:");
    attroff(COLOR_SECTION_HEADER);
    row++;
    attron(COLOR_SECTION_HEADER);
    for (int target = 0; target < kFMTargetCount; ++target) {
        int labelCol = cellStartCol + target * cellStride;
        mvprintw(row, labelCol, "%5s", targetLabels[target]);
    }
    attroff(COLOR_SECTION_HEADER);
    row += 2;

    // Draw matrix grid (8 sources × 8 targets)
    const char* sourceLabels[] = {
        "OSC1", "OSC2", "OSC3", "OSC4",
        "SMP1", "SMP2", "SMP3", "SMP4"
    };

    for (int source = 0; source < 8; ++source) {
        // Row label
        mvprintw(row, 6, "%s", sourceLabels[source]);

        // Draw cells for each target
        for (int target = 0; target < kFMTargetCount; ++target) {
            int cellCol = cellStartCol + (target * cellStride);

            // Get FM depth value
            float depth = params->getFMDepth(target, source);
            int depthPercent = static_cast<int>(std::round(depth * 100.0f));
            depthPercent = std::max(-99, std::min(99, depthPercent));

            // Check if this cell is selected (only when matrix has focus)
            bool isSelected = (fmFocusArea == 0 && fmMatrixCursorRow == source && fmMatrixCursorCol == target);

            bool isNonZero = (depthPercent != 0);

            bool isLocked = fmMatrixLocked[target][source];

            // Selection uses standard highlight; otherwise show status/lock colors
            if (isSelected) {
                attron(COLOR_SELECTION | A_BOLD);
            } else if (isLocked) {
                attron(COLOR_LOCKED);
            } else if (isNonZero) {
                attron(COLOR_STATUS_ACTIVE);
            }

            if (isLocked) {
                mvprintw(row, cellCol, "%4dL", depthPercent);
            } else {
                mvprintw(row, cellCol, "%5d", depthPercent);
            }

            if (isSelected) {
                attroff(COLOR_SELECTION | A_BOLD);
            } else if (isLocked) {
                attroff(COLOR_LOCKED);
            } else if (isNonZero) {
                attroff(COLOR_STATUS_ACTIVE);
            }
        }
        row++;
    }

    // Global Depth control (standalone parameter below matrix)
    row += 1;
    float gdepth = params ? params->fmGlobalDepth.load() : 0.0f;
    drawBar(row, 2, "Global Depth", gdepth, 0.0f, 1.0f, 40);
    // Overlay highlight on the label when focused
    if (fmFocusArea == 2) {
        attron(COLOR_SELECTION | A_BOLD);
        mvprintw(row, 2, "Global Depth");
        attroff(COLOR_SELECTION | A_BOLD);
    }

    // Legend / hints
    attron(COLOR_HINT);
    mvprintw(row + 2, 2, "Legend: red = locked (immune to G/M/R)  |  'l' toggles cell lock  |  'K' = Lock All");
    attroff(COLOR_HINT);
}
