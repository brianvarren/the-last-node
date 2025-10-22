#include "../../ui.h"
#include "../../synth.h"
#include "../../sampler.h"
#include "../../sample_bank.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

void UI::drawSamplerPage() {
    const int leftCol = 2;
    int row = 3;

    // Get sample bank
    const SampleBank* bank = synth->getSampleBank();
    const int sampleCount = bank ? bank->getSampleCount() : 0;

    // Get current sample
    const SampleData* sample = (sampleCount > 0) ? bank->getSample(0) : nullptr;

    // Title with sampler index
    attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(row++, leftCol, "SAMPLER %d", currentSamplerIndex + 1);
    attroff(COLOR_PAIR(5) | A_BOLD);
    row++;

    // Sample name (highlighted - this will be the Enter key target)
    attron(COLOR_PAIR(5) | A_BOLD);
    if (sample && sample->name.length() > 0) {
        mvprintw(row++, leftCol, "> %s", sample->name.c_str());
    } else {
        mvprintw(row++, leftCol, "> No sample loaded");
    }
    attroff(COLOR_PAIR(5) | A_BOLD);
    row++;

    // Waveform preview
    if (sample && sample->sampleCount > 0) {
        const int waveformWidth = 76;
        const int waveformHeight = 12;

        drawSamplerWaveform(row, leftCol, waveformHeight, waveformWidth, sample);
        row += waveformHeight + 1;
    } else {
        attron(COLOR_PAIR(8));
        mvprintw(row++, leftCol, "[ No waveform - load a sample ]");
        attroff(COLOR_PAIR(8));
        row += 2;
    }

    // Loop region indicator
    float loopStart = synth->getSamplerLoopStart(currentSamplerIndex);
    float loopLength = synth->getSamplerLoopLength(currentSamplerIndex);

    attron(COLOR_PAIR(3));
    mvprintw(row, leftCol, "Loop Region: ");
    attroff(COLOR_PAIR(3));

    // Draw loop indicator bar
    const int barWidth = 60;
    int loopStartPos = static_cast<int>(loopStart * barWidth);
    int loopEndPos = static_cast<int>((loopStart + loopLength) * barWidth);
    loopEndPos = std::min(loopEndPos, barWidth);

    int barX = leftCol + 14;
    for (int i = 0; i < barWidth; ++i) {
        if (i >= loopStartPos && i < loopEndPos) {
            attron(COLOR_PAIR(2) | A_BOLD);
            mvaddch(row, barX + i, '=');
            attroff(COLOR_PAIR(2) | A_BOLD);
        } else {
            mvaddch(row, barX + i, '-');
        }
    }
    row += 2;

    // Parameters
    attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(row++, leftCol, "PARAMETERS");
    attroff(COLOR_PAIR(5) | A_BOLD);
    row++;

    // Get parameters from synth
    PlaybackMode mode = synth->getSamplerPlaybackMode(currentSamplerIndex);
    float crossfade = synth->getSamplerCrossfadeLength(currentSamplerIndex);
    float ratio = synth->getSamplerPlaybackSpeed(currentSamplerIndex);
    float offset = 0.0f; // TODO: Add offset parameter
    int syncMode = 0; // TODO: Add sync parameter
    bool noteReset = true; // TODO: Add note reset parameter

    const char* modeStr = (mode == PlaybackMode::FORWARD) ? "KEY" : "FREE";

    mvprintw(row++, leftCol, "  Mode:        %s", modeStr);
    mvprintw(row++, leftCol, "  Loop Start:  %.1f%%", loopStart * 100.0f);
    mvprintw(row++, leftCol, "  Loop Length: %.1f%%", loopLength * 100.0f);
    mvprintw(row++, leftCol, "  Xfade:       %.1f%%", crossfade * 100.0f);
    mvprintw(row++, leftCol, "  Ratio:       %.2f", ratio);
    mvprintw(row++, leftCol, "  Offset:      %.0f Hz", offset);

    const char* syncStr = "Off";
    if (syncMode == 1) syncStr = "On";
    else if (syncMode == 2) syncStr = "Trip";
    else if (syncMode == 3) syncStr = "Dot";
    mvprintw(row++, leftCol, "  Sync:        %s", syncStr);

    mvprintw(row++, leftCol, "  Note Reset:  %s", noteReset ? "On" : "Off");
    row += 2;

    // Instructions
    attron(COLOR_PAIR(8));
    mvprintw(row++, leftCol, "Keys 1-4: Select sampler | Enter: Load sample | Arrow keys: Adjust");
    attroff(COLOR_PAIR(8));
}

void UI::drawSamplerWaveform(int topRow, int leftCol, int height, int width, const SampleData* sample) {
    if (!sample || sample->sampleCount == 0 || !sample->samples) return;

    // Calculate how many samples to average per column
    const int samplesPerColumn = sample->sampleCount / width;
    if (samplesPerColumn == 0) return;

    // For each column, find the average amplitude
    for (int col = 0; col < width; ++col) {
        int startSample = col * samplesPerColumn;
        int endSample = std::min(startSample + samplesPerColumn, static_cast<int>(sample->sampleCount));

        // Calculate RMS amplitude for this column
        float sumSquared = 0.0f;
        for (int i = startSample; i < endSample; ++i) {
            float sampleValue = sample->samples[i] / 32768.0f; // Convert Q15 to float
            sumSquared += sampleValue * sampleValue;
        }
        float rms = std::sqrt(sumSquared / (endSample - startSample));

        // Convert to column height (bipolar)
        int columnHeight = static_cast<int>(rms * height * 0.8f); // Scale to 80% of max height
        columnHeight = std::min(columnHeight, height / 2);

        // Draw the column centered
        int centerRow = topRow + height / 2;

        // Draw above center
        for (int i = 0; i < columnHeight; ++i) {
            attron(COLOR_PAIR(2));
            mvaddch(centerRow - i, leftCol + col, '*');
            attroff(COLOR_PAIR(2));
        }

        // Draw below center
        for (int i = 0; i < columnHeight; ++i) {
            attron(COLOR_PAIR(2));
            mvaddch(centerRow + i + 1, leftCol + col, '*');
            attroff(COLOR_PAIR(2));
        }

        // Draw center line
        attron(COLOR_PAIR(8));
        if (columnHeight == 0) {
            mvaddch(centerRow, leftCol + col, '-');
        }
        attroff(COLOR_PAIR(8));
    }

    // Draw border
    attron(COLOR_PAIR(8));
    mvhline(topRow - 1, leftCol, '-', width);
    mvhline(topRow + height, leftCol, '-', width);
    attroff(COLOR_PAIR(8));
}
