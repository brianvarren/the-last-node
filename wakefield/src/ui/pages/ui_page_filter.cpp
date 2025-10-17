#include "../../ui.h"

void UI::drawFilterPage() {
    drawParametersPage();

    // Show MIDI Learn status if active
    if (params->midiLearnActive.load() && params->midiLearnParameterId.load() == 32) { // Filter cutoff
        int row = 15;
        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(row++, 2, ">>> MIDI LEARN ACTIVE <<<");
        attroff(COLOR_PAIR(3) | A_BOLD);
        mvprintw(row++, 2, "Move a MIDI controller to assign it to Filter Cutoff");
    }
}
