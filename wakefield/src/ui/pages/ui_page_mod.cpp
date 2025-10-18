#include "../../ui.h"
#include <algorithm>
#include <string>

void UI::drawModPage() {
    int row = 3;

    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row, 2, "MODULATION MATRIX");
    attroff(COLOR_PAIR(1) | A_BOLD);
    row += 2;

    mvprintw(row, 2, "Slot  Source        Curve      Amount      Destination        Type");
    row++;
    mvhline(row, 2, '-', 72);
    row += 2;

    for (int slot = 0; slot < 16; ++slot) {
        mvprintw(row++, 2, "%02d    --            --         --          --                --", slot + 1);
    }

    row += 1;
    attron(COLOR_PAIR(6));
    mvprintw(row++, 2, "Mod routing UI coming soon. Configure FM routing on the FM page.");
    attroff(COLOR_PAIR(6));
}
