#include "../../ui.h"
#include "../../synth.h"
#include "../../sampler.h"
#include "../../sample_bank.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

void UI::drawSamplerPage() {
    const int startRow = 3;
    const int leftCol = 2;
    const int valueCol = 35;
    int row = startRow;

    // Get sample bank
    const SampleBank* bank = synth->getSampleBank();
    const int sampleCount = bank ? bank->getSampleCount() : 0;

    // Draw title with sampler index
    attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(row++, leftCol, "SAMPLER %d", currentSamplerIndex + 1);
    attroff(COLOR_PAIR(5) | A_BOLD);
    row++;

    // Hotkey instructions
    attron(COLOR_PAIR(8));
    mvprintw(row++, leftCol, "Keys 1-4: Select sampler | Tab: Next page");
    attroff(COLOR_PAIR(8));
    row++;

    // Get current sampler state (from first voice as reference)
    float loopStart = synth->getSamplerLoopStart(currentSamplerIndex);
    float loopLength = synth->getSamplerLoopLength(currentSamplerIndex);
    float crossfadeLength = synth->getSamplerCrossfadeLength(currentSamplerIndex);
    float playbackSpeed = synth->getSamplerPlaybackSpeed(currentSamplerIndex);
    float tzfmDepth = synth->getSamplerTZFMDepth(currentSamplerIndex);
    PlaybackMode mode = synth->getSamplerPlaybackMode(currentSamplerIndex);
    float level = synth->getSamplerLevel(currentSamplerIndex);

    // Sample selection
    attron(COLOR_PAIR(7));
    mvprintw(row, leftCol, "Sample:");
    attroff(COLOR_PAIR(7));

    if (sampleCount > 0) {
        // TODO: Get current sample index from somewhere
        // For now, show first sample as placeholder
        const char* sampleName = bank->getSampleName(0);
        if (sampleName) {
            mvprintw(row++, valueCol, "%s", sampleName);
        } else {
            mvprintw(row++, valueCol, "No sample loaded");
        }
    } else {
        attron(COLOR_PAIR(8));
        mvprintw(row++, valueCol, "No samples found");
        attroff(COLOR_PAIR(8));
    }
    row++;

    // Playback mode
    attron(COLOR_PAIR(7));
    mvprintw(row, leftCol, "Playback Mode:");
    attroff(COLOR_PAIR(7));
    const char* modeStr = "Forward";
    if (mode == PlaybackMode::REVERSE) modeStr = "Reverse";
    else if (mode == PlaybackMode::ALTERNATE) modeStr = "Ping-Pong";
    mvprintw(row++, valueCol, "%s", modeStr);
    row++;

    // Loop start
    attron(COLOR_PAIR(7));
    mvprintw(row, leftCol, "Loop Start:");
    attroff(COLOR_PAIR(7));
    mvprintw(row++, valueCol, "%.1f%%", loopStart * 100.0f);

    // Loop length
    attron(COLOR_PAIR(7));
    mvprintw(row, leftCol, "Loop Length:");
    attroff(COLOR_PAIR(7));
    mvprintw(row++, valueCol, "%.1f%%", loopLength * 100.0f);

    // Crossfade length
    attron(COLOR_PAIR(7));
    mvprintw(row, leftCol, "Crossfade Length:");
    attroff(COLOR_PAIR(7));
    mvprintw(row++, valueCol, "%.1f%%", crossfadeLength * 100.0f);
    row++;

    // Playback speed
    attron(COLOR_PAIR(7));
    mvprintw(row, leftCol, "Playback Speed:");
    attroff(COLOR_PAIR(7));
    mvprintw(row++, valueCol, "%.2fx", playbackSpeed);

    // TZFM depth
    attron(COLOR_PAIR(7));
    mvprintw(row, leftCol, "TZFM Depth:");
    attroff(COLOR_PAIR(7));
    mvprintw(row++, valueCol, "%.1f%%", tzfmDepth * 100.0f);
    row++;

    // Level
    attron(COLOR_PAIR(7));
    mvprintw(row, leftCol, "Level:");
    attroff(COLOR_PAIR(7));
    mvprintw(row++, valueCol, "%.1f%%", level * 100.0f);
    row += 2;

    // Sample info section
    if (sampleCount > 0) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(row++, leftCol, "SAMPLE BANK");
        attroff(COLOR_PAIR(5) | A_BOLD);
        row++;

        attron(COLOR_PAIR(8));
        mvprintw(row++, leftCol, "Available samples: %d", sampleCount);
        attroff(COLOR_PAIR(8));
        row++;

        // List up to 10 samples
        int maxDisplay = std::min(10, sampleCount);
        for (int i = 0; i < maxDisplay; ++i) {
            const char* name = bank->getSampleName(i);
            if (name) {
                attron(COLOR_PAIR(7));
                mvprintw(row++, leftCol + 2, "[%d] %s", i + 1, name);
                attroff(COLOR_PAIR(7));
            }
        }

        if (sampleCount > 10) {
            attron(COLOR_PAIR(8));
            mvprintw(row++, leftCol + 2, "... and %d more", sampleCount - 10);
            attroff(COLOR_PAIR(8));
        }
    } else {
        row++;
        attron(COLOR_PAIR(8));
        mvprintw(row++, leftCol, "Place WAV files in build/samples/ to load them");
        attroff(COLOR_PAIR(8));
    }

    // Instructions at bottom
    row += 2;
    attron(COLOR_PAIR(8));
    mvprintw(row++, leftCol, "Sampler controls will be added in a future update");
    mvprintw(row++, leftCol, "For now, samplers are configured via the modulation matrix");
    attroff(COLOR_PAIR(8));
}
