#include "../../ui.h"
#include <algorithm>
#include <string>

void UI::drawFMPage() {
    int row = 3;

    // Title
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row, 2, "FM MATRIX - Audio-Rate Frequency Modulation");
    attroff(COLOR_PAIR(1) | A_BOLD);
    row += 2;

    // Instructions
    attron(COLOR_PAIR(6));
    mvprintw(row, 2, "Arrow keys: navigate | +/- or Left/Right: adjust depth | Enter: direct entry");
    attroff(COLOR_PAIR(6));
    row += 2;

    // Draw matrix header
    attron(COLOR_PAIR(1));
    mvprintw(row, 2, "     TARGET ->");
    attroff(COLOR_PAIR(1));
    row++;

    // Column labels (targets)
    mvprintw(row, 16, "OSC1");
    mvprintw(row, 24, "OSC2");
    mvprintw(row, 32, "OSC3");
    mvprintw(row, 40, "OSC4");
    row++;

    // Draw horizontal separator
    attron(COLOR_PAIR(6));
    mvprintw(row, 2, "SOURCE");
    for (int i = 14; i < 48; ++i) {
        mvprintw(row, i, "-");
    }
    attroff(COLOR_PAIR(6));
    row++;

    // Draw matrix grid (4x4)
    const char* sourceLabels[] = {"OSC1", "OSC2", "OSC3", "OSC4"};

    for (int source = 0; source < 4; ++source) {
        // Row label
        attron(COLOR_PAIR(1));
        mvprintw(row, 4, "%s", sourceLabels[source]);
        attroff(COLOR_PAIR(1));

        // Draw cells for each target
        for (int target = 0; target < 4; ++target) {
            int cellCol = 14 + (target * 8);

            // Get FM depth value
            float depth = params->getFMDepth(target, source);
            int depthPercent = static_cast<int>(depth * 100.0f);

            // Check if this cell is selected
            bool isSelected = (fmMatrixCursorRow == source && fmMatrixCursorCol == target);

            // Self-modulation is disabled (diagonal)
            bool isSelfMod = (source == target);

            if (isSelfMod) {
                // Gray out diagonal (no self-modulation)
                attron(COLOR_PAIR(6) | A_DIM);
                mvprintw(row, cellCol, "[ -- ]");
                attroff(COLOR_PAIR(6) | A_DIM);
            } else {
                // Determine display style based on depth and selection
                if (isSelected) {
                    attron(COLOR_PAIR(5) | A_BOLD);  // Cyan highlight for selected
                } else if (depthPercent == 0) {
                    attron(COLOR_PAIR(6) | A_DIM);  // Dim for zero depth
                } else if (depthPercent > 50) {
                    attron(A_BOLD);  // Bold for high depth
                } else {
                    attron(A_NORMAL);  // Normal for moderate depth
                }

                mvprintw(row, cellCol, "[%3d%%]", depthPercent);

                if (isSelected) {
                    attroff(COLOR_PAIR(5) | A_BOLD);
                } else {
                    attroff(A_BOLD | A_DIM | COLOR_PAIR(6));
                }
            }
        }
        row++;
    }

    row += 2;

    // Show current selection details
    if (fmMatrixCursorRow >= 0 && fmMatrixCursorRow < 4 &&
        fmMatrixCursorCol >= 0 && fmMatrixCursorCol < 4 &&
        fmMatrixCursorRow != fmMatrixCursorCol) {

        float depth = params->getFMDepth(fmMatrixCursorCol, fmMatrixCursorRow);
        int depthPercent = static_cast<int>(depth * 100.0f);

        attron(COLOR_PAIR(1));
        mvprintw(row, 2, "Selected: OSC%d modulates OSC%d at %d%% depth",
                 fmMatrixCursorRow + 1, fmMatrixCursorCol + 1, depthPercent);
        attroff(COLOR_PAIR(1));
        row++;

        // Show what this means
        if (depthPercent > 0) {
            attron(COLOR_PAIR(6));
            mvprintw(row, 2, "OSC%d's output controls OSC%d's frequency (Through-Zero FM)",
                     fmMatrixCursorRow + 1, fmMatrixCursorCol + 1);
            attroff(COLOR_PAIR(6));
        } else {
            attron(COLOR_PAIR(6) | A_DIM);
            mvprintw(row, 2, "No modulation - adjust depth to enable FM");
            attroff(COLOR_PAIR(6) | A_DIM);
        }
    }

    row += 2;

    // Tips section
    attron(COLOR_PAIR(6));
    mvprintw(row, 2, "TIPS:");
    row++;
    mvprintw(row, 4, "- Diagonal cells are disabled (no self-modulation)");
    row++;
    mvprintw(row, 4, "- FM is audio-rate (very fast modulation)");
    row++;
    mvprintw(row, 4, "- Through-Zero FM allows negative frequencies (phase reversal)");
    row++;
    mvprintw(row, 4, "- Stack multiple FM sources for complex timbres");
    attroff(COLOR_PAIR(6));
}
