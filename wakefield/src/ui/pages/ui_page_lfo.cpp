#include "../../ui.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

struct Shade {
    chtype attr;
    char ch;
};

float renderPhaseDistorted(float phase, float morph) {
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

float renderTanhPulse(float phase, float morph, float duty) {
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

float renderLfoSample(float phase, float morph, float duty) {
    float morphAmount = std::min(std::max(morph, 0.0f), 1.0f);
    if (morphAmount < 0.5f) {
        return renderPhaseDistorted(phase, morphAmount * 2.0f);
    }

    float shiftedPhase = phase + 0.5f;
    if (shiftedPhase >= 1.0f) {
        shiftedPhase -= 1.0f;
    }
    float tanhMorph = (morphAmount - 0.5f) * 2.0f;
    return renderTanhPulse(shiftedPhase, tanhMorph, duty);
}

} // namespace

void UI::drawLFOWavePreview(int topRow, int leftCol, int plotHeight, int plotWidth, int lfoIndex, float phaseHead) {
    int width = std::max(32, plotWidth);
    int height = std::max(8, plotHeight);
    std::vector<std::vector<int>> shadeIndex(height, std::vector<int>(width, 5));

    const Shade shades[] = {
        {COLOR_PAIR(5) | A_BOLD, '@'},
        {COLOR_PAIR(5), '#'},
        {COLOR_PAIR(6) | A_BOLD, '*'},
        {COLOR_PAIR(6), '+'},
        {COLOR_PAIR(6) | A_DIM, '.'}
    };
    constexpr int shadeCount = sizeof(shades) / sizeof(shades[0]);

    float morph = params->getLfoMorph(lfoIndex);
    float duty = params->getLfoDuty(lfoIndex);
    bool flip = params->getLfoFlip(lfoIndex);

    auto applyPoint = [&](int x, int y, int shade) {
        if (x < 0 || x >= width || y < 0 || y >= height) {
            return;
        }
        int clampedShade = std::max(0, std::min(shade, shadeCount - 1));
        if (shadeIndex[y][x] > clampedShade) {
            shadeIndex[y][x] = clampedShade;
        }
    };

    int axisRow = height / 2;
    float previousPhase = std::fmod(phaseHead, 1.0f);
    if (previousPhase < 0.0f) previousPhase += 1.0f;
    float previousSample = renderLfoSample(previousPhase, morph, duty);
    if (flip) previousSample = -previousSample;
    float previousNorm = (-previousSample + 1.0f) * 0.5f;
    int previousRow = static_cast<int>(std::round(previousNorm * (height - 1)));
    previousRow = std::clamp(previousRow, 0, height - 1);
    int previousCol = 0;
    applyPoint(previousCol, previousRow, 0);

    for (int x = 1; x < width; ++x) {
        float progress = static_cast<float>(x) / static_cast<float>(width - 1);
        float phase = std::fmod(phaseHead + progress, 1.0f);
        if (phase < 0.0f) phase += 1.0f;

        float sample = renderLfoSample(phase, morph, duty);
        if (flip) sample = -sample;

        float normalized = (-sample + 1.0f) * 0.5f;
        int row = static_cast<int>(std::round(normalized * (height - 1)));
        row = std::clamp(row, 0, height - 1);

        int currentShade = static_cast<int>(progress * (shadeCount - 1));

        int dx = x - previousCol;
        int dy = row - previousRow;
        int steps = std::max(std::abs(dx), std::abs(dy));
        if (steps == 0) {
            applyPoint(x, row, currentShade);
        } else {
            for (int step = 1; step <= steps; ++step) {
                int px = previousCol + (dx * step) / steps;
                int py = previousRow + (dy * step) / steps;
                float mix = static_cast<float>(previousCol + step) / static_cast<float>(width);
                int shade = static_cast<int>(mix * (shadeCount - 1));
                applyPoint(px, py, shade);
            }
        }

        previousCol = x;
        previousRow = row;
    }

    std::string horizontal(width, '-');
    mvprintw(topRow - 1, leftCol, "LFO Wave");
    mvprintw(topRow, leftCol, "+%s+", horizontal.c_str());
    for (int y = 0; y < height; ++y) {
        mvprintw(topRow + 1 + y, leftCol, "|");
        for (int x = 0; x < width; ++x) {
            if (shadeIndex[y][x] < shadeCount) {
                attron(shades[shadeIndex[y][x]].attr);
                mvaddch(topRow + 1 + y, leftCol + 1 + x, shades[shadeIndex[y][x]].ch);
                attroff(shades[shadeIndex[y][x]].attr);
            } else if (y == axisRow) {
                attron(COLOR_PAIR(6) | A_DIM);
                mvaddch(topRow + 1 + y, leftCol + 1 + x, '-');
                attroff(COLOR_PAIR(6) | A_DIM);
            } else {
                mvaddch(topRow + 1 + y, leftCol + 1 + x, ' ');
            }
        }
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

    float currentPhase = params->getLfoVisualPhase(currentLFOIndex);
    drawLFOWavePreview(previewTop, previewLeft, plotHeight, plotWidth, currentLFOIndex, currentPhase);

    int parameterCol = previewLeft + plotWidth + 6;
    drawParametersPage(previewTop, parameterCol);
}
