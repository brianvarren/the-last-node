#include "../../ui.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

float computePhaseDistorted(float phase, float morph) {
    float clamped = std::min(std::max(morph, 0.0f), 1.0f);
    float pivot;
    if (clamped <= 0.5f) {
        float t = clamped * 2.0f;  // 0..1
        pivot = 0.5f - (0.5f - 0.0001f) * t;
    } else {
        float t = (clamped - 0.5f) * 2.0f;  // 0..1
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

float computeTanhShaped(float phase, float morph, float duty) {
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

float computeWaveSample(float phase, float morph, float duty, int shape) {
    if (shape == 0) {
        return computePhaseDistorted(phase, morph);
    }
    float shiftedPhase = phase + 0.5f;
    if (shiftedPhase >= 1.0f) {
        shiftedPhase -= 1.0f;
    }
    return computeTanhShaped(shiftedPhase, morph, duty);
}

} // namespace

void UI::drawOscillatorWavePreview(int topRow, int leftCol, int plotHeight, int plotWidth) {
    float morph = params->getOscMorph(currentOscillatorIndex);
    float duty = params->getOscDuty(currentOscillatorIndex);
    bool flip = params->getOscFlip(currentOscillatorIndex);
    int shape = params->getOscShape(currentOscillatorIndex);

    morph = std::min(std::max(morph, 0.0f), 1.0f);
    duty = std::min(std::max(duty, 0.0f), 1.0f);

    int width = std::max(16, plotWidth);
    int height = std::max(6, plotHeight);

    std::vector<std::string> grid(height, std::string(width, ' '));

    int axisRow = height / 2;
    for (int x = 0; x < width; ++x) {
        grid[axisRow][x] = '-';
    }

    auto plotPoint = [&](int px, int py) {
        if (px >= 0 && px < width && py >= 0 && py < height) {
            grid[py][px] = '*';
        }
    };

    int prevRow = -1;
    int prevCol = -1;
    for (int x = 0; x < width; ++x) {
        float phase = (width == 1) ? 0.0f : static_cast<float>(x) / static_cast<float>(width - 1);
        float sample = computeWaveSample(phase, morph, duty, shape);
        if (flip) {
            sample = -sample;
        }
        sample = std::min(std::max(sample, -1.0f), 1.0f);
        float normalized = (-sample + 1.0f) * 0.5f;
        int row = static_cast<int>(std::round(normalized * (height - 1)));
        row = std::min(std::max(row, 0), height - 1);

        plotPoint(x, row);

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

void UI::drawOscillatorPage() {
    int row = 2;
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row, 2, "OSCILLATORS");
    attroff(COLOR_PAIR(1) | A_BOLD);

    row += 2;
    for (int i = 0; i < 4; ++i) {
        int col = 2 + i * 4;
        if (i == currentOscillatorIndex) {
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

    drawOscillatorWavePreview(previewTop, previewLeft, plotHeight, plotWidth);

    int parameterCol = previewLeft + plotWidth + 6;
    drawParametersPage(previewTop, parameterCol);
}
