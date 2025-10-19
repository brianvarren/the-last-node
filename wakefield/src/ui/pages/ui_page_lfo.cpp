#include "../../ui.h"
#include <algorithm>
#include <cmath>
#include <vector>

void UI::drawLFOWavePreview(int topRow, int leftCol, int plotHeight, int plotWidth, int lfoIndex, float /*phaseHead*/) {
    // Rolling history scope view - newest sample on right, history scrolls left
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

    int writePos = lfoHistoryWritePos[lfoIndex];
    int historySize = lfoHistoryBuffer[lfoIndex].size();

    // X zoom: how many history samples to display across the width
    // Y zoom: vertical scaling factor (1 = full range, 2 = 2x zoom, etc)
    int samplesToShow = historySize > 0 ? std::min(lfoScopeXZoom, historySize) : 1;
    float yScale = static_cast<float>(lfoScopeYZoom);

    int denominator = std::max(1, width - 1);
    int sampleSpan = std::max(0, samplesToShow - 1);
    float columnsPerSample = (sampleSpan > 0) ? static_cast<float>(denominator) / static_cast<float>(sampleSpan) : 0.0f;
    int newestIndex = (historySize > 0) ? (writePos - 1 + historySize) % historySize : 0;

    auto sampleHistory = [&](float offsetFloat) -> float {
        if (historySize == 0) {
            return 0.0f;
        }
        int baseOffset = static_cast<int>(std::floor(offsetFloat));
        float frac = offsetFloat - static_cast<float>(baseOffset);

        int index1 = (newestIndex - baseOffset + historySize) % historySize;
        float sample1 = lfoHistoryBuffer[lfoIndex][index1];

        if (historySize == 1) {
            return sample1;
        }

        int index2 = (index1 - 1 + historySize) % historySize;
        float sample2 = lfoHistoryBuffer[lfoIndex][index2];
        return sample1 + frac * (sample2 - sample1);
    };

    auto& state = lfoScopeState[lfoIndex];
    bool needReinit = !state.initialized || state.width != width || state.height != height ||
                      state.lastSamplesToShow != samplesToShow ||
                      static_cast<int>(state.columnAmplitudes.size()) != width;

    auto initializeAmplitudes = [&]() {
        state.columnAmplitudes.assign(width, 0.0f);
        if (historySize > 0) {
            for (int col = 0; col < width; ++col) {
                float t = (denominator > 0) ? static_cast<float>(col) / static_cast<float>(denominator) : 0.0f;
                float historyOffsetFloat = static_cast<float>(sampleSpan) * (1.0f - t);
                state.columnAmplitudes[col] = sampleHistory(historyOffsetFloat);
            }
        }
        state.width = width;
        state.height = height;
        state.lastSamplesToShow = samplesToShow;
        state.lastWritePos = writePos;
        state.initialized = true;
    };

    if (needReinit) {
        initializeAmplitudes();
    } else if (historySize > 0) {
        int deltaSamples = (writePos - state.lastWritePos + historySize) % historySize;
        state.lastWritePos = writePos;

        float shiftAmount = columnsPerSample * static_cast<float>(deltaSamples);

        if (shiftAmount >= static_cast<float>(width)) {
            initializeAmplitudes();
        } else if (shiftAmount > 0.0f && !state.columnAmplitudes.empty()) {
            std::vector<float> shifted(width, 0.0f);
            for (int dst = 0; dst < width; ++dst) {
                float srcPos = static_cast<float>(dst) + shiftAmount;
                if (srcPos <= static_cast<float>(width - 1)) {
                    int idx0 = static_cast<int>(std::floor(srcPos));
                    int idx1 = std::min(width - 1, idx0 + 1);
                    float frac = srcPos - static_cast<float>(idx0);
                    float value0 = state.columnAmplitudes[idx0];
                    float value1 = state.columnAmplitudes[idx1];
                    shifted[dst] = value0 + frac * (value1 - value0);
                }
            }

            int newColumns = std::min(width, static_cast<int>(std::ceil(shiftAmount)));
            int startCol = std::max(0, width - newColumns);
            for (int col = startCol; col < width; ++col) {
                float t = (denominator > 0) ? static_cast<float>(col) / static_cast<float>(denominator) : 0.0f;
                float historyOffsetFloat = static_cast<float>(sampleSpan) * (1.0f - t);
                shifted[col] = sampleHistory(historyOffsetFloat);
            }

            state.columnAmplitudes.swap(shifted);
        }
    } else {
        // No history data yet; ensure amplitudes vector matches width
        if (static_cast<int>(state.columnAmplitudes.size()) != width) {
            state.columnAmplitudes.assign(width, 0.0f);
        }
        state.width = width;
        state.height = height;
        state.lastSamplesToShow = samplesToShow;
        state.lastWritePos = writePos;
        state.initialized = true;
    }

    // Build the display grid from the current amplitude state
    int prevRow = -1;
    int prevCol = -1;

    for (int x = 0; x < width; ++x) {
        float amplitude = (x < static_cast<int>(state.columnAmplitudes.size())) ? state.columnAmplitudes[x] : 0.0f;

        // Apply Y zoom
        amplitude *= yScale;
        amplitude = std::min(std::max(amplitude, -1.0f), 1.0f);

        // Convert amplitude (-1 to +1) to row position
        float normalized = (-amplitude + 1.0f) * 0.5f;
        int row = static_cast<int>(std::round(normalized * (height - 1)));
        row = std::min(std::max(row, 0), height - 1);

        plotPoint(x, row);

        // Draw line segments between points
        if (prevRow >= 0) {
            int dx = x - prevCol;
            int dy = row - prevRow;
            int steps = std::max(std::abs(dx), std::abs(dy));
            for (int step_i = 1; step_i <= steps; ++step_i) {
                int px = prevCol + (dx * step_i) / steps;
                int py = prevRow + (dy * step_i) / steps;
                plotPoint(px, py);
            }
        }

        prevRow = row;
        prevCol = x;
    }

    // Draw the preview box with zoom info
    std::string horizontal(width, '-');
    mvprintw(topRow - 1, leftCol, "LFO Scope (X:%d Y:%dx)", lfoScopeXZoom, lfoScopeYZoom);
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

    // Draw zoom controls below the scope
    int zoomRow = previewTop + plotHeight + 3;
    mvprintw(zoomRow, previewLeft, "Zoom: [/] X-axis   {/} Y-axis");
    mvprintw(zoomRow + 1, previewLeft, "      [%3d]       {%dx}", lfoScopeXZoom, lfoScopeYZoom);
}
