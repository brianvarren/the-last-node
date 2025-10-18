#include "../../ui.h"
#include "../../envelope.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

void UI::drawEnvelopePreview(int topRow, int leftCol, int plotHeight, int plotWidth) {
    // Get envelope parameters for the currently selected envelope
    float attack = params->getEnvAttack(currentEnvelopeIndex);
    float decay = params->getEnvDecay(currentEnvelopeIndex);
    float sustain = params->getEnvSustain(currentEnvelopeIndex);
    float release = params->getEnvRelease(currentEnvelopeIndex);
    float attackBend = params->getEnvAttackBend(currentEnvelopeIndex);
    float releaseBend = params->getEnvReleaseBend(currentEnvelopeIndex);

    // Calculate total envelope time (normalize to fit in display)
    float totalTime = attack + decay + release + 0.5f; // +0.5 for sustain section

    std::vector<std::string> grid(plotHeight, std::string(plotWidth, ' '));

    // Draw baseline at bottom
    for (int x = 0; x < plotWidth; ++x) {
        grid[plotHeight - 1][x] = '-';
    }

    // Calculate time divisions
    int attackWidth = std::max(1, static_cast<int>((attack / totalTime) * plotWidth));
    int decayWidth = std::max(1, static_cast<int>((decay / totalTime) * plotWidth));
    int sustainWidth = std::max(1, static_cast<int>((0.5f / totalTime) * plotWidth));
    int releaseWidth = plotWidth - attackWidth - decayWidth - sustainWidth;
    releaseWidth = std::max(1, releaseWidth);

    // Helper lambda to apply bend curve
    auto applyBend = [](float t, float bend) -> float {
        if (bend < 0.45f) {
            // Concave (logarithmic)
            float factor = (0.45f - bend) * 20.0f;
            return (std::exp(factor * t) - 1.0f) / (std::exp(factor) - 1.0f);
        } else if (bend > 0.55f) {
            // Convex (exponential)
            float factor = (bend - 0.55f) * 20.0f;
            return 1.0f - (std::exp(factor * (1.0f - t)) - 1.0f) / (std::exp(factor) - 1.0f);
        } else {
            // Linear
            return t;
        }
    };

    // Draw envelope curve
    int x = 0;

    // Attack phase
    for (int i = 0; i < attackWidth && x < plotWidth; ++i, ++x) {
        float t = static_cast<float>(i) / std::max(1.0f, static_cast<float>(attackWidth - 1));
        float bentT = applyBend(t, attackBend);
        float amplitude = bentT; // 0 to 1
        int row = plotHeight - 1 - static_cast<int>(std::round(amplitude * (plotHeight - 1)));
        row = std::min(std::max(row, 0), plotHeight - 1);
        grid[row][x] = '*';
    }

    // Decay phase
    for (int i = 0; i < decayWidth && x < plotWidth; ++i, ++x) {
        float t = static_cast<float>(i) / std::max(1.0f, static_cast<float>(decayWidth - 1));
        float bentT = applyBend(t, releaseBend);
        float amplitude = 1.0f - bentT * (1.0f - sustain); // 1 to sustain
        int row = plotHeight - 1 - static_cast<int>(std::round(amplitude * (plotHeight - 1)));
        row = std::min(std::max(row, 0), plotHeight - 1);
        grid[row][x] = '*';
    }

    // Sustain phase
    for (int i = 0; i < sustainWidth && x < plotWidth; ++i, ++x) {
        int row = plotHeight - 1 - static_cast<int>(std::round(sustain * (plotHeight - 1)));
        row = std::min(std::max(row, 0), plotHeight - 1);
        grid[row][x] = '*';
    }

    // Release phase
    for (int i = 0; i < releaseWidth && x < plotWidth; ++i, ++x) {
        float t = static_cast<float>(i) / std::max(1.0f, static_cast<float>(releaseWidth - 1));
        float bentT = applyBend(t, releaseBend);
        float amplitude = sustain * (1.0f - bentT); // sustain to 0
        int row = plotHeight - 1 - static_cast<int>(std::round(amplitude * (plotHeight - 1)));
        row = std::min(std::max(row, 0), plotHeight - 1);
        grid[row][x] = '*';
    }

    // Draw the grid
    std::string horizontal(plotWidth, '-');
    mvprintw(topRow - 1, leftCol, "Envelope Preview");
    mvprintw(topRow, leftCol, "+%s+", horizontal.c_str());
    for (int y = 0; y < plotHeight; ++y) {
        mvprintw(topRow + 1 + y, leftCol, "|");
        mvprintw(topRow + 1 + y, leftCol + 1, "%s", grid[y].c_str());
        mvprintw(topRow + 1 + y, leftCol + 1 + plotWidth, "|");
    }
    mvprintw(topRow + 1 + plotHeight, leftCol, "+%s+", horizontal.c_str());

    // Draw stage labels below the plot
    int labelY = topRow + 1 + plotHeight + 1;
    int attackEnd = attackWidth;
    int decayEnd = attackWidth + decayWidth;
    int sustainEnd = attackWidth + decayWidth + sustainWidth;

    if (attackWidth > 2) mvprintw(labelY, leftCol + 1 + attackWidth / 2, "A");
    if (decayWidth > 2) mvprintw(labelY, leftCol + 1 + attackEnd + decayWidth / 2, "D");
    if (sustainWidth > 2) mvprintw(labelY, leftCol + 1 + decayEnd + sustainWidth / 2, "S");
    if (releaseWidth > 2) mvprintw(labelY, leftCol + 1 + sustainEnd + releaseWidth / 2, "R");
}

void UI::drawEnvelopePage() {
    // Draw envelope selector at top
    int row = 3;
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row, 2, "Select Envelope (1-4): ");
    attroff(COLOR_PAIR(1) | A_BOLD);

    // Draw envelope buttons
    for (int i = 0; i < 4; ++i) {
        if (i == currentEnvelopeIndex) {
            attron(COLOR_PAIR(5) | A_BOLD);  // Highlight selected
            mvprintw(row, 26 + (i * 4), "[%d]", i + 1);
            attroff(COLOR_PAIR(5) | A_BOLD);
        } else {
            attron(COLOR_PAIR(6));
            mvprintw(row, 26 + (i * 4), " %d ", i + 1);
            attroff(COLOR_PAIR(6));
        }
    }

    row += 2;
    attron(COLOR_PAIR(1));
    mvprintw(row, 2, "Envelope %d Parameters:", currentEnvelopeIndex + 1);
    attroff(COLOR_PAIR(1));

    // Draw parameters for the currently selected envelope
    drawParametersPage(row + 2);

    // Draw envelope preview in the bottom portion, leaving room for parameters
    int rows = getmaxy(stdscr);
    int cols = getmaxx(stdscr);

    // Calculate preview dimensions: use about 35% of screen height for preview
    // This leaves plenty of room for parameters at top
    int availableHeight = rows - (row + 2) - 2; // Subtract top content and bottom margin
    int plotHeight = std::min(12, availableHeight / 3); // Max 12 rows or 1/3 available space
    int plotWidth = cols - 8; // Leave some margin

    // Position preview: leave space after parameters (about 10 rows) plus border/label space
    int plotTop = row + 2 + 10; // Start after parameter list with some spacing
    int leftCol = 4;

    drawEnvelopePreview(plotTop, leftCol, plotHeight, plotWidth);
}
