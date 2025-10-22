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

    // Get current sample for this sampler
    int sampleIndex = synth->getSamplerSampleIndex(currentSamplerIndex);
    const SampleData* sample = (sampleIndex >= 0) ? bank->getSample(sampleIndex) : nullptr;

    // Title with sampler index and sample name on same line
    attron(COLOR_PAIR(5) | A_BOLD);
    if (sample && sample->name.length() > 0) {
        mvprintw(row++, leftCol, "SAMPLER %d  |  > %s", currentSamplerIndex + 1, sample->name.c_str());
    } else {
        mvprintw(row++, leftCol, "SAMPLER %d  |  > No sample loaded", currentSamplerIndex + 1);
    }
    attroff(COLOR_PAIR(5) | A_BOLD);

    // Waveform preview with loop indicator overlay
    if (sample && sample->sampleCount > 0) {
        const int waveformWidth = 76;
        const int waveformHeight = 11; // Odd number for centerline visibility

        drawSamplerWaveform(row, leftCol, waveformHeight, waveformWidth, sample);

        // Draw loop indicator overlaid on bottom of waveform
        float loopStart = synth->getSamplerLoopStart(currentSamplerIndex);
        float loopLength = synth->getSamplerLoopLength(currentSamplerIndex);

        int loopRow = row + waveformHeight;
        int loopStartPos = static_cast<int>(loopStart * waveformWidth);
        int loopEndPos = static_cast<int>((loopStart + loopLength) * waveformWidth);
        loopEndPos = std::min(loopEndPos, waveformWidth);

        for (int i = 0; i < waveformWidth; ++i) {
            if (i >= loopStartPos && i < loopEndPos) {
                attron(COLOR_PAIR(2) | A_BOLD);
                mvaddch(loopRow, leftCol + i, '=');
                attroff(COLOR_PAIR(2) | A_BOLD);
            } else {
                attron(COLOR_PAIR(8));
                mvaddch(loopRow, leftCol + i, '-');
                attroff(COLOR_PAIR(8));
            }
        }

        row += waveformHeight + 2;
    } else {
        attron(COLOR_PAIR(8));
        mvprintw(row++, leftCol, "[ No waveform - load a sample ]");
        attroff(COLOR_PAIR(8));
        row += 2;
    }

    // Parameters in two columns
    attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(row++, leftCol, "PARAMETERS");
    attroff(COLOR_PAIR(5) | A_BOLD);

    // Get parameters from synth
    PlaybackMode mode = synth->getSamplerPlaybackMode(currentSamplerIndex);
    float loopStart = synth->getSamplerLoopStart(currentSamplerIndex);
    float loopLength = synth->getSamplerLoopLength(currentSamplerIndex);
    float crossfade = synth->getSamplerCrossfadeLength(currentSamplerIndex);
    float ratio = synth->getSamplerPlaybackSpeed(currentSamplerIndex);
    float offset = 0.0f; // TODO: Add offset parameter
    int syncMode = 0; // TODO: Add sync parameter
    bool noteReset = true; // TODO: Add note reset parameter

    const char* modeStr = (mode == PlaybackMode::FORWARD) ? "KEY" : "FREE";

    const char* syncStr = "Off";
    if (syncMode == 1) syncStr = "On";
    else if (syncMode == 2) syncStr = "Trip";
    else if (syncMode == 3) syncStr = "Dot";

    // Two columns layout
    const int col1 = leftCol + 2;
    const int col2 = leftCol + 40;
    int paramRow = row;

    mvprintw(paramRow++, col1, "Mode:        %s", modeStr);
    mvprintw(paramRow++, col1, "Loop Start:  %.1f%%", loopStart * 100.0f);
    mvprintw(paramRow++, col1, "Loop Length: %.1f%%", loopLength * 100.0f);
    mvprintw(paramRow++, col1, "Xfade:       %.1f%%", crossfade * 100.0f);

    paramRow = row;
    mvprintw(paramRow++, col2, "Ratio:       %.2f", ratio);
    mvprintw(paramRow++, col2, "Offset:      %.0f Hz", offset);
    mvprintw(paramRow++, col2, "Sync:        %s", syncStr);
    mvprintw(paramRow++, col2, "Note Reset:  %s", noteReset ? "On" : "Off");

    row = paramRow + 1;

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
