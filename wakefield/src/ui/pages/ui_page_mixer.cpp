#include "../../ui.h"
#include "../../synth.h"

void UI::drawMixerPage() {
    int row = 3;
    int col = 2;

    // Page title
    attron(COLOR_PAGE_TITLE | A_BOLD);
    mvprintw(row++, col, "MIXER");
    attroff(COLOR_PAGE_TITLE | A_BOLD);

    row++; // Blank line

    // Header
    attron(COLOR_SECTION_HEADER);
    mvprintw(row++, col, "Source    M S  Level");
    attroff(COLOR_SECTION_HEADER);

    // Get parameter IDs for this page
    std::vector<int> pageParams = getParameterIdsForPage(UIPage::MIXER);

    // Draw oscillators (IDs 50-53)
    for (int i = 0; i < 4; ++i) {
        int paramId = 50 + i;
        float level = params->getOscLevel(i);
        bool muted = params->oscMuted[i].load();
        bool solo = params->oscSolo[i].load();

        // Highlight selected parameter
        if (paramId == selectedParameterId) {
            attron(COLOR_SELECTION | A_BOLD);
            mvprintw(row, col, ">");
        } else {
            mvprintw(row, col, " ");
        }

        mvprintw(row, col + 2, "OSC %d", i + 1);

        // Draw mute/solo indicators
        if (muted) {
            attron(COLOR_LOCKED | A_BOLD);  // Red for mute
            mvprintw(row, col + 10, "M");
            attroff(COLOR_LOCKED | A_BOLD);
        } else {
            mvprintw(row, col + 10, "-");
        }

        if (solo) {
            attron(COLOR_MODULATED | A_BOLD);  // Green for solo
            mvprintw(row, col + 12, "S");
            attroff(COLOR_MODULATED | A_BOLD);
        } else {
            mvprintw(row, col + 12, "-");
        }

        // Draw compact bar (28 chars to make room for M/S)
        drawBar(row, col + 15, "", level, 0.0f, 1.0f, 28);

        if (paramId == selectedParameterId) {
            attroff(COLOR_SELECTION | A_BOLD);
        }

        row++;
    }

    row++; // Blank line

    // Draw samplers (IDs 54-57)
    for (int i = 0; i < 4; ++i) {
        int paramId = 54 + i;
        float level = synth->getSamplerLevel(i);
        bool muted = params->samplerMuted[i].load();
        bool solo = params->samplerSolo[i].load();

        // Highlight selected parameter
        if (paramId == selectedParameterId) {
            attron(COLOR_SELECTION | A_BOLD);
            mvprintw(row, col, ">");
        } else {
            mvprintw(row, col, " ");
        }

        mvprintw(row, col + 2, "SAMP %d", i + 1);

        // Draw mute/solo indicators
        if (muted) {
            attron(COLOR_LOCKED | A_BOLD);  // Red for mute
            mvprintw(row, col + 10, "M");
            attroff(COLOR_LOCKED | A_BOLD);
        } else {
            mvprintw(row, col + 10, "-");
        }

        if (solo) {
            attron(COLOR_MODULATED | A_BOLD);  // Green for solo
            mvprintw(row, col + 12, "S");
            attroff(COLOR_MODULATED | A_BOLD);
        } else {
            mvprintw(row, col + 12, "-");
        }

        // Draw compact bar (28 chars to make room for M/S)
        drawBar(row, col + 15, "", level, 0.0f, 1.0f, 28);

        if (paramId == selectedParameterId) {
            attroff(COLOR_SELECTION | A_BOLD);
        }

        row++;
    }

    row += 2;
    attron(COLOR_PAIR(8));
    mvprintw(row++, col, "Mix levels control the static volume for each source.");
    mvprintw(row++, col, "Hotkeys: M=Toggle Mute | S=Toggle Solo");
    attroff(COLOR_PAIR(8));

    row += 1;
    // Draw chaos generators (IDs 410-413)
    for (int i = 0; i < 4; ++i) {
        int paramId = 410 + i;
        float level = params->getChaosLevel(i);
        bool muted = params->chaosMuted[i].load();
        bool solo = params->chaosSolo[i].load();

        // Highlight selected parameter
        if (paramId == selectedParameterId) {
            attron(COLOR_SELECTION | A_BOLD);
            mvprintw(row, col, ">");
        } else {
            mvprintw(row, col, " ");
        }

        mvprintw(row, col + 2, "CHAOS %d", i + 1);

        // Draw mute/solo indicators
        if (muted) {
            attron(COLOR_LOCKED | A_BOLD);
            mvprintw(row, col + 10, "M");
            attroff(COLOR_LOCKED | A_BOLD);
        } else {
            mvprintw(row, col + 10, "-");
        }
        if (solo) {
            attron(COLOR_MODULATED | A_BOLD);
            mvprintw(row, col + 12, "S");
            attroff(COLOR_MODULATED | A_BOLD);
        } else {
            mvprintw(row, col + 12, "-");
        }

        drawBar(row, col + 15, "", level, 0.0f, 1.0f, 28);

        if (paramId == selectedParameterId) {
            attroff(COLOR_SELECTION | A_BOLD);
        }

        row++;
    }
}
