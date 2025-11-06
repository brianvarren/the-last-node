#include "../../ui.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

// PolyBLEP anti-aliasing (simplified for preview - no phaseInc needed for visual)
static inline float polyblep_preview(float phase) {
    // For preview purposes, assume small phase increment (high sample rate visual)
    const float phaseInc = 0.01f;
    if (phase < phaseInc) {
        float t = phase / phaseInc;
        return t + t - t * t - 1.0f;
    } else if (phase > 1.0f - phaseInc) {
        float t = (phase - 1.0f) / phaseInc;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// Generate sine wave (matches brainwave_osc.cpp)
static inline float generateSine(float phase, float morph) {
    const float kTwoPi = 2.0f * static_cast<float>(M_PI);
    float sine = std::sin(kTwoPi * phase);
    // Apply soft waveshaping when morph > 0
    float shaped = sine + 0.3f * morph * sine * sine * sine;
    return std::min(std::max(shaped, -1.0f), 1.0f);
}

// Generate triangle wave with PolyBLEP (matches brainwave_osc.cpp)
static inline float generateTriangle(float phase, float morph) {
    float pivot = 0.5f * (1.0f - morph) + morph * morph;
    float tri;
    if (phase < pivot) {
        tri = phase / pivot;
    } else {
        tri = 1.0f - (phase - pivot) / (1.0f - pivot);
    }
    tri = 2.0f * tri - 1.0f;

    float correction = 0.0f;
    if (phase < 0.01f || phase > 0.99f) {
        correction = polyblep_preview(phase);
    }
    return tri - 0.5f * correction;
}

// Generate sawtooth wave with PolyBLEP (matches brainwave_osc.cpp)
static inline float generateSaw(float phase) {
    float saw = 2.0f * phase - 1.0f;
    saw -= polyblep_preview(phase);
    return saw;
}

// Generate square/pulse wave with PolyBLEP (matches brainwave_osc.cpp)
static inline float generateSquare(float phase, float duty) {
    float square = (phase < duty) ? 1.0f : -1.0f;
    square += polyblep_preview(phase);

    float phaseShifted = phase + (1.0f - duty);
    if (phaseShifted >= 1.0f) phaseShifted -= 1.0f;
    square -= polyblep_preview(phaseShifted);

    return square;
}

// Generate waveform based on shape parameter (0.0-1.0)
// Matches the actual BrainwaveOscillator::generateSample implementation
float computeWaveSample(float phase, float shapePos) {
    // 5-shape continuum: Sine → Triangle → Saw → Square → Pulse
    if (shapePos < 0.2f) {
        // Sine wave - use shapePos/0.2 for subtle waveshaping
        float waveshape = shapePos / 0.2f;
        return generateSine(phase, waveshape);
    } else if (shapePos < 0.4f) {
        // Triangle - use normalized position for slope asymmetry
        float asymmetry = (shapePos - 0.2f) / 0.2f;
        return generateTriangle(phase, asymmetry);
    } else if (shapePos < 0.6f) {
        // Sawtooth
        return generateSaw(phase);
    } else if (shapePos < 0.8f) {
        // Square wave (50% duty)
        return generateSquare(phase, 0.5f);
    } else {
        // Pulse - duty narrows from 50% to ~5% as shapePos goes 0.8 → 1.0
        float pulseAmount = (shapePos - 0.8f) / 0.2f;
        float duty = 0.5f - 0.45f * pulseAmount;
        return generateSquare(phase, duty);
    }
}

} // namespace

void UI::drawOscillatorWavePreview(int topRow, int leftCol, int plotHeight, int plotWidth) {
    float shape = params->getOscShape(currentOscillatorIndex);
    shape = std::min(std::max(shape, 0.0f), 1.0f);

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
        float sample = computeWaveSample(phase, shape);
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
