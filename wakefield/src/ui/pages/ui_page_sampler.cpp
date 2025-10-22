#include "../../ui.h"
#include "../../synth.h"
#include "../../sampler.h"
#include "../../sample_bank.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

void UI::drawSamplerPage() {
    int maxY = getmaxy(stdscr);
    int maxX = getmaxx(stdscr);

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
    row++; // Add blank line after title

    // Waveform preview with loop indicator overlay - stretch to fill screen
    const int waveformHeight = 9; // Odd number for centerline visibility
    const int waveformWidth = maxX - leftCol - 2; // Stretch to screen width

    // Draw top border
    attron(COLOR_PAIR(8));
    mvhline(row++, leftCol, '-', waveformWidth);
    attroff(COLOR_PAIR(8));

    if (sample && sample->sampleCount > 0) {
        drawSamplerWaveform(row, leftCol, waveformHeight, waveformWidth, sample);
    } else {
        // Draw empty waveform with just centerline
        int centerRow = row + waveformHeight / 2;
        attron(COLOR_PAIR(2));
        mvhline(centerRow, leftCol, '*', waveformWidth);
        attroff(COLOR_PAIR(2));
    }
    row += waveformHeight;

    // Draw loop indicator as bottom border
    float loopStart = synth->getSamplerLoopStart(currentSamplerIndex);
    float loopLength = synth->getSamplerLoopLength(currentSamplerIndex);

    int loopStartPos = static_cast<int>(loopStart * waveformWidth);
    int loopEndPos = static_cast<int>((loopStart + loopLength) * waveformWidth);
    loopEndPos = std::min(loopEndPos, waveformWidth);

    for (int i = 0; i < waveformWidth; ++i) {
        if (i >= loopStartPos && i < loopEndPos) {
            attron(COLOR_PAIR(2) | A_BOLD);
            mvaddch(row, leftCol + i, '=');
            attroff(COLOR_PAIR(2) | A_BOLD);
        } else {
            attron(COLOR_PAIR(8));
            mvaddch(row, leftCol + i, '-');
            attroff(COLOR_PAIR(8));
        }
    }
    row += 2; // Skip past loop indicator and add blank line

    // Parameters in two columns
    attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(row++, leftCol, "PARAMETERS");
    attroff(COLOR_PAIR(5) | A_BOLD);

    // Get parameters from synth
    PlaybackMode mode = synth->getSamplerPlaybackMode(currentSamplerIndex);
    // loopStart and loopLength already declared above for loop indicator
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

    // Two columns layout with highlighting
    const int col1 = leftCol + 2;
    const int col2 = leftCol + 40;
    int paramRow = row;

    // Column 1 parameters (IDs 60-63)
    const int paramIds1[] = {60, 61, 62, 63};
    const char* labels1[] = {"Mode:       ", "Loop Start: ", "Loop Length:", "Xfade:      "};
    const char* values1[] = {modeStr, nullptr, nullptr, nullptr};

    for (int i = 0; i < 4; ++i) {
        if (paramIds1[i] == selectedParameterId) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(paramRow, col1, ">");
        } else {
            mvprintw(paramRow, col1, " ");
        }

        mvprintw(paramRow, col1 + 2, "%s", labels1[i]);

        if (i == 0) {
            printw("%s", modeStr);
        } else if (i == 1) {
            printw("%.1f%%", loopStart * 100.0f);
        } else if (i == 2) {
            printw("%.1f%%", loopLength * 100.0f);
        } else if (i == 3) {
            printw("%.1f%%", crossfade * 100.0f);
        }

        if (paramIds1[i] == selectedParameterId) {
            attroff(COLOR_PAIR(5) | A_BOLD);
        }
        paramRow++;
    }

    // Column 2 parameters (IDs 64-67)
    paramRow = row;
    const int paramIds2[] = {64, 65, 66, 67};
    const char* labels2[] = {"Ratio:      ", "Offset:     ", "Sync:       ", "Note Reset: "};

    for (int i = 0; i < 4; ++i) {
        if (paramIds2[i] == selectedParameterId) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(paramRow, col2, ">");
        } else {
            mvprintw(paramRow, col2, " ");
        }

        mvprintw(paramRow, col2 + 2, "%s", labels2[i]);

        if (i == 0) {
            printw("%.2f", ratio);
        } else if (i == 1) {
            printw("%.0f Hz", offset);
        } else if (i == 2) {
            printw("%s", syncStr);
        } else if (i == 3) {
            printw("%s", noteReset ? "On" : "Off");
        }

        if (paramIds2[i] == selectedParameterId) {
            attroff(COLOR_PAIR(5) | A_BOLD);
        }
        paramRow++;
    }

    row = paramRow + 1;

    // Instructions
    attron(COLOR_PAIR(8));
    mvprintw(row++, leftCol, "Keys 1-4: Select sampler | Enter: Load sample | Arrow keys: Adjust");
    attroff(COLOR_PAIR(8));
}

void UI::drawSamplerWaveform(int topRow, int leftCol, int height, int width, const SampleData* sample) {
    if (!sample || sample->sampleCount == 0 || !sample->samples) return;

    // Calculate how many samples to analyze per column
    const int samplesPerColumn = sample->sampleCount / width;
    if (samplesPerColumn == 0) return;

    int centerRow = topRow + height / 2;

    // For each column, find the peak amplitude
    for (int col = 0; col < width; ++col) {
        int startSample = col * samplesPerColumn;
        int endSample = std::min(startSample + samplesPerColumn, static_cast<int>(sample->sampleCount));

        // Find peak amplitude in this column
        float peakAmplitude = 0.0f;
        for (int i = startSample; i < endSample; ++i) {
            float sampleValue = std::abs(sample->samples[i]) / 32768.0f; // Convert Q15 to float
            if (sampleValue > peakAmplitude) {
                peakAmplitude = sampleValue;
            }
        }

        // Convert to column height (bipolar display)
        // Use full height/2 for maximum amplitude (don't scale down)
        int columnHeight = static_cast<int>(peakAmplitude * (height / 2));
        columnHeight = std::min(columnHeight, height / 2);

        // Draw above center
        for (int i = 0; i < columnHeight; ++i) {
            attron(COLOR_PAIR(2));
            mvaddch(centerRow - i - 1, leftCol + col, '*');
            attroff(COLOR_PAIR(2));
        }

        // Draw below center
        for (int i = 0; i < columnHeight; ++i) {
            attron(COLOR_PAIR(2));
            mvaddch(centerRow + i + 1, leftCol + col, '*');
            attroff(COLOR_PAIR(2));
        }

        // Always draw center line in green with * characters
        attron(COLOR_PAIR(2));
        mvaddch(centerRow, leftCol + col, '*');
        attroff(COLOR_PAIR(2));
    }
}
