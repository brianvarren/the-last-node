#include "../../ui.h"
#include "../../synth.h"
#include "../../sampler.h"
#include "../../sample_bank.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

void drawSamplerWaveform(int topRow, int leftCol, int height, int width, const SampleData* sample, float loopStart, float loopLength) {
    if (!sample || sample->sampleCount == 0 || !sample->samples) return;

    // Calculate how many samples to analyze per column
    const int samplesPerColumn = sample->sampleCount / width;
    if (samplesPerColumn == 0) return;

    int centerRow = topRow + height / 2;

    // Calculate loop region boundaries in columns
    int loopStartCol = static_cast<int>(loopStart * width);
    int loopEndCol = static_cast<int>((loopStart + loopLength) * width);

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

        // Determine color: bright green in loop region, dim green outside
        bool inLoop = (col >= loopStartCol && col < loopEndCol);
        int colorAttr = inLoop ? (COLOR_WAVEFORM_ACTIVE | A_BOLD) : (COLOR_WAVEFORM_INACTIVE | A_DIM);

        // Draw above center
        for (int i = 0; i < columnHeight; ++i) {
            attron(colorAttr);
            mvaddch(centerRow - i - 1, leftCol + col, '*');
            attroff(colorAttr);
        }

        // Draw below center
        for (int i = 0; i < columnHeight; ++i) {
            attron(colorAttr);
            mvaddch(centerRow + i + 1, leftCol + col, '*');
            attroff(colorAttr);
        }

        // Always draw center line in green with * characters
        attron(colorAttr);
        mvaddch(centerRow, leftCol + col, '*');
        attroff(colorAttr);
    }
}

} // namespace

