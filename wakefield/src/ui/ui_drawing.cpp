#include "../ui.h"
#include "../sequencer.h"
#include "ui_utils.h"
#include <algorithm>
#include <string>

void UI::drawTabs() {
    int cols = getmaxx(stdscr);

    struct TabInfo {
        const char* label;
        UIPage page;
    };

    const TabInfo tabs[] = {
        {"MAIN", UIPage::MAIN},
        {"OSC", UIPage::OSCILLATOR},
        {"SAMP", UIPage::SAMPLER},
        {"MIX", UIPage::MIXER},
        {"LFO", UIPage::LFO},
        {"ENV", UIPage::ENV},
        {"FILTER", UIPage::FILTER},
        {"REVERB", UIPage::REVERB},
        {"CHAOS", UIPage::CHAOS},
        {"MOD", UIPage::MOD},
        {"FM", UIPage::FM},
        {"SEQUENCER", UIPage::SEQUENCER},
        {"CONFIG", UIPage::CONFIG}
    };

    int x = 0;
    const int tabCount = static_cast<int>(sizeof(tabs) / sizeof(tabs[0]));
    for (int i = 0; i < tabCount; ++i) {
        std::string text = " " + std::string(tabs[i].label) + " ";
        int textLen = static_cast<int>(text.size());

        if (currentPage == tabs[i].page) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(0, x, "%s", text.c_str());
            attroff(COLOR_PAIR(5) | A_BOLD);
        } else {
            attron(COLOR_PAIR(6));
            mvprintw(0, x, "%s", text.c_str());
            attroff(COLOR_PAIR(6));
        }

        x += textLen;
        if (i < tabCount - 1) {
            mvaddch(0, x, ' ');
            ++x;
        }
    }

    // Fill rest of line
    for (int i = x; i < cols; ++i) {
        mvaddch(0, i, ' ');
    }

    // Draw separator line
    attron(COLOR_PAIR(1));
    mvhline(1, 0, '-', cols);
    attroff(COLOR_PAIR(1));
}

void UI::drawBar(int y, int x, const char* label, float value, float min, float max, int width) {
    float normalized = (value - min) / (max - min);
    int fillWidth = static_cast<int>(normalized * width);

    mvprintw(y, x, "%s", label);

    int labelLen = 0;
    while (label[labelLen] != '\0') labelLen++;

    int barStart = x + labelLen + 1;

    mvaddch(y, barStart, '[');
    attron(COLOR_PAIR(2));
    for (int i = 0; i < width; ++i) {
        if (i < fillWidth) {
            mvaddch(y, barStart + 1 + i, '=');
        } else {
            mvaddch(y, barStart + 1 + i, ' ');
        }
    }
    attroff(COLOR_PAIR(2));
    mvaddch(y, barStart + width + 1, ']');

    mvprintw(y, barStart + width + 3, "%.3f", value);
}

void UI::drawCPUOverlay() {
    if (!cpuMonitor.isEnabled()) {
        return;
    }

    int maxX = getmaxx(stdscr);
    float cpuUsage = cpuMonitor.getCPUUsage();

    // Draw in top-right corner, right after the tabs
    int x = maxX - 15;  // Reserve 15 chars for "CPU: 100.0%"

    if (x < 0) {
        return;  // Terminal too narrow
    }

    // Format: "CPU: XX.X%"
    attron(COLOR_PAIR(1));
    mvprintw(0, x, "CPU:");
    attroff(COLOR_PAIR(1));

    // Color code based on usage
    int colorPair;
    if (cpuUsage < 50.0f) {
        colorPair = 2;  // Green - low usage
    } else if (cpuUsage < 80.0f) {
        colorPair = 3;  // Yellow - medium usage
    } else {
        colorPair = 4;  // Red - high usage
    }

    attron(COLOR_PAIR(colorPair) | A_BOLD);
    mvprintw(0, x + 5, "%5.1f%%", cpuUsage);
    attroff(COLOR_PAIR(colorPair) | A_BOLD);
}

