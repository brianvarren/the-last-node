#include "../../ui.h"

void UI::drawChaosPage() {
    int row = 3;
    int col = 2;

    // Page title
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row++, col, "CHAOS GENERATORS (Ikeda Map)");
    attroff(COLOR_PAIR(1) | A_BOLD);

    row++; // Blank line

    // Info text
    attron(COLOR_PAIR(8));
    mvprintw(row++, col, "Chaos generators produce complex, non-repeating modulation signals");
    mvprintw(row++, col, "using the Ikeda map algorithm. Useful for evolving textures.");
    attroff(COLOR_PAIR(8));

    row += 2;

    // Draw parameters for Chaos 1-4 (currently using chaos index selection like LFOs)
    // For now, we'll show all 4 chaos generators on one page
    for (int i = 0; i < 4; ++i) {
        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(row++, col, "CHAOS %d", i + 1);
        attroff(COLOR_PAIR(1) | A_BOLD);

        // Get parameters for this chaos generator
        float chaosParam = 0.6f;  // Default for now
        float clockFreq = 1.0f;
        bool cubicInterp = false;
        bool fastMode = false;

        switch (i) {
            case 0:
                chaosParam = params->chaos1Parameter.load();
                clockFreq = params->chaos1ClockFreq.load();
                cubicInterp = params->chaos1CubicInterp.load();
                fastMode = params->chaos1FastMode.load();
                break;
            case 1:
                chaosParam = params->chaos2Parameter.load();
                clockFreq = params->chaos2ClockFreq.load();
                cubicInterp = params->chaos2CubicInterp.load();
                fastMode = params->chaos2FastMode.load();
                break;
            case 2:
                chaosParam = params->chaos3Parameter.load();
                clockFreq = params->chaos3ClockFreq.load();
                cubicInterp = params->chaos3CubicInterp.load();
                fastMode = params->chaos3FastMode.load();
                break;
            case 3:
                chaosParam = params->chaos4Parameter.load();
                clockFreq = params->chaos4ClockFreq.load();
                cubicInterp = params->chaos4CubicInterp.load();
                fastMode = params->chaos4FastMode.load();
                break;
        }

        mvprintw(row, col + 2, "Chaos:  %.2f", chaosParam);
        mvprintw(row, col + 25, "Clock: %.2f Hz", clockFreq);
        row++;

        mvprintw(row, col + 2, "Mode:   %s", fastMode ? "FAST (audio)" : "CLOCKED (slow)");
        mvprintw(row, col + 25, "Interp: %s", cubicInterp ? "CUBIC" : "LINEAR");
        row += 2;
    }

    row++;
    attron(COLOR_PAIR(8));
    mvprintw(row++, col, "Chaos Parameter: 0.0=periodic, ~0.6=chaotic, >0.9=very chaotic");
    mvprintw(row++, col, "Fast Mode: Audio-rate iteration (more CPU, richer sound)");
    mvprintw(row++, col, "Clocked Mode: Low-rate iteration with interpolation (less CPU)");
    mvprintw(row++, col, "Cubic Interpolation: Smoother transitions in clocked mode");
    attroff(COLOR_PAIR(8));
}
