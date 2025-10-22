#include "../../ui.h"
#include "../../synth.h"

void UI::drawMixerPage() {
    int row = 3;
    int col = 2;

    // Page title
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row++, col, "MIXER");
    attroff(COLOR_PAIR(1) | A_BOLD);

    row++; // Blank line

    // Header
    attron(COLOR_PAIR(3));
    mvprintw(row++, col, "Source    Level");
    attroff(COLOR_PAIR(3));

    // Get parameter IDs for this page
    std::vector<int> pageParams = getParameterIdsForPage(UIPage::MIXER);

    // Draw oscillators (IDs 50-53)
    for (int i = 0; i < 4; ++i) {
        int paramId = 50 + i;
        float level = params->getOscLevel(i);

        // Highlight selected parameter
        if (paramId == selectedParameterId) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(row, col, ">");
        } else {
            mvprintw(row, col, " ");
        }

        mvprintw(row, col + 2, "OSC %d", i + 1);

        // Draw compact bar (30 chars)
        drawBar(row, col + 10, "", level, 0.0f, 1.0f, 30);

        if (paramId == selectedParameterId) {
            attroff(COLOR_PAIR(5) | A_BOLD);
        }

        row++;
    }

    row++; // Blank line

    // Draw samplers (IDs 54-57)
    for (int i = 0; i < 4; ++i) {
        int paramId = 54 + i;
        float level = synth->getSamplerLevel(i);

        // Highlight selected parameter
        if (paramId == selectedParameterId) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(row, col, ">");
        } else {
            mvprintw(row, col, " ");
        }

        mvprintw(row, col + 2, "SAMP %d", i + 1);

        // Draw compact bar (30 chars)
        drawBar(row, col + 10, "", level, 0.0f, 1.0f, 30);

        if (paramId == selectedParameterId) {
            attroff(COLOR_PAIR(5) | A_BOLD);
        }

        row++;
    }

    row += 2;
    attron(COLOR_PAIR(8));
    mvprintw(row++, col, "Mix levels control the static volume for each source.");
    attroff(COLOR_PAIR(8));
}