void UI::drawHotkeyLine() {
    int maxY = getmaxy(stdscr);
    int maxX = getmaxx(stdscr);
    int row = maxY - 1;

    attron(COLOR_PAIR(1));
    mvhline(row - 1, 0, '-', maxX);
    attroff(COLOR_PAIR(1));

    if (numericInputActive) {
        mvprintw(row, 1, "Type value  |  Enter Confirm  |  Esc Cancel  |  Q Quit");
    } else if (sequencerScaleMenuActive) {
        mvprintw(row, 1, "Up/Down Choose Scale  |  Enter Confirm  |  Esc Cancel  |  Q Quit");
    } else if (textInputActive) {
        mvprintw(row, 1, "Type preset name  |  Enter Save  |  Esc Cancel  |  Q Quit");
    } else if (params->midiLearnActive.load()) {
        mvprintw(row, 1, "Move MIDI controller to assign  |  Esc Cancel  |  Q Quit");
    } else {
        if (currentPage == UIPage::FM) {
            mvprintw(row, 1, "Tab Page  |  Arrows Nav  |  Left/Right Adjust  |  Enter Type  |  l Lock  |  G/M/R Ops  |  Q Quit");
        } else if (currentPage == UIPage::MOD) {
            mvprintw(row, 1, "Tab Page  |  Arrows Nav  |  Enter Edit  |  l Lock Slot  |  Q Quit");
        } else {
            mvprintw(row, 1, "Tab Page  |  Up/Dn Param  |  Left/Right Adjust  |  Enter Type  |  l Lock  |  L Learn  |  G/M/R Page Ops  |  Q Quit");
        }
    }
}