void UI::drawSamplerPage() {
    int maxY = getmaxy(stdscr);
    int maxX = getmaxx(stdscr);

    const int leftCol = 2;
    int row = 2;  // Shifted up from 3

    // Get sample bank
    const SampleBank* bank = synth->getSampleBank();

    // Get current sample for this sampler
    int sampleIndex = synth->getSamplerSampleIndex(currentSamplerIndex);
    const SampleData* sample = (sampleIndex >= 0) ? bank->getSample(sampleIndex) : nullptr;

    // Page title with inline instance buttons
    attron(COLOR_PAGE_TITLE | A_BOLD);
    mvprintw(row, leftCol, "SAMPLERS");
    attroff(COLOR_PAGE_TITLE | A_BOLD);
    int titleLen = 8; // length of "SAMPLERS"
    int buttonsStartCol = leftCol + titleLen + 2;
    int activeSamp = std::clamp(params->activeSamplerCount.load(), 1, 4);
    for (int i = 0; i < 4; ++i) {
        int col = buttonsStartCol + i * 4;
        bool inactive = (i >= activeSamp);
        if (inactive) attron(COLOR_LOCKED | A_DIM);
        if (!inactive && i == currentSamplerIndex) {
            attron(COLOR_SELECTION | A_BOLD);
            mvprintw(row, col, "[%d]", i + 1);
            attroff(COLOR_SELECTION | A_BOLD);
        } else {
            mvprintw(row, col, "[%d]", i + 1);
        }
        if (inactive) attroff(COLOR_LOCKED | A_DIM);
    }
    row += 2;
    attron(COLOR_SECTION_HEADER);
    mvprintw(row, leftCol, "Sampler %d", currentSamplerIndex + 1);
    attroff(COLOR_SECTION_HEADER);

    // Sample name as selectable parameter (ID 69) - inline with title
    const int sampleCol = leftCol + 15;
    bool sampleSelected = (selectedParameterId == 69);
    if (sampleSelected) {
        attron(COLOR_SELECTION | A_BOLD);
        mvprintw(row, sampleCol, ">");
    } else {
        mvprintw(row, sampleCol, " ");
    }
    mvprintw(row, sampleCol + 2, "Sample: ");
    if (sample && sample->name.length() > 0) {
        printw("%s", sample->name.c_str());
    } else {
        printw("No sample loaded");
    }
    if (sampleSelected) {
        attroff(COLOR_SELECTION | A_BOLD);
    }
    row += 2;  // Spacing before waveform

    // Waveform preview with loop indicator overlay - stretch to fill screen
    const int waveformHeight = 9; // Odd number for centerline visibility
    const int waveformWidth = maxX - leftCol - 2; // Stretch to screen width

    // Draw border box around waveform area (using default text color)
    // Top and bottom borders
    mvhline(row, leftCol, '-', waveformWidth);
    mvhline(row + waveformHeight + 1, leftCol, '-', waveformWidth);
    // Left and right borders
    for (int i = 0; i < waveformHeight; ++i) {
        mvaddch(row + 1 + i, leftCol - 1, '|');
        mvaddch(row + 1 + i, leftCol + waveformWidth, '|');
    }
    // Corners
    mvaddch(row, leftCol - 1, '+');
    mvaddch(row, leftCol + waveformWidth, '+');
    mvaddch(row + waveformHeight + 1, leftCol - 1, '+');
    mvaddch(row + waveformHeight + 1, leftCol + waveformWidth, '+');

    row++; // Move to inside the border

    // Get loop parameters to pass to waveform drawer
    float loopStart = synth->getSamplerLoopStart(currentSamplerIndex);
    float loopLength = synth->getSamplerLoopLength(currentSamplerIndex);

    if (sample && sample->sampleCount > 0) {
        drawSamplerWaveform(row, leftCol, waveformHeight, waveformWidth, sample, loopStart, loopLength);
    } else {
        // Draw empty waveform with just centerline
        int centerRow = row + waveformHeight / 2;
        attron(COLOR_GRID_LINE);
        mvhline(centerRow, leftCol, '*', waveformWidth);
        attroff(COLOR_GRID_LINE);
    }
    row += waveformHeight;

    // Draw regular bottom border
    mvhline(row, leftCol, '-', waveformWidth);
    row += 2; // Skip past border and add blank line

    // Parameters in two columns
    attron(COLOR_SECTION_HEADER | A_BOLD);
    mvprintw(row++, leftCol, "PARAMETERS");
    attroff(COLOR_SECTION_HEADER | A_BOLD);

    // Get parameters from synth
    int noteSource = params->getSamplerNoteSource(currentSamplerIndex);
    PlaybackMode direction = synth->getSamplerPlaybackMode(currentSamplerIndex);
    // loopStart and loopLength already declared above for loop indicator
    float crossfade = synth->getSamplerCrossfadeLength(currentSamplerIndex);
    int octave = synth->getSamplerOctave(currentSamplerIndex);
    float tune = synth->getSamplerTune(currentSamplerIndex);
    int syncMode = synth->getSamplerSyncMode(currentSamplerIndex);
    bool noteReset = synth->getSamplerNoteReset(currentSamplerIndex);

    const char* noteSourceStr = "Unknown";
    switch (noteSource) {
        case static_cast<int>(NoteSource::FREE): noteSourceStr = "Free"; break;
        case static_cast<int>(NoteSource::EXTERNAL_MIDI): noteSourceStr = "Ext MIDI"; break;
        case static_cast<int>(NoteSource::TRACK_1): noteSourceStr = "Track 1"; break;
        case static_cast<int>(NoteSource::TRACK_2): noteSourceStr = "Track 2"; break;
        case static_cast<int>(NoteSource::TRACK_3): noteSourceStr = "Track 3"; break;
        case static_cast<int>(NoteSource::TRACK_4): noteSourceStr = "Track 4"; break;
        default: noteSourceStr = "Unknown"; break;
    }
    const char* directionStr = "Forward";
    if (direction == PlaybackMode::REVERSE) {
        directionStr = "Reverse";
    } else if (direction == PlaybackMode::ALTERNATE) {
        directionStr = "Ping-Pong";
    }

    const char* syncStr = "Off";
    if (syncMode == 1) syncStr = "On";
    else if (syncMode == 2) syncStr = "Trip";
    else if (syncMode == 3) syncStr = "Dot";

    // Two columns layout with highlighting
    const int col1 = leftCol + 2;
    const int col2 = leftCol + 40;
    int paramRow = row;

    // Column 1 parameters
    const int paramIds1[] = {80, 68, 61, 62, 63};
    const char* labels1[] = {"Note Source:", "Direction:  ", "Loop Start: ", "Loop Length:", "Xfade:      "};

    for (int i = 0; i < 5; ++i) {
        if (paramIds1[i] == selectedParameterId) {
            attron(COLOR_SELECTION | A_BOLD);
            mvprintw(paramRow, col1, ">");
        } else {
            mvprintw(paramRow, col1, " ");
        }

        InlineParameter* p = getParameter(paramIds1[i]);
        bool locked = p && !p->randomizable;
        if (locked) attron(COLOR_LOCKED);
        mvprintw(paramRow, col1 + 2, "%s", labels1[i]);

        if (i == 0) {
            printw("%s", noteSourceStr);
        } else if (i == 1) {
            printw("%s", directionStr);
        } else if (i == 2) {
            printw("%.1f%%", loopStart * 100.0f);
        } else if (i == 3) {
            printw("%.1f%%", loopLength * 100.0f);
        } else if (i == 4) {
            printw("%.1f%%", crossfade * 100.0f);
        }

        // Lock indicator
        if (locked) {
            int x = col1 + 2 + 14; // end of label area approx
            attron(COLOR_LOCKED);
            mvprintw(paramRow, x, " [LOCK]");
            attroff(COLOR_LOCKED);
        }

        if (locked) attroff(COLOR_LOCKED);

        if (paramIds1[i] == selectedParameterId) {
            attroff(COLOR_SELECTION | A_BOLD);
        }
        paramRow++;
    }

    // Column 2 parameters (IDs 64-67)
    paramRow = row;
    const int paramIds2[] = {64, 65, 66, 67};
    const char* labels2[] = {"Octave:     ", "Tune:       ", "Sync:       ", "Note Reset: "};

    for (int i = 0; i < 4; ++i) {
        if (paramIds2[i] == selectedParameterId) {
            attron(COLOR_SELECTION | A_BOLD);
            mvprintw(paramRow, col2, ">");
        } else {
            mvprintw(paramRow, col2, " ");
        }

        InlineParameter* p2 = getParameter(paramIds2[i]);
        bool locked2 = p2 && !p2->randomizable;
        if (locked2) attron(COLOR_LOCKED);
        mvprintw(paramRow, col2 + 2, "%s", labels2[i]);

        if (i == 0) {
            printw("%+d", octave);
        } else if (i == 1) {
            printw("%.3f", tune);
        } else if (i == 2) {
            printw("%s", syncStr);
        } else if (i == 3) {
            printw("%s", noteReset ? "On" : "Off");
        }

        if (locked2) {
            int x = col2 + 2 + 14;
            attron(COLOR_LOCKED);
            mvprintw(paramRow, x, " [LOCK]");
            attroff(COLOR_LOCKED);
        }

        if (locked2) attroff(COLOR_LOCKED);

        if (paramIds2[i] == selectedParameterId) {
            attroff(COLOR_SELECTION | A_BOLD);
        }
        paramRow++;
    }

}

// (helper lives in anonymous namespace above)
