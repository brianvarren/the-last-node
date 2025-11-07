#include "../../ui.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "../../tanh_gain_lut.h"

namespace {

constexpr float kTwoPi = 2.0f * static_cast<float>(M_PI);
constexpr float kMinPivot = 0.0001f;
constexpr float kMaxPivot = 0.9999f;
constexpr float kMaxBeta = 80.0f;
constexpr int kShapeSaw = 0;
constexpr int kShapePulse = 1;

inline float lookupTanhGain(float morph) {
    float clamped = std::clamp(morph, 0.0f, 1.0f);
    float position = clamped * static_cast<float>(kTanhGainLUTSize - 1);
    int index = static_cast<int>(position);
    int next = std::min(index + 1, kTanhGainLUTSize - 1);
    float frac = position - static_cast<float>(index);
    float a = kTanhGainLUT[index];
    float b = kTanhGainLUT[next];
    return a + (b - a) * frac;
}

float computePhaseDistorted(float phase, float morph) {
    float clamped = std::clamp(morph, 0.0f, 1.0f);
    bool mirror = false;
    float morphAmount = clamped;

    if (clamped < 0.5f) {
        mirror = true;
        morphAmount = 1.0f - clamped * 2.0f;
        morphAmount = 0.5f + morphAmount * 0.5f;
    }

    float t = (morphAmount - 0.5f) * 2.0f;
    float pivot = 0.5f + 0.4999f * t;
    pivot = std::clamp(pivot, kMinPivot, kMaxPivot);

    float workingPhase = mirror ? (1.0f - phase) : phase;
    float shapedPhase;
    if (workingPhase <= pivot) {
        float denom = std::max(2.0f * pivot, kMinPivot);
        shapedPhase = workingPhase / denom;
    } else {
        float denom = std::max(1.0f - pivot, kMinPivot);
        shapedPhase = 0.5f * (1.0f + ((workingPhase - pivot) / denom));
    }

    return -std::cos(shapedPhase * kTwoPi);
}

float computeTanhSquare(float phase, float morph) {
    float edge = std::clamp(morph, 0.0f, 1.0f);
    if (edge <= 1e-6f) {
        return std::sin(kTwoPi * phase);
    }

    float beta = 1.0f + edge * kMaxBeta;
    float sample = std::tanh(beta * std::sin(kTwoPi * phase));
    return sample * lookupTanhGain(edge);
}

float computeWaveSample(float phase, int shape, float morph) {
    if (shape == kShapeSaw) {
        return computePhaseDistorted(phase, morph);
    }
    return computeTanhSquare(phase, morph);
}

} // namespace

void UI::drawOscillatorWavePreview(int topRow, int leftCol, int plotHeight, int plotWidth) {
    int shape = params->getOscShape(currentOscillatorIndex);
    float morph = std::clamp(params->getOscMorph(currentOscillatorIndex), 0.0f, 1.0f);

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
        float sample = computeWaveSample(phase, shape, morph);
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
    // Page title with inline instance buttons
    attron(COLOR_PAGE_TITLE | A_BOLD);
    mvprintw(row, 2, "OSCILLATORS");
    attroff(COLOR_PAGE_TITLE | A_BOLD);

    int titleLen = 11; // length of "OSCILLATORS"
    int buttonsStartCol = 2 + titleLen + 2;
    int activeOsc = std::clamp(params->activeOscCount.load(), 1, 4);
    for (int i = 0; i < 4; ++i) {
        int col = buttonsStartCol + i * 4;
        bool inactive = (i >= activeOsc);
        if (inactive) attron(COLOR_LOCKED | A_DIM);
        if (!inactive && i == currentOscillatorIndex) {
            attron(COLOR_SELECTION | A_BOLD);
            mvprintw(row, col, "[%d]", i + 1);
            attroff(COLOR_SELECTION | A_BOLD);
        } else {
            mvprintw(row, col, "[%d]", i + 1);
        }
        if (inactive) attroff(COLOR_LOCKED | A_DIM);
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
