#include "../../ui.h"
#include "../../synth.h"
#include "../../filters.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

namespace {

float simulateResponse(int type,
                       float cutoff,
                       float gainDb,
                       float resonance,
                       float drive,
                       float feedbackHP,
                       float spread,
                       float notchFeedback,
                       float width,
                       float freq,
                       float sampleRate) {
    const float nyquist = 0.5f * sampleRate;
    if (freq <= 0.0f || freq >= nyquist) {
        return 0.0f;
    }

    constexpr int totalSamples = 512;
    constexpr int settleSamples = 256;
    const float omega = 2.0f * static_cast<float>(M_PI) * freq / sampleRate;
    auto sineSample = [&](int n) {
        return std::sin(omega * static_cast<float>(n));
    };

    float sumSquares = 0.0f;
    int count = 0;

    if (type == 0 || type == 1) {
        OnePoleTPT filt(sampleRate);
        filt.setCutoff(cutoff);
        for (int n = 0; n < totalSamples; ++n) {
            float x = sineSample(n);
            auto [lp, hp] = filt.process(x);
            float y = (type == 0) ? lp : hp;
            if (n >= settleSamples) {
                sumSquares += y * y;
                ++count;
            }
        }
    } else if (type == 2) {
        OnePoleHighShelfBLT filt;
        filt.setSampleRate(sampleRate);
        filt.setCutoff(cutoff);
        filt.setGainDb(gainDb);
        for (int n = 0; n < totalSamples; ++n) {
            float x = sineSample(n);
            float y = filt.process(x);
            if (n >= settleSamples) {
                sumSquares += y * y;
                ++count;
            }
        }
    } else if (type == 3) {
        OnePoleLowShelfBLT filt;
        filt.setSampleRate(sampleRate);
        filt.setCutoff(cutoff);
        filt.setGainDb(gainDb);
        for (int n = 0; n < totalSamples; ++n) {
            float x = sineSample(n);
            float y = filt.process(x);
            if (n >= settleSamples) {
                sumSquares += y * y;
                ++count;
            }
        }
    } else if (type == 4) {
        Ladder8PoleZdf ladder(sampleRate);
        ladder.setCutoff(cutoff);
        ladder.setResonance(resonance);
        ladder.setDrive(drive);
        ladder.setFeedbackHighpass(feedbackHP);
        const float inputLevel = 0.25f;
        for (int n = 0; n < totalSamples; ++n) {
            float x = sineSample(n) * inputLevel;
            float y = ladder.process(x);
            if (n >= settleSamples) {
                sumSquares += y * y;
                ++count;
            }
        }
    } else if (type == 5) {
        LadderDiodeZdf ladder(sampleRate);
        ladder.setCutoff(cutoff);
        ladder.setResonance(resonance);
        ladder.setDrive(drive);
        ladder.setFeedbackHighpass(feedbackHP);
        const float inputLevel = 0.25f;
        for (int n = 0; n < totalSamples; ++n) {
            float x = sineSample(n) * inputLevel;
            float y = ladder.process(x);
            if (n >= settleSamples) {
                sumSquares += y * y;
                ++count;
            }
        }
    } else if (type == 6) {
        LadderBandpassZdf ladder(sampleRate);
        ladder.setCutoff(cutoff);
        ladder.setResonance(resonance);
        ladder.setDrive(drive);
        ladder.setFeedbackHighpass(feedbackHP);
        ladder.setWidth(width);
        const float inputLevel = 0.25f;
        for (int n = 0; n < totalSamples; ++n) {
            float x = sineSample(n) * inputLevel;
            float y = ladder.process(x);
            if (n >= settleSamples) {
                sumSquares += y * y;
                ++count;
            }
        }
    } else if (type == 7) {
        DualNotchZdf notch(sampleRate);
        notch.setCutoff(cutoff);
        notch.setSpread(spread);
        notch.setResonance(resonance);
        notch.setDrive(drive);
        notch.setNotchFeedback(notchFeedback);
        for (int n = 0; n < totalSamples; ++n) {
            float x = sineSample(n);
            float y = notch.process(x);
            if (n >= settleSamples) {
                sumSquares += y * y;
                ++count;
            }
        }
    } else if (type == 8) {
        Bandpass2PoleZdf bp(sampleRate);
        bp.setCutoff(cutoff);
        bp.setResonance(resonance);
        bp.setDrive(drive);
        bp.setWidth(width);
        const float inputLevel = 0.25f;
        for (int n = 0; n < totalSamples; ++n) {
            float x = sineSample(n) * inputLevel;
            float y = bp.process(x);
            if (n >= settleSamples) {
                sumSquares += y * y;
                ++count;
            }
        }
    } else if (type == 9) {
        Ladder8PoleBandpassZdf ladder(sampleRate);
        ladder.setCutoff(cutoff);
        ladder.setResonance(resonance);
        ladder.setDrive(drive);
        ladder.setWidth(width);
        const float inputLevel = 0.25f;
        for (int n = 0; n < totalSamples; ++n) {
            float x = sineSample(n) * inputLevel;
            float y = ladder.process(x);
            if (n >= settleSamples) {
                sumSquares += y * y;
                ++count;
            }
        }
    }

    if (count <= 0) return 0.0f;
    float rms = std::sqrt(sumSquares / static_cast<float>(count));
    return rms;
}

void drawFilterResponsePreview(int topRow,
                               int leftCol,
                               int plotHeight,
                               int plotWidth,
                               bool enabled,
                               int type,
                               float cutoff,
                               float gainDb,
                               float resonance,
                               float drive,
                               float feedbackHP,
                               float spread,
                               float notchFeedback,
                               float width,
                               float sampleRate) {
    const float minHz = 20.0f;
    const float maxHz = std::min(20000.0f, 0.45f * sampleRate);
    const int plotW = std::max(16, plotWidth);
    const int height = std::max(8, plotHeight);

    std::vector<float> response(plotW, -80.0f);
    if (enabled) {
        for (int x = 0; x < plotW; ++x) {
            float t = (plotW == 1) ? 0.0f : static_cast<float>(x) / static_cast<float>(plotW - 1);
            float freq = minHz * std::pow(maxHz / minHz, t);
            float amp = simulateResponse(type, cutoff, gainDb, resonance, drive, feedbackHP,
                                         spread, notchFeedback, width, freq, sampleRate);
            float dB = 20.0f * std::log10(std::max(amp, 1e-5f));
            dB = std::clamp(dB, -80.0f, 12.0f);
            response[x] = dB;
        }
    }

    std::vector<std::string> grid(height, std::string(plotW, ' '));
    auto dbToRow = [&](float dB) {
        float normalized = (dB + 80.0f) / 92.0f;  // map [-80,12] -> [0,1]
        normalized = std::clamp(1.0f - normalized, 0.0f, 1.0f);
        int row = static_cast<int>(std::round(normalized * (height - 1)));
        return std::clamp(row, 0, height - 1);
    };

    for (int x = 0; x < plotW; ++x) {
        int row = dbToRow(response[x]);
        grid[row][x] = '*';
    }

    std::string horizontal(plotW, '-');
    mvprintw(topRow - 1, leftCol, "Filter Response");
    mvprintw(topRow, leftCol, "+%s+", horizontal.c_str());
    for (int y = 0; y < height; ++y) {
        mvprintw(topRow + 1 + y, leftCol, "|");
        mvprintw(topRow + 1 + y, leftCol + 1, "%s", grid[y].c_str());
        mvprintw(topRow + 1 + y, leftCol + 1 + plotW, "|");
    }
    mvprintw(topRow + 1 + height, leftCol, "+%s+", horizontal.c_str());

    if (!enabled) {
        attron(COLOR_PAIR(4) | A_BOLD);
        mvprintw(topRow + height / 2, leftCol + plotW / 2 - 6, "FILTER OFF");
        attroff(COLOR_PAIR(4) | A_BOLD);
    }
}

} // namespace

