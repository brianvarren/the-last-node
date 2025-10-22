#include "../../ui.h"

void UI::drawMixerPage() {
    int row = 3;
    int col = 2;

    // Page title
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row++, col, "MIXER - Oscillator Mix Levels");
    attroff(COLOR_PAIR(1) | A_BOLD);

    row++; // Blank line

    mvprintw(row++, col, "Mix levels control the static volume of each oscillator.");
    mvprintw(row++, col, "Amplitude (modulated by envelopes) is separate from mix level.");

    row++; // Blank line

    // Draw oscillator mix levels
    for (int i = 0; i < 4; ++i) {
        float level = params->getOscLevel(i);
        float amp = params->getOscAmp(i);

        attron(COLOR_PAIR(2) | A_BOLD);
        mvprintw(row++, col, "OSC %d:", i + 1);
        attroff(COLOR_PAIR(2) | A_BOLD);

        // Mix level bar
        mvprintw(row, col + 2, "Mix Level: ");
        drawBar(row++, col + 14, "", level, 0.0f, 1.0f, 40);

        // Amp value (read-only display)
        mvprintw(row, col + 2, "Amplitude: ");
        drawBar(row++, col + 14, "", amp, 0.0f, 1.0f, 40);

        row++; // Blank line between oscillators
    }

    row++;
    attron(COLOR_PAIR(3));
    mvprintw(row++, col, "Note: Use arrow keys to navigate and adjust mix levels.");
    mvprintw(row++, col, "      Amplitude is controlled by modulation (see MOD page).");
    attroff(COLOR_PAIR(3));
}