void UI::draw(int activeVoices) {
    erase();  // Use erase() instead of clear() - doesn't cause flicker

    // If help is active, show help instead of normal UI
    if (helpActive) {
        drawHelpPage();
        refresh();
        return;
    }

    drawTabs();
    drawCPUOverlay();  // Always draw CPU overlay on top bar

    // Draw MIDI keyboard mode indicator (center of top bar)
    if (midiKeyboardMode) {
        int maxX = getmaxx(stdscr);
        std::string indicator = " [MIDI KB: OCT" + std::to_string(midiKeyboardOctave) + "] ";
        int indicatorLen = static_cast<int>(indicator.length());
        int x = (maxX - indicatorLen) / 2;  // Center it

        attron(COLOR_PAIR(4) | A_BOLD | A_REVERSE);  // Red/yellow bold reverse (attention-grabbing)
        mvprintw(0, x, "%s", indicator.c_str());
        attroff(COLOR_PAIR(4) | A_BOLD | A_REVERSE);
    }

    switch (currentPage) {
        case UIPage::MAIN:
            drawMainPage();
            break;
        case UIPage::OSCILLATOR:
            drawOscillatorPage();
            break;
        case UIPage::SAMPLER:
            drawSamplerPage();
            break;
        case UIPage::MIXER:
            drawMixerPage();
            break;
        case UIPage::LFO:
            drawLFOPage();
            break;
        case UIPage::ENV:
            drawEnvelopePage();
            break;
        case UIPage::FM:
            drawFMPage();
            break;
        case UIPage::MOD:
            drawModPage();
            break;
        case UIPage::REVERB:
            drawReverbPage();
            break;
        case UIPage::FILTER:
            drawFilterPage();
            break;
        case UIPage::SEQUENCER:
            drawSequencerPage();
            break;
        case UIPage::CHAOS:
            drawChaosPage();
            break;
        case UIPage::CONFIG:
            drawConfigPage();
            break;
    }

    drawHotkeyLine();  // Always draw hotkey line at bottom

    // Draw input overlays if active
    if (textInputActive) {
        int maxY = getmaxy(stdscr);
        int maxX = getmaxx(stdscr);
        int height = 7;
        int width = 50;
        int startY = (maxY - height) / 2;
        int startX = (maxX - width) / 2;

        // Draw box
        attron(COLOR_PAIR(5));
        for (int y = startY; y < startY + height; ++y) {
            for (int x = startX; x < startX + width; ++x) {
                mvaddch(y, x, ' ');
            }
        }
        attroff(COLOR_PAIR(5));

        // Draw border
        attron(A_BOLD);
        mvhline(startY, startX, '-', width);
        mvhline(startY + height - 1, startX, '-', width);
        mvvline(startY, startX, '|', height);
        mvvline(startY, startX + width - 1, '|', height);
        attroff(A_BOLD);

        // Draw title
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(startY + 1, startX + 2, "Save Preset As:");
        attroff(COLOR_PAIR(5) | A_BOLD);

        // Draw input field
        mvprintw(startY + 3, startX + 2, "> %s_", textInputBuffer.c_str());

        // Draw hint
        attron(COLOR_PAIR(3));
        mvprintw(startY + 5, startX + 2, "Press Enter to save, Esc to cancel");
        attroff(COLOR_PAIR(3));
    } else if (numericInputActive) {
        int maxY = getmaxy(stdscr);
        int maxX = getmaxx(stdscr);
        int height = 7;
        int width = 40;
        int startY = (maxY - height) / 2;
        int startX = (maxX - width) / 2;

        // Draw box
        attron(COLOR_PAIR(5));
        for (int y = startY; y < startY + height; ++y) {
            for (int x = startX; x < startX + width; ++x) {
                mvaddch(y, x, ' ');
            }
        }
        attroff(COLOR_PAIR(5));

        // Draw border
        attron(A_BOLD);
        mvhline(startY, startX, '-', width);
        mvhline(startY + height - 1, startX, '-', width);
        mvvline(startY, startX, '|', height);
        mvvline(startY, startX + width - 1, '|', height);
        attroff(A_BOLD);

        const char* label = nullptr;
        if (numericInputIsMod) {
            label = "Modulation Amount (-99 to 99)";
        } else if (numericInputIsSequencer) {
            switch (sequencerNumericContext.field) {
                case SequencerNumericField::NOTE: label = "MIDI Note / Name"; break;
                case SequencerNumericField::VELOCITY: label = "Velocity (0-127)"; break;
                case SequencerNumericField::GATE: label = "Gate % (0-200)"; break;
                case SequencerNumericField::PROBABILITY: label = "Probability % (0-100)"; break;
                case SequencerNumericField::TEMPO: label = "Tempo BPM (0.1-999)"; break;
                case SequencerNumericField::SCALE: label = "Scale"; break;
                case SequencerNumericField::ROOT: label = "Root Note"; break;
                case SequencerNumericField::EUCLID_HITS: label = "Euclid Hits"; break;
                case SequencerNumericField::EUCLID_STEPS: label = "Euclid Steps"; break;
                case SequencerNumericField::EUCLID_ROTATION: label = "Euclid Rotation"; break;
                case SequencerNumericField::SUBDIVISION: label = "Subdivision (1/..)"; break;
                case SequencerNumericField::MUTATE_AMOUNT: label = "Mutate % (0-100)"; break;
                case SequencerNumericField::MUTED: label = "Muted (on/off)"; break;
                case SequencerNumericField::SOLO: label = "Solo (on/off)"; break;
                default: label = "Value"; break;
            }
        } else {
            InlineParameter* param = getParameter(selectedParameterId);
            if (param) {
                label = param->name.c_str();
            }
        }

        if (label) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(startY + 1, startX + 2, "Enter %s:", label);
            attroff(COLOR_PAIR(5) | A_BOLD);
        }

        // Draw input field
        mvprintw(startY + 3, startX + 2, "> %s_", numericInputBuffer.c_str());

        // Draw hint
        attron(COLOR_PAIR(3));
        if (numericInputIsSequencer) {
            mvprintw(startY + 5, startX + 2, "Enter value, Esc cancel");
        } else {
            mvprintw(startY + 5, startX + 2, "Press Enter to confirm, Esc to cancel");
        }
        attroff(COLOR_PAIR(3));
    } else if (sequencerScaleMenuActive) {
        int maxY = getmaxy(stdscr);
        int maxX = getmaxx(stdscr);
        int optionCount = static_cast<int>(UIUtils::kScaleOrder.size());
        int height = optionCount + 4;
        if (height < 8) {
            height = 8;
        }
        int width = 36;
        int startY = std::max(1, (maxY - height) / 2);
        int startX = std::max(1, (maxX - width) / 2);

        for (int y = startY; y < startY + height; ++y) {
            for (int x = startX; x < startX + width; ++x) {
                mvaddch(y, x, ' ');
            }
        }

        attron(A_BOLD);
        mvhline(startY, startX, '-', width);
        mvhline(startY + height - 1, startX, '-', width);
        mvvline(startY, startX, '|', height);
        mvvline(startY, startX + width - 1, '|', height);
        mvprintw(startY + 1, startX + 2, "Select Scale");
        attroff(A_BOLD);

        for (int i = 0; i < optionCount; ++i) {
            int y = startY + 3 + i;
            const char* name = UIUtils::scaleDisplayName(UIUtils::kScaleOrder[i]);
            bool selected = (i == sequencerScaleMenuIndex);
            if (selected) {
                attron(COLOR_PAIR(5) | A_BOLD);
                mvprintw(y, startX + 2, "> %-20s", name);
                attroff(COLOR_PAIR(5) | A_BOLD);
            } else {
                mvprintw(y, startX + 2, "  %-20s", name);
            }
        }

        attron(COLOR_PAIR(3));
        mvprintw(startY + height - 2, startX + 2, "Enter confirm, Esc cancel");
        attroff(COLOR_PAIR(3));
    } else if (sampleBrowserActive) {
        int maxY = getmaxy(stdscr);
        int maxX = getmaxx(stdscr);
        const int maxVisible = 20;
        int totalItems = sampleBrowserDirs.size() + sampleBrowserFiles.size();
        int height = std::min(maxVisible + 4, totalItems + 4);
        if (height < 8) height = 8;
        int width = 60;
        int startY = std::max(1, (maxY - height) / 2);
        int startX = std::max(1, (maxX - width) / 2);

        // Draw background
        for (int y = startY; y < startY + height; ++y) {
            for (int x = startX; x < startX + width; ++x) {
                mvaddch(y, x, ' ');
            }
        }

        // Draw border
        attron(A_BOLD);
        mvhline(startY, startX, '-', width);
        mvhline(startY + height - 1, startX, '-', width);
        mvvline(startY, startX, '|', height);
        mvvline(startY, startX + width - 1, '|', height);
        attroff(A_BOLD);

        // Draw title
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(startY + 1, startX + 2, "Load Sample - %s", sampleBrowserCurrentDir.c_str());
        attroff(COLOR_PAIR(5) | A_BOLD);

        // Draw items
        int displayRow = startY + 3;
        int endIndex = std::min(sampleBrowserScrollOffset + maxVisible, totalItems);

        for (int i = sampleBrowserScrollOffset; i < endIndex; ++i) {
            bool isSelected = (i == sampleBrowserSelectedIndex);
            bool isDirectory = (i < static_cast<int>(sampleBrowserDirs.size()));

            std::string displayName;
            if (isDirectory) {
                displayName = "[" + sampleBrowserDirs[i] + "]";
            } else {
                int fileIndex = i - sampleBrowserDirs.size();
                displayName = sampleBrowserFiles[fileIndex];
            }

            // Truncate if too long
            if (displayName.length() > 54) {
                displayName = displayName.substr(0, 51) + "...";
            }

            if (isSelected) {
                attron(COLOR_PAIR(5) | A_BOLD);
                mvprintw(displayRow, startX + 2, "> %-54s", displayName.c_str());
                attroff(COLOR_PAIR(5) | A_BOLD);
            } else {
                mvprintw(displayRow, startX + 2, "  %-54s", displayName.c_str());
            }

            displayRow++;
        }

        // Draw hint
        attron(COLOR_PAIR(3));
        mvprintw(startY + height - 2, startX + 2, "Enter: Select | Arrows: Navigate | Esc: Cancel");
        attroff(COLOR_PAIR(3));
    }

    refresh();
}

