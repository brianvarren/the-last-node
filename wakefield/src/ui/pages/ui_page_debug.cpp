#include "../../ui.h"
#include <algorithm>
#include <limits>

void UI::drawDebugPage() {
    int row = 3;

    attron(A_BOLD);
    mvprintw(row++, 1, "AUDIO DEBUG MONITOR");
    attroff(A_BOLD);
    row++;

    uint32_t currentMicros = audioDebugState.currentMicros.load(std::memory_order_relaxed);
    uint32_t minMicros = audioDebugState.minMicros.load(std::memory_order_relaxed);
    uint32_t maxMicros = audioDebugState.maxMicros.load(std::memory_order_relaxed);
    float currentPeak = audioDebugState.currentPeak.load(std::memory_order_relaxed);
    float maxPeak = audioDebugState.maxPeak.load(std::memory_order_relaxed);
    uint32_t currentVoices = audioDebugState.currentVoices.load(std::memory_order_relaxed);
    uint32_t maxVoices = audioDebugState.maxVoices.load(std::memory_order_relaxed);

    attron(COLOR_SECTION_HEADER);
    mvprintw(row++, 2, "Audio Callback");
    attroff(COLOR_SECTION_HEADER);
    std::string minMicrosStr = (minMicros == std::numeric_limits<uint32_t>::max())
                                   ? "n/a"
                                   : std::to_string(minMicros);
    std::string maxMicrosStr = (maxMicros == 0)
                                   ? "n/a"
                                   : std::to_string(maxMicros);

    mvprintw(row++, 4, "Duration: %u us (min %s, max %s)",
             currentMicros,
             minMicrosStr.c_str(),
             maxMicrosStr.c_str());

    mvprintw(row++, 4, "Peak amplitude: %.4f (max %.4f)", currentPeak, maxPeak);
    mvprintw(row++, 4, "Active voices: %u (peak %u)", currentVoices, maxVoices);
    mvprintw(row++, 4, "Output underruns: %llu", static_cast<unsigned long long>(getAudioUnderrunCount()));

    row += 2;
    attron(COLOR_HINT);
    mvprintw(row++, 2, "Ctrl+D - jump to this page   R - reset min/max statistics");
    mvprintw(row++, 2, "Use Tab/Shift+Tab to navigate pages (Debug follows Config).");
    attroff(COLOR_HINT);
}
