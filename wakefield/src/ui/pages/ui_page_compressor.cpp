#include "../../ui.h"
#include "../../synth.h"
#include <cmath>

void UI::drawCompressorPage() {
    int row = 2;
    attron(COLOR_PAGE_TITLE | A_BOLD);
    mvprintw(row, 2, "COMPRESSOR/LIMITER");
    attroff(COLOR_PAGE_TITLE | A_BOLD);
    row += 2;

    // Get parameters
    bool enabled = params->compressorEnabled.load();
    float threshold = params->compressorThreshold.load();
    float ratio = params->compressorRatio.load();
    float attack = params->compressorAttack.load();
    float release = params->compressorRelease.load();
    float knee = params->compressorKnee.load();
    float mix = params->compressorMix.load();
    bool autoMakeup = params->compressorAutoMakeup.load();
    bool rmsMode = params->compressorRMS.load();

    // Get gain reduction for metering
    float gainReduction = 0.0f;
    if (synth && enabled) {
        gainReduction = synth->getCompressorGainReduction();
    }

    // Draw gain reduction meter
    const int meterWidth = 60;
    const int meterHeight = 3;
    const int meterLeft = 4;
    const int meterTop = row;

    // Title
    attron(COLOR_LOCKED);
    mvprintw(meterTop, meterLeft, "Gain Reduction Meter");
    attroff(COLOR_LOCKED);

    // Draw meter background
    std::string meterBg(meterWidth, '-');
    mvprintw(meterTop + 1, meterLeft, "[%s]", meterBg.c_str());

    // Draw gain reduction bar (0 to -24 dB range)
    if (enabled && gainReduction < 0.0f) {
        int barLength = static_cast<int>(std::abs(gainReduction) / 24.0f * meterWidth);
        barLength = std::clamp(barLength, 0, meterWidth);

        // Color based on amount of reduction
        int color = COLOR_LABEL;
        if (gainReduction < -12.0f) {
            color = COLOR_SECTION_HEADER;  // Heavy compression
        } else if (gainReduction < -6.0f) {
            color = COLOR_VALUE;            // Moderate compression
        }

        attron(color | A_BOLD);
        for (int i = 0; i < barLength; ++i) {
            mvprintw(meterTop + 1, meterLeft + 1 + i, "=");
        }
        attroff(color | A_BOLD);

        // Show numeric value
        mvprintw(meterTop + 2, meterLeft, "GR: %.1f dB", gainReduction);

        // Show makeup gain if auto makeup is enabled
        if (autoMakeup && synth) {
            float makeupGain = synth->getCompressorGainReduction();  // This will need a getMakeupGain method
            mvprintw(meterTop + 2, meterLeft + 20, "Auto Makeup: ON");
        }
    } else {
        attron(COLOR_LOCKED);
        mvprintw(meterTop + 2, meterLeft, "GR: 0.0 dB (No Reduction)");
        attroff(COLOR_LOCKED);
    }

    if (!enabled) {
        attron(COLOR_LOCKED | A_BOLD);
        mvprintw(meterTop + 1, meterLeft + meterWidth/2 - 7, " COMPRESSOR OFF ");
        attroff(COLOR_LOCKED | A_BOLD);
    }

    // Draw parameter list on the right
    int parameterCol = meterLeft + meterWidth + 8;
    int parameterTop = meterTop;

    std::vector<int> compressorParams = getCompressorParameterIds();
    if (!compressorParams.empty()) {
        // Make sure selectedParameterId is valid for this page
        if (std::find(compressorParams.begin(), compressorParams.end(), selectedParameterId) == compressorParams.end()) {
            selectedParameterId = compressorParams.front();
        }
        drawParameterList(parameterTop, parameterCol, compressorParams);
    }

    // Draw info box below meter
    row = meterTop + meterHeight + 2;
    attron(COLOR_LOCKED);
    mvprintw(row++, meterLeft, "COMPRESSOR INFO:");
    attroff(COLOR_LOCKED);

    mvprintw(row++, meterLeft, "Threshold: %.1f dB | Ratio: %.1f:1 %s",
             threshold, ratio, ratio >= 20.0f ? "(LIMITING)" : "");
    mvprintw(row++, meterLeft, "Attack: %.1f ms | Release: %.1f ms",
             attack, release);
    mvprintw(row++, meterLeft, "Knee: %.1f dB | Mix: %.0f%%",
             knee, mix * 100.0f);
    mvprintw(row++, meterLeft, "Detection: %s | Auto Makeup: %s",
             rmsMode ? "RMS" : "Peak", autoMakeup ? "ON" : "OFF");

    row += 2;
    attron(COLOR_LOCKED);
    mvprintw(row++, meterLeft, "FEATURES:");
    attroff(COLOR_LOCKED);
    mvprintw(row++, meterLeft, "• Stereo-linked compression for cohesive imaging");
    mvprintw(row++, meterLeft, "• Automatic makeup gain maintains perceived loudness");
    mvprintw(row++, meterLeft, "• Soft-knee compression for musical transitions");
    mvprintw(row++, meterLeft, "• Integrated soft clipping prevents digital clipping");
    mvprintw(row++, meterLeft, "• Peak/RMS detection modes for different material");
    mvprintw(row++, meterLeft, "• Dry/wet mix for parallel compression");

    row += 2;
    attron(COLOR_LOCKED);
    mvprintw(row++, meterLeft, "TIPS:");
    attroff(COLOR_LOCKED);
    mvprintw(row++, meterLeft, "• Start with 4:1 ratio and adjust threshold to taste");
    mvprintw(row++, meterLeft, "• Use fast attack (1-5ms) for transient control");
    mvprintw(row++, meterLeft, "• Use slow attack (10-30ms) to preserve punch");
    mvprintw(row++, meterLeft, "• Set ratio to 20:1 or higher for limiting");
    mvprintw(row++, meterLeft, "• Use RMS mode for smooth, consistent compression");
    mvprintw(row++, meterLeft, "• Use Peak mode for fast transient control");
}
