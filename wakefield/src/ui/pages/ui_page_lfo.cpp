#include "../../ui.h"
#include <algorithm>
#include <cmath>
#include <vector>

void UI::drawLFOWavePreview(int topRow, int leftCol, int plotHeight, int plotWidth, int lfoIndex, float phaseHead) {
    // Rolling history scope view that fits one cycle to the window width.
    int width = std::max(16, plotWidth);
    int height = std::max(6, plotHeight);

    std::vector<std::string> grid(height, std::string(width, ' '));

    // Draw center axis
    int axisRow = height / 2;
    for (int x = 0; x < width; ++x) {
        grid[axisRow][x] = '-';
    }

    auto plotPoint = [&](int px, int py) {
        if (px >= 0 && px < width && py >= 0 && py < height) {
            grid[py][px] = '*';
        }
    };

    // Get current LFO period to determine zoom level.
    float period = params->getLfoPeriod(lfoIndex);
    const float LFO_UI_SAMPLE_RATE = 1000.0f; // Corresponds to usleep(1000) in the LFO thread.
    float samplesPerCycle = period * LFO_UI_SAMPLE_RATE;

    // Prevent division by zero or extreme values.
    if (samplesPerCycle < 1.0f) {
        samplesPerCycle = 1.0f;
    }

    // Calculate the step size to sample one full cycle across the window width.
    float step = samplesPerCycle / static_cast<float>(width);

    int writePos = lfoHistoryWritePos[lfoIndex];
    int historySize = lfoHistoryBuffer[lfoIndex].size();

    int prevRow = -1;
    int prevCol = -1;

    for (int x = 0; x < width; ++x) {
        // Map x position to a point within the last cycle in the history buffer.
        float historyOffset = (width - 1 - x) * step;
        int bufferIndex = (writePos - 1 - static_cast<int>(historyOffset) + historySize) % historySize;

        float amplitude = lfoHistoryBuffer[lfoIndex][bufferIndex];

        // Convert amplitude (-1 to +1) to row position
        amplitude = std::min(std::max(amplitude, -1.0f), 1.0f);
        float normalized = (-amplitude + 1.0f) * 0.5f;
        int row = static_cast<int>(std::round(normalized * (height - 1)));
        row = std::min(std::max(row, 0), height - 1);

        plotPoint(x, row);

        // Draw line segments between points
        if (prevRow >= 0) {
            int dx = x - prevCol;
            int dy = row - prevRow;
            int steps = std::max(std::abs(dx), std::abs(dy));
            for (int step_i = 1; step_i <= steps; ++step_i) { // use step_i to avoid shadowing
                int px = prevCol + (dx * step_i) / steps;
                int py = prevRow + (dy * step_i) / steps;
                plotPoint(px, py);
            }
        }

        prevRow = row;
        prevCol = x;
    }

    // Draw the preview box
    std::string horizontal(width, '-');
    mvprintw(topRow - 1, leftCol, "LFO Scope (1 cycle)");
    mvprintw(topRow, leftCol, "+%s+", horizontal.c_str());
    for (int y = 0; y < height; ++y) {
        mvprintw(topRow + 1 + y, leftCol, "|");
        mvprintw(topRow + 1 + y, leftCol + 1, "%s", grid[y].c_str());
        mvprintw(topRow + 1 + y, leftCol + 1 + width, "|");
    }
    mvprintw(topRow + 1 + height, leftCol, "+%s+", horizontal.c_str());
}

void UI::drawLFOPage() {
    int row = 2;
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row, 2, "LFOs");
    attroff(COLOR_PAIR(1) | A_BOLD);

    row += 2;
    for (int i = 0; i < 4; ++i) {
        int col = 2 + i * 4;
        if (i == currentLFOIndex) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(row, col, "[%d]", i + 1);
            attroff(COLOR_PAIR(5) | A_BOLD);
        } else {
            mvprintw(row, col, " %d ", i + 1);
        }
    }

    row += 2;
    const int plotWidth = 42;
    const int plotHeight = 14;
    int previewLeft = 2;
    int previewTop = row;

    float currentPhase = params->getLfoVisualPhase(currentLFOIndex);
    drawLFOWavePreview(previewTop, previewLeft, plotHeight, plotWidth, currentLFOIndex, currentPhase);

    int parameterCol = previewLeft + plotWidth + 6;
    drawParametersPage(previewTop, parameterCol);
}
