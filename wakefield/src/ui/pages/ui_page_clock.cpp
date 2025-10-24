#include "../../ui.h"
#include "../../sequencer.h"
#include "../../clock.h"
#include <string>
#include <cstdio>

extern Sequencer* sequencer;

namespace {
void drawLabelValue(int row, int col, const char* label, const char* value) {
    mvprintw(row, col, "%-14s", label);
    mvprintw(row, col + 16, "%s", value);
}
} // namespace

void UI::drawClockPage() {
    int col = 2;
    int row = 3;

    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row, col, "CLOCK");
    attroff(COLOR_PAIR(1) | A_BOLD);
    row += 2;

    double tempo = sequencer ? sequencer->getTempo() : 0.0;
    bool playing = sequencer ? sequencer->isPlaying() : false;

    char tempoBuf[32];
    if (tempo > 0.0) {
        std::snprintf(tempoBuf, sizeof(tempoBuf), "%.2f BPM", tempo);
    } else {
        std::snprintf(tempoBuf, sizeof(tempoBuf), "%s", "N/A");
    }
    drawLabelValue(row++, col, "Tempo:", tempoBuf);
    drawLabelValue(row++, col, "State:", playing ? "Playing" : "Stopped");

    Clock* clock = synth ? synth->getClock() : nullptr;
    if (clock) {
        double quarterPhase = clock->getPhase(Subdivision::QUARTER);
        double sixteenthPhase = clock->getPhase(Subdivision::SIXTEENTH);
        char phaseBuf[32];
        std::snprintf(phaseBuf, sizeof(phaseBuf), "%.3f", quarterPhase);
        drawLabelValue(row++, col, "Quarter Phase:", phaseBuf);
        std::snprintf(phaseBuf, sizeof(phaseBuf), "%.3f", sixteenthPhase);
        drawLabelValue(row++, col, "Sixteenth Phase:", phaseBuf);
    } else {
        drawLabelValue(row++, col, "Clock Source:", "Unavailable");
    }

    row += 2;
    attron(COLOR_PAIR(5));
    mvprintw(row++, col, "ROUTING");
    attroff(COLOR_PAIR(5));
    mvprintw(row++, col, "- Mod matrix slots 9-12 default to Clock → Sequencer Track 1-4");
    mvprintw(row++, col, "- Replace Clock source to drive track phase from any modulator");
    mvprintw(row++, col, "- Sampler phase drivers follow the same routing defaults");
}
