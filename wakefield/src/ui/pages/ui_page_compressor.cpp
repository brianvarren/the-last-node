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

    // Get output level (placeholder until proper metering exposed)
    float outputLevel = 0.5f;

    int maxY = getmaxy(stdscr);
    int maxX = getmaxx(stdscr);
    const int leftMargin = 2;
    const int meterLeft = leftMargin + 2;
    // Allocate room for right parameter column (~40 cols) and spacing
    int rightCol = std::max(40, maxX / 2);
    int availableWidth = std::max(20, rightCol - meterLeft - 4);
    int meterWidth = std::min(40, availableWidth);
    const int meterHeight = 3;
    int meterTop = row;

    // Output Level Meter
    attron(COLOR_LOCKED);
    mvprintw(meterTop, meterLeft, "Output Level");
    attroff(COLOR_LOCKED);

    std::string outputBg(meterWidth, '-');
    mvprintw(meterTop + 1, meterLeft, "[%s]", outputBg.c_str());

    // Draw output level bar (0 to 1.0 range, showing 0 to -40 dB)
    if (outputLevel > 0.0f) {
        float outputDB = 20.0f * std::log10(std::max(outputLevel, 1e-6f));
        float normalizedLevel = std::clamp((outputDB + 40.0f) / 40.0f, 0.0f, 1.0f);  // Map -40 to 0 dB -> 0 to 1
        int barLength = static_cast<int>(normalizedLevel * meterWidth);
        barLength = std::clamp(barLength, 0, meterWidth);

        // Color based on level
        int color = COLOR_VALUE;
        if (outputDB > -3.0f) {
            color = COLOR_SECTION_HEADER;  // Red zone (near clipping)
        } else if (outputDB > -6.0f) {
            color = COLOR_STATUS_ACTIVE;  // Yellow zone
        }

        attron(color | A_BOLD);
        for (int i = 0; i < barLength; ++i) {
            mvprintw(meterTop + 1, meterLeft + 1 + i, "=");
        }
        attroff(color | A_BOLD);

        mvprintw(meterTop + 2, meterLeft, "Level: %6.1f dB", outputDB);
    } else {
        mvprintw(meterTop + 2, meterLeft, "Level: -inf dB");
    }

    meterTop += 4;  // Move down for gain reduction meter

    // Gain Reduction Meter Title
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

        // Show numeric value (right-aligned field)
        mvprintw(meterTop + 2, meterLeft, "GR: %6.1f dB", gainReduction);
        if (autoMakeup) {
            mvprintw(meterTop + 2, meterLeft + 18, "Auto Makeup: ON");
        }
    } else {
        attron(COLOR_LOCKED);
        mvprintw(meterTop + 2, meterLeft, "GR:   0.0 dB");
        attroff(COLOR_LOCKED);
    }

    if (!enabled) {
        attron(COLOR_LOCKED | A_BOLD);
        int cx = meterLeft + std::max(0, meterWidth/2 - 7);
        mvprintw(meterTop + 1, cx, " COMPRESSOR OFF ");
        attroff(COLOR_LOCKED | A_BOLD);
    }

    // Draw parameter list on the right (fits grid within one page)
    int parameterCol = rightCol;
    int parameterTop = row;

    std::vector<int> compressorParams = getCompressorParameterIds();
    if (!compressorParams.empty()) {
        // Make sure selectedParameterId is valid for this page
        if (std::find(compressorParams.begin(), compressorParams.end(), selectedParameterId) == compressorParams.end()) {
            selectedParameterId = compressorParams.front();
        }
        drawParameterList(parameterTop, parameterCol, compressorParams);
    }

    // Compact info line below meters (single page, grid-aligned)
    int infoRow = meterTop + meterHeight + 2;
    mvprintw(infoRow++, meterLeft, "Thresh:%6.1f dB  Ratio:%5.1f:1 %s",
             threshold, ratio, ratio >= 20.0f ? "(LIMIT)" : "");
    mvprintw(infoRow++, meterLeft, "Attack:%6.1f ms  Release:%6.1f ms  Knee:%5.1f dB  Mix:%3.0f%%",
             attack, release, knee, mix * 100.0f);
    mvprintw(infoRow++, meterLeft, "Detect:%s  AutoMakeup:%s",
             rmsMode ? "RMS" : "Peak", autoMakeup ? "ON" : "OFF");
}
