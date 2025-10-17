#include "../../ui.h"

void UI::drawLFOPage() {
    // Draw LFO selector at top
    int row = 3;
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row, 2, "Select LFO (1-4): ");
    attroff(COLOR_PAIR(1) | A_BOLD);

    // Draw LFO buttons
    for (int i = 0; i < 4; ++i) {
        if (i == currentLFOIndex) {
            attron(COLOR_PAIR(5) | A_BOLD);  // Highlight selected
            mvprintw(row, 21 + (i * 4), "[%d]", i + 1);
            attroff(COLOR_PAIR(5) | A_BOLD);
        } else {
            attron(COLOR_PAIR(6));
            mvprintw(row, 21 + (i * 4), " %d ", i + 1);
            attroff(COLOR_PAIR(6));
        }
    }

    row += 2;
    attron(COLOR_PAIR(1));
    mvprintw(row, 2, "LFO %d Parameters:", currentLFOIndex + 1);
    attroff(COLOR_PAIR(1));

    // Draw parameters for the currently selected LFO
    drawParametersPage(row + 2);
}
