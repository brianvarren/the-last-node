#include "../../ui.h"
#include <algorithm>
#include <cmath>

void UI::drawLFOWavePreview(int topRow, int leftCol, int plotHeight, int plotWidth, int lfoIndex, float phase) {
    if (lfoIndex < 0 || lfoIndex >= static_cast<int>(lfoWaveHistory.size())) {
        return;
    }

    int width = std::max(16, plotWidth);
    int height = std::max(6, plotHeight);

    auto& history = lfoWaveHistory[lfoIndex];
    // Ensure history buffer matches preview width
    while (static_cast<int>(history.size()) < width) {
        history.push_front(0.0f);
    }
    while (static_cast<int>(history.size()) > width) {
        history.pop_front();
    }

    std::vector<std::string> grid(height, std::string(width, ' '));
    int axisRow = height / 2;
    for (int x = 0; x < width; ++x) {
        grid[axisRow][x] = '-';
    }

    for (int x = 0; x < width; ++x) {
        float sample = history[x];
        sample = std::min(std::max(sample, -1.0f), 1.0f);
        float normalized = (-sample + 1.0f) * 0.5f;
        int row = static_cast<int>(std::round(normalized * (height - 1)));
        row = std::min(std::max(row, 0), height - 1);

        float age = static_cast<float>(x) / std::max(1, width - 1);
        char drawChar = '.';
        if (age > 0.75f) {
            drawChar = '*';
        } else if (age > 0.5f) {
            drawChar = '+';
        } else if (age > 0.25f) {
            drawChar = ':';
        }

        grid[row][x] = drawChar;
    }

    int phaseCol = static_cast<int>(std::round(phase * (width - 1)));
    phaseCol = std::min(std::max(phaseCol, 0), width - 1);
    float phaseSample = history[phaseCol];
    float phaseNorm = (-phaseSample + 1.0f) * 0.5f;
    int phaseRow = static_cast<int>(std::round(phaseNorm * (height - 1)));
    phaseRow = std::min(std::max(phaseRow, 0), height - 1);
    grid[phaseRow][phaseCol] = 'O';

    std::string horizontal(width, '-');
    mvprintw(topRow - 1, leftCol, "Wave Preview");
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
    const int plotHeight = 12;
    int previewLeft = 2;
    int previewTop = row;

    float currentValue = params->getLfoVisualValue(currentLFOIndex);
    float currentPhase = params->getLfoVisualPhase(currentLFOIndex);
    if (currentLFOIndex < static_cast<int>(lfoWaveHistory.size())) {
        auto& history = lfoWaveHistory[currentLFOIndex];
        if (static_cast<int>(history.size()) >= plotWidth) {
            history.pop_front();
        }
        history.push_back(currentValue);
    }
    lfoPhaseSnapshot[currentLFOIndex] = currentPhase;

    drawLFOWavePreview(previewTop, previewLeft, plotHeight, plotWidth, currentLFOIndex, currentPhase);

    int parameterCol = previewLeft + plotWidth + 6;
    drawParametersPage(previewTop, parameterCol);
}