void UI::drawMainPage() {
    int maxY = getmaxy(stdscr);
    int maxX = getmaxx(stdscr);
    int leftWidth = maxX / 2 - 4;  // Left column takes ~half width

    int row = 3;
    int col = 2;

    // Title
    attron(A_BOLD);
    mvprintw(row, col, "Main");
    attroff(A_BOLD);
    row += 2;

    // Current preset name
    mvprintw(row++, col, "Current Preset: %s", currentPresetName.empty() ? "None" : currentPresetName.c_str());
    row++;

    // Action buttons section
    const char* actionLabels[] = {
        "Save Preset",
        "Load Preset",
        "Global Randomize",
        "Global Mutate",
        "Mutate Amount",
        "Global Reset",
        "CPU Monitor"
    };

    mvprintw(row, col, "Actions:");
    row += 2;

    // Draw action buttons
    for (int i = 0; i < 7; ++i) {
        bool selected = (i == mainPageActionIndex && mainPageFocusLeft);

        if (selected) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(row, col, "> %s", actionLabels[i]);
            attroff(COLOR_PAIR(5) | A_BOLD);
        } else {
            mvprintw(row, col, "  %s", actionLabels[i]);
        }

        // Show mutate percentage for Mutate Amount item
        if (i == 4) {  // Mutate Amount
            mvprintw(row, col + 20, "%.0f%%", globalMutatePercentage);
        }

        // Show CPU monitor state
        if (i == 6) {  // CPU Monitor
            const char* state = cpuMonitor.isEnabled() ? "ON" : "OFF";
            mvprintw(row, col + 20, "%s", state);
        }

        row++;
    }

    row += 2;
    attron(COLOR_PAIR(8));
    mvprintw(row++, col, "Left/Right: Switch columns");
    mvprintw(row++, col, "Up/Down: Navigate items");
    mvprintw(row++, col, "Enter: Execute | ?: Help");
    attroff(COLOR_PAIR(8));

    // Mixer on right side
    int rightCol = leftWidth + 4;
    int mixerRow = 3;

    // Mixer title
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(mixerRow++, rightCol, "MIXER");
    attroff(COLOR_PAIR(1) | A_BOLD);
    mixerRow++; // Blank line

    // Mixer header
    attron(COLOR_PAIR(3));
    mvprintw(mixerRow++, rightCol, "Source   M S  Level");
    attroff(COLOR_PAIR(3));

    // Draw oscillators
    for (int i = 0; i < 4; ++i) {
        float level = params->getOscLevel(i);
        bool muted = params->oscMuted[i].load();
        bool solo = params->oscSolo[i].load();
        bool selected = (mainPageMixerChannel == i && !mainPageFocusLeft);

        if (selected) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(mixerRow, rightCol, ">OSC %d", i + 1);
        } else {
            mvprintw(mixerRow, rightCol, " OSC %d", i + 1);
        }

        // Draw mute/solo indicators
        if (muted) {
            attron(COLOR_PAIR(4) | A_BOLD);
            mvprintw(mixerRow, rightCol + 9, "M");
            attroff(COLOR_PAIR(4) | A_BOLD);
        } else {
            mvprintw(mixerRow, rightCol + 9, "-");
        }

        if (solo) {
            attron(COLOR_PAIR(2) | A_BOLD);
            mvprintw(mixerRow, rightCol + 11, "S");
            attroff(COLOR_PAIR(2) | A_BOLD);
        } else {
            mvprintw(mixerRow, rightCol + 11, "-");
        }

        // Draw compact bar (20 chars)
        drawBar(mixerRow, rightCol + 14, "", level, 0.0f, 1.0f, 20);

        if (selected) {
            attroff(COLOR_PAIR(5) | A_BOLD);
        }

        mixerRow++;
    }

    mixerRow++; // Blank line

    // Draw samplers
    for (int i = 0; i < 4; ++i) {
        float level = synth->getSamplerLevel(i);
        bool muted = params->samplerMuted[i].load();
        bool solo = params->samplerSolo[i].load();
        bool selected = (mainPageMixerChannel == i + 4 && !mainPageFocusLeft);

        if (selected) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(mixerRow, rightCol, ">SMP %d", i + 1);
        } else {
            mvprintw(mixerRow, rightCol, " SMP %d", i + 1);
        }

        // Draw mute/solo indicators
        if (muted) {
            attron(COLOR_PAIR(4) | A_BOLD);
            mvprintw(mixerRow, rightCol + 9, "M");
            attroff(COLOR_PAIR(4) | A_BOLD);
        } else {
            mvprintw(mixerRow, rightCol + 9, "-");
        }

        if (solo) {
            attron(COLOR_PAIR(2) | A_BOLD);
            mvprintw(mixerRow, rightCol + 11, "S");
            attroff(COLOR_PAIR(2) | A_BOLD);
        } else {
            mvprintw(mixerRow, rightCol + 11, "-");
        }

        // Draw compact bar (20 chars)
        drawBar(mixerRow, rightCol + 14, "", level, 0.0f, 1.0f, 20);

        if (selected) {
            attroff(COLOR_PAIR(5) | A_BOLD);
        }

        mixerRow++;
    }

    mixerRow++; // Blank line

    // Draw chaos generators
    for (int i = 0; i < 4; ++i) {
        float level = 0.5f;  // TODO: Add actual chaos level parameters
        bool muted = params->chaosMuted[i].load();
        bool solo = params->chaosSolo[i].load();
        bool selected = (mainPageMixerChannel == i + 8 && !mainPageFocusLeft);

        if (selected) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(mixerRow, rightCol, ">CHS %d", i + 1);
        } else {
            mvprintw(mixerRow, rightCol, " CHS %d", i + 1);
        }

        // Draw mute/solo indicators
        if (muted) {
            attron(COLOR_PAIR(4) | A_BOLD);
            mvprintw(mixerRow, rightCol + 9, "M");
            attroff(COLOR_PAIR(4) | A_BOLD);
        } else {
            mvprintw(mixerRow, rightCol + 9, "-");
        }

        if (solo) {
            attron(COLOR_PAIR(2) | A_BOLD);
            mvprintw(mixerRow, rightCol + 11, "S");
            attroff(COLOR_PAIR(2) | A_BOLD);
        } else {
            mvprintw(mixerRow, rightCol + 11, "-");
        }

        // Draw compact bar (20 chars)
        drawBar(mixerRow, rightCol + 14, "", level, 0.0f, 1.0f, 20);

        if (selected) {
            attroff(COLOR_PAIR(5) | A_BOLD);
        }

        mixerRow++;
    }

    mixerRow += 2;
    attron(COLOR_PAIR(8));
    mvprintw(mixerRow++, rightCol, "1-4: Select OSC | 5-8: Select SAMP");
    mvprintw(mixerRow++, rightCol, "Shift+1-4: Select CHAOS");
    mvprintw(mixerRow++, rightCol, "M: Mute | S: Solo | [/]: Level");
    attroff(COLOR_PAIR(8));

    // Draw preset browser if active
    if (presetBrowserActive) {
        int maxY = getmaxy(stdscr);
        int maxX = getmaxx(stdscr);
        int boxWidth = std::min(60, maxX - 10);
        int boxHeight = std::min(25, maxY - 10);
        int boxTop = (maxY - boxHeight) / 2;
        int boxLeft = (maxX - boxWidth) / 2;

        // Draw background
        for (int y = boxTop; y < boxTop + boxHeight; ++y) {
            for (int x = boxLeft; x < boxLeft + boxWidth; ++x) {
                mvaddch(y, x, ' ' | COLOR_PAIR(1));
            }
        }

        // Draw border
        attron(COLOR_PAIR(1) | A_BOLD);
        mvhline(boxTop, boxLeft, '-', boxWidth);
        mvhline(boxTop + boxHeight - 1, boxLeft, '-', boxWidth);
        mvvline(boxTop, boxLeft, '|', boxHeight);
        mvvline(boxTop, boxLeft + boxWidth - 1, '|', boxHeight);

        // Corners
        mvaddch(boxTop, boxLeft, '+');
        mvaddch(boxTop, boxLeft + boxWidth - 1, '+');
        mvaddch(boxTop + boxHeight - 1, boxLeft, '+');
        mvaddch(boxTop + boxHeight - 1, boxLeft + boxWidth - 1, '+');
        attroff(COLOR_PAIR(1) | A_BOLD);

        // Title
        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(boxTop + 1, boxLeft + 2, "Load Preset");
        attroff(COLOR_PAIR(1) | A_BOLD);

        // List contents
        int visible = boxHeight - 4;
        if (presetBrowserSelectedIndex < presetBrowserScrollOffset) {
            presetBrowserScrollOffset = presetBrowserSelectedIndex;
        }
        if (presetBrowserSelectedIndex >= presetBrowserScrollOffset + visible) {
            presetBrowserScrollOffset = presetBrowserSelectedIndex - visible + 1;
        }

        for (int i = 0; i < visible; ++i) {
            int idx = presetBrowserScrollOffset + i;
            int y = boxTop + 3 + i;

            if (idx >= 0 && idx < static_cast<int>(presetBrowserPresets.size())) {
                bool selected = (idx == presetBrowserSelectedIndex);
                if (selected) {
                    attron(COLOR_PAIR(5) | A_BOLD);
                    mvprintw(y, boxLeft + 2, "> %s", presetBrowserPresets[idx].c_str());
                    attroff(COLOR_PAIR(5) | A_BOLD);
                } else {
                    mvprintw(y, boxLeft + 2, "  %s", presetBrowserPresets[idx].c_str());
                }
            }
        }

        // Instructions
        attron(COLOR_PAIR(1));
        mvprintw(boxTop + boxHeight - 2, boxLeft + 2, "Enter=Load  Esc/Q=Cancel");
        attroff(COLOR_PAIR(1));
    }
}
