#include "../../ui.h"
#include <algorithm>
#include <cmath>
#include <vector>

void UI::drawLFOWavePreview(int topRow, int leftCol, int plotHeight, int plotWidth, int lfoIndex, float phaseHead) {
    // Rolling history scope view - shows amplitude over time
    // Current amplitude on right edge, history scrolling left
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

    // Read from circular history buffer
    // Most recent samples are at writePos-1, oldest at writePos
    int writePos = lfoHistoryWritePos[lfoIndex];
    int historySize = lfoHistoryBuffer[lfoIndex].size();

    // Draw from history buffer, newest on right
    int prevRow = -1;
    int prevCol = -1;

    for (int x = 0; x < width; ++x) {
        // Map x position to history buffer index
        // x=0 shows oldest sample in window, x=width-1 shows newest
        int historyOffset = width - 1 - x;
        int bufferIndex = (writePos - 1 - historyOffset + historySize) % historySize;

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
            for (int step = 1; step <= steps; ++step) {
                int px = prevCol + (dx * step) / steps;
                int py = prevRow + (dy * step) / steps;
                plotPoint(px, py);
            }
        }

        prevRow = row;
        prevCol = x;
    }

    // Draw the preview box
    std::string horizontal(width, '-');
    mvprintw(topRow - 1, leftCol, "LFO Scope");
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
