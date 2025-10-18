#include "../../ui.h"
#include <algorithm>
#include <string>

void UI::drawModPage() {
    static const char* headers[] = {"Slot", "Source", "Curve", "Amount", "Destination", "Type"};
    static const int headerCols[] = {2, 10, 28, 40, 56, 74};
    static const int colWidths[] = {4, 16, 10, 12, 18, 8};
    constexpr int slotCount = 16;
    constexpr int columnCount = 5;  // Source..Type

    int row = 3;

    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row, 2, "MODULATION MATRIX");
    attroff(COLOR_PAIR(1) | A_BOLD);
    row += 2;

    for (int h = 0; h < 6; ++h) {
        attron(COLOR_PAIR(1));
        mvprintw(row, headerCols[h], "%s", headers[h]);
        attroff(COLOR_PAIR(1));
    }
    row++;
    mvhline(row, 2, '-', 78);
    row += 2;

    for (int slot = 0; slot < slotCount; ++slot) {
        mvprintw(row, headerCols[0], "%02d", slot + 1);

        const char* placeholders[columnCount] = {"--", "--", "--", "--", "--"};
        for (int col = 0; col < columnCount; ++col) {
            bool selected = (slot == modMatrixCursorRow && col == modMatrixCursorCol);
            if (selected) {
                attron(COLOR_PAIR(5) | A_REVERSE | A_BOLD);
            } else {
                attron(COLOR_PAIR(6));
            }
            mvprintw(row, headerCols[col + 1], "%-*s", colWidths[col + 1], placeholders[col]);
            if (selected) {
                attroff(COLOR_PAIR(5) | A_REVERSE | A_BOLD);
            } else {
                attroff(COLOR_PAIR(6));
            }
        }
        row++;
    }

    row += 1;
    attron(COLOR_PAIR(6));
    mvprintw(row++, 2, "Arrow keys move the selection. Mod routing editing coming soon.");
    attroff(COLOR_PAIR(6));
}
