#include "../../ui.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

// LFO waveform generation (matches LFO class implementation)
float computeLFOPhaseDistorted(float phase, float morph) {
    float clamped = std::min(std::max(morph, 0.0f), 1.0f);
    float pivot;
    if (clamped <= 0.5f) {
        float t = clamped * 2.0f;
        pivot = 0.5f - (0.5f - 0.0001f) * t;
    } else {
        float t = (clamped - 0.5f) * 2.0f;
        pivot = 0.5f + 0.4999f * t;
    }
    pivot = std::min(std::max(pivot, 0.0001f), 0.9999f);

    float shapedPhase;
    if (phase <= pivot) {
        float denom = std::max(1e-6f, 2.0f * pivot);
        shapedPhase = phase / denom;
    } else {
        float denom = std::max(1e-6f, 1.0f - pivot);
        shapedPhase = 0.5f * (1.0f + ((phase - pivot) / denom));
    }

    return -std::cos(shapedPhase * 2.0f * static_cast<float>(M_PI));
}

float computeLFOTanhShaped(float phase, float morph, float duty) {
    const float twoPi = 2.0f * static_cast<float>(M_PI);
    float sine = std::sin(twoPi * phase);
    float edge = std::min(std::max(morph, 0.0f), 1.0f);

    if (edge < 1e-3f) {
        return sine;
    }

    float theta = twoPi * (duty - 0.5f);
    float x = sine - std::sin(theta);
    float beta = 1.0f + 80.0f * edge;
    float tanhPulse = std::tanh(beta * x);
    return (1.0f - edge) * sine + edge * tanhPulse;
}

float computeLFOWaveSample(float phase, float morph, float duty, int shape) {
    if (shape == 0) {
        return computeLFOPhaseDistorted(phase, morph);
    }
    float shiftedPhase = phase + 0.5f;
    if (shiftedPhase >= 1.0f) {
        shiftedPhase -= 1.0f;
    }
    return computeLFOTanhShaped(shiftedPhase, morph, duty);
}

} // namespace

void UI::drawLFOWavePreview(int topRow, int leftCol, int plotHeight, int plotWidth, int lfoIndex, float phaseHead) {
    // Static waveform preview - show one complete cycle
    int width = std::max(16, plotWidth);
    int height = std::max(6, plotHeight);

    std::vector<std::string> grid(height, std::string(width, ' '));

    // Get LFO parameters
    float morph = params->getLfoMorph(lfoIndex);
    float duty = params->getLfoDuty(lfoIndex);
    bool flip = params->getLfoFlip(lfoIndex);
    int shape = params->getLfoShape(lfoIndex);

    morph = std::min(std::max(morph, 0.0f), 1.0f);
    duty = std::min(std::max(duty, 0.0f), 1.0f);

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

    // Draw static waveform (one complete cycle)
    int prevRow = -1;
    int prevCol = -1;
    for (int x = 0; x < width; ++x) {
        float phase = (width == 1) ? 0.0f : static_cast<float>(x) / static_cast<float>(width - 1);
        float sample = computeLFOWaveSample(phase, morph, duty, shape);
        if (flip) {
            sample = -sample;
        }
        sample = std::min(std::max(sample, -1.0f), 1.0f);
        float normalized = (-sample + 1.0f) * 0.5f;
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
    mvprintw(topRow - 1, leftCol, "Wave Preview");
    mvprintw(topRow, leftCol, "+%s+", horizontal.c_str());
    for (int y = 0; y < height; ++y) {
        mvprintw(topRow + 1 + y, leftCol, "|");
        mvprintw(topRow + 1 + y, leftCol + 1, "%s", grid[y].c_str());
        mvprintw(topRow + 1 + y, leftCol + 1 + width, "|");
    }
    mvprintw(topRow + 1 + height, leftCol, "+%s+", horizontal.c_str());

    // Draw LED bar indicator for current LFO output level
    int ledRow = topRow + 2 + height;
    float currentValue = params->getLfoVisualValue(lfoIndex);
    currentValue = std::min(std::max(currentValue, -1.0f), 1.0f);

    // Bar width is same as waveform width
    int barWidth = width;
    int centerPos = barWidth / 2;

    mvprintw(ledRow, leftCol, "Level:");
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
