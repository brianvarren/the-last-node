#include "../../ui.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

float computePhaseDistorted(float phase, float morph) {
    float d = std::max(0.0001f, std::min(morph, 0.5f));

    float shapedPhase;
    if (phase <= d) {
        shapedPhase = phase / (2.0f * d);
    } else {
        float denom = std::max(1e-6f, 1.0f - d);
        shapedPhase = 0.5f * (1.0f + ((phase - d) / denom));
    }

    return -std::cos(shapedPhase * 2.0f * static_cast<float>(M_PI));
}

float computeTanhShaped(float phase, float morph, float duty) {
    const float twoPi = 2.0f * static_cast<float>(M_PI);
    float sine = std::sin(twoPi * phase);
    float edge = (morph - 0.5f) * 2.0f;
    edge = std::min(std::max(edge, 0.0f), 1.0f);

    if (edge < 1e-3f) {
        return sine;
    }

    float theta = twoPi * (duty - 0.5f);
    float x = sine - std::sin(theta);
    float beta = 1.0f + 40.0f * edge;
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

    int width = plotWidth + static_cast<int>(plotWidth * 0.15f);
    int height = plotHeight + std::max(1, static_cast<int>(plotHeight * 0.15f));
    width = std::max(width, plotWidth);
    height = std::max(height, 4);

    std::vector<std::string> grid(height, std::string(width, ' '));

    int axisRow = height / 2;
    for (int x = 0; x < width; ++x) {
        grid[axisRow][x] = '-';
    }

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
        grid[row][x] = '*';
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
    // Draw oscillator selector at top
    int row = 3;
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row, 2, "Select Oscillator (1-4): ");
    attroff(COLOR_PAIR(1) | A_BOLD);

    // Draw oscillator buttons
    for (int i = 0; i < 4; ++i) {
        if (i == currentOscillatorIndex) {
            attron(COLOR_PAIR(5) | A_BOLD);  // Highlight selected
            mvprintw(row, 28 + (i * 4), "[%d]", i + 1);
            attroff(COLOR_PAIR(5) | A_BOLD);
        } else {
            attron(COLOR_PAIR(6));
            mvprintw(row, 28 + (i * 4), " %d ", i + 1);
            attroff(COLOR_PAIR(6));
        }
    }

    row += 2;
    attron(COLOR_PAIR(1));
    mvprintw(row, 2, "Oscillator %d Parameters:", currentOscillatorIndex + 1);
    attroff(COLOR_PAIR(1));

    // Draw parameters for the currently selected oscillator
    drawParametersPage(row + 2);

    // Draw waveform preview to the right
    int cols = getmaxx(stdscr);
    const int plotWidth = 28;
    const int plotHeight = 10;
    int leftCol = std::max(50, cols - (plotWidth + 4));
    int plotTop = row + 2;
    drawOscillatorWavePreview(plotTop, leftCol, plotHeight, plotWidth);
}
