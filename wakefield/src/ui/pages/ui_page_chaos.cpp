#include "../../ui.h"
#include "../../chaos.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <deque>

// Trajectory history buffer for visualization
namespace {
    // Store recent (x, y) points for each chaos generator
    const int TRAJECTORY_HISTORY_SIZE = 300;
    std::deque<std::pair<float, float>> trajectoryHistory[4];
}

void UI::drawChaosVisualization(int topRow, int leftCol, int plotHeight, int plotWidth, int chaosIndex) {
    int width = std::max(16, plotWidth);
    int height = std::max(6, plotHeight);

    // Get current chaos state
    float currentX = 0.0f;
    float currentY = 0.0f;
    float currentValue = 0.0f;
    bool running = true;

    switch (chaosIndex) {
        case 0:
            currentX = params->chaos1VisualX.load();
            currentY = params->chaos1VisualY.load();
            running = params->chaos1Running.load();
            break;
        case 1:
            currentX = params->chaos2VisualX.load();
            currentY = params->chaos2VisualY.load();
            running = params->chaos2Running.load();
            break;
        case 2:
            currentX = params->chaos3VisualX.load();
            currentY = params->chaos3VisualY.load();
            running = params->chaos3Running.load();
            break;
        case 3:
            currentX = params->chaos4VisualX.load();
            currentY = params->chaos4VisualY.load();
            running = params->chaos4Running.load();
            break;
    }

    // Update trajectory history
    if (running && chaosIndex >= 0 && chaosIndex < 4) {
        trajectoryHistory[chaosIndex].push_back({currentX, currentY});
        if (trajectoryHistory[chaosIndex].size() > TRAJECTORY_HISTORY_SIZE) {
            trajectoryHistory[chaosIndex].pop_front();
        }
    }

    // Create visualization grid
    std::vector<std::string> grid(height, std::string(width, ' '));

    // Define viewport bounds for Ikeda map (typically bounded around -2 to +2)
    const float viewMinX = -2.0f;
    const float viewMaxX = 2.0f;
    const float viewMinY = -2.0f;
    const float viewMaxY = 2.0f;

    // Helper to map world coordinates to grid coordinates
    auto worldToGrid = [&](float x, float y, int& gx, int& gy) {
        float normX = (x - viewMinX) / (viewMaxX - viewMinX);
        float normY = (y - viewMinY) / (viewMaxY - viewMinY);
        gx = static_cast<int>(normX * (width - 1));
        gy = static_cast<int>((1.0f - normY) * (height - 1));  // Flip Y for screen coords
        gx = std::min(std::max(gx, 0), width - 1);
        gy = std::min(std::max(gy, 0), height - 1);
    };

    // Draw axis markers at origin if visible
    int originX, originY;
    worldToGrid(0.0f, 0.0f, originX, originY);
    if (originX >= 0 && originX < width && originY >= 0 && originY < height) {
        grid[originY][originX] = '+';
    }

    // Draw trajectory history
    if (chaosIndex >= 0 && chaosIndex < 4) {
        for (size_t i = 0; i < trajectoryHistory[chaosIndex].size(); ++i) {
            auto& point = trajectoryHistory[chaosIndex][i];
            int gx, gy;
            worldToGrid(point.first, point.second, gx, gy);

            if (gx >= 0 && gx < width && gy >= 0 && gy < height) {
                // Use different characters for recent vs old points
                if (i < trajectoryHistory[chaosIndex].size() / 3) {
                    grid[gy][gx] = '.';  // Older points
                } else if (i < 2 * trajectoryHistory[chaosIndex].size() / 3) {
                    grid[gy][gx] = 'o';  // Mid-age points
                } else {
                    grid[gy][gx] = '*';  // Recent points
                }
            }
        }
    }

    // Draw current position
    int currentGX, currentGY;
    worldToGrid(currentX, currentY, currentGX, currentGY);
    if (currentGX >= 0 && currentGX < width && currentGY >= 0 && currentGY < height) {
        grid[currentGY][currentGX] = '@';
    }

    // Draw the preview box
    std::string horizontal(width, '-');
    mvprintw(topRow - 1, leftCol, "Phase Space (%s)", running ? "Running" : "Stopped");
    mvprintw(topRow, leftCol, "+%s+", horizontal.c_str());
    for (int y = 0; y < height; ++y) {
        mvprintw(topRow + 1 + y, leftCol, "|");
        mvprintw(topRow + 1 + y, leftCol + 1, "%s", grid[y].c_str());
        mvprintw(topRow + 1 + y, leftCol + 1 + width, "|");
    }
    mvprintw(topRow + 1 + height, leftCol, "+%s+", horizontal.c_str());

    // Draw output level bar (X output)
    int ledRow = topRow + 2 + height;
    currentValue = currentX;
    currentValue = std::min(std::max(currentValue, -1.0f), 1.0f);

    int barWidth = width;
    int centerPos = barWidth / 2;

    mvprintw(ledRow, leftCol, "X Out:");
    mvprintw(ledRow + 1, leftCol, "[");

    // Draw bar with center at middle
    for (int i = 0; i < barWidth; ++i) {
        char ch = ' ';
        if (currentValue >= 0.0f) {
            // Positive: fill from center to right
            float fillPos = centerPos + (currentValue * (barWidth - centerPos));
            if (i >= centerPos && i < static_cast<int>(fillPos)) {
                ch = '=';
            } else if (i == centerPos) {
                ch = '|';
            }
        } else {
            // Negative: fill from center to left
            float fillPos = centerPos + (currentValue * centerPos);
            if (i > static_cast<int>(fillPos) && i <= centerPos) {
                ch = '=';
            } else if (i == centerPos) {
                ch = '|';
            }
        }
        mvprintw(ledRow + 1, leftCol + 1 + i, "%c", ch);
    }

    mvprintw(ledRow + 1, leftCol + 1 + barWidth, "]");
    mvprintw(ledRow + 1, leftCol + 3 + barWidth, "%+.2f", currentValue);
}

void UI::drawChaosPage() {
    int row = 2;
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row, 2, "CHAOS");
    attroff(COLOR_PAIR(1) | A_BOLD);

    row += 2;
    // Draw tabs for chaos generators 1-4
    for (int i = 0; i < 4; ++i) {
        int col = 2 + i * 4;
        if (i == currentChaosIndex) {
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

    drawChaosVisualization(previewTop, previewLeft, plotHeight, plotWidth, currentChaosIndex);

    int parameterCol = previewLeft + plotWidth + 6;
    drawParametersPage(previewTop, parameterCol);
}