void UI::drawFilterPage() {
    int row = 2;
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(row, 2, "FILTER");
    attroff(COLOR_PAIR(1) | A_BOLD);
    row += 2;

    const int plotWidth = 42;
    const int plotHeight = 14;
    int previewLeft = 2;
    int previewTop = row;

    bool enabled = params->filterEnabled.load();
    int type = params->filterType.load();
    float cutoff = params->filterCutoff.load();
    float gainDb = params->filterGain.load();
    float resonance = params->filterResonance.load();
    float drive = params->filterDrive.load();
    float feedbackHP = params->filterFeedbackHP.load();
    float sampleRate = (audioSampleRate > 0) ? static_cast<float>(audioSampleRate) : 48000.0f;

    float spread = params->filterSpread.load();
    float notchFb = params->filterNotchFeedback.load();

    drawFilterResponsePreview(previewTop, previewLeft, plotHeight, plotWidth,
                              enabled, type, cutoff, gainDb, resonance, drive, feedbackHP,
                              spread, notchFb, params->filterBandWidth.load(), sampleRate);

    int parameterCol = previewLeft + plotWidth + 6;

    std::vector<int> filterParams = getFilterParameterIds();
    int paramCount = 0;
    if (!filterParams.empty()) {
        if (std::find(filterParams.begin(), filterParams.end(), selectedParameterId) == filterParams.end()) {
            selectedParameterId = filterParams.front();
        }
        drawParameterList(previewTop, parameterCol, filterParams);
        paramCount = filterParams.size();
    }

    // Show debug info for 2-pole bandpass (type 8) below parameters
    if (type == 8 && synth) {
        const auto* bp = synth->getBandpass2FilterL();
        if (bp) {
            int debugRow = previewTop + paramCount + 2;  // 2 lines below parameters
            attron(COLOR_PAIR(4));
            mvprintw(debugRow++, parameterCol, "=== 2-Pole BP Debug ===");
            attroff(COLOR_PAIR(4));
            mvprintw(debugRow++, parameterCol, "g (tan w/2): %.6f", bp->getG());
            mvprintw(debugRow++, parameterCol, "fc (Hz):     %.2f", bp->getCenterFreq());
            mvprintw(debugRow++, parameterCol, "widthOct:    %.4f", bp->getWidthOct());
            mvprintw(debugRow++, parameterCol, "ratio:       %.4f", bp->getWidthRatio());
            mvprintw(debugRow++, parameterCol, "TwoR:        %.4f", bp->getTwoR());
            mvprintw(debugRow++, parameterCol, "damping R:   %.4f", bp->getDamping());
            mvprintw(debugRow++, parameterCol, "normGain:    %.4f", bp->getNormGain());
        }
    }

    // Show MIDI Learn status if active on filter cutoff
    if (params->midiLearnActive.load() && params->midiLearnParameterId.load() == 32) {
        int statusRow = previewTop + plotHeight + 4;
        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(statusRow++, 2, ">>> MIDI LEARN ACTIVE <<<");
        attroff(COLOR_PAIR(3) | A_BOLD);
        mvprintw(statusRow++, 2, "Move a MIDI controller to assign it to Filter Cutoff");
    }
}
