#include "../ui.h"
#include "../sequencer.h"
#include "ui_utils.h"
#include <algorithm>
#include <string>
#include <chrono>

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
        {"LFO", UIPage::LFO},
        {"ENV", UIPage::ENV},
        {"FILTER", UIPage::FILTER},
        {"REVERB", UIPage::REVERB},
        {"CHAOS", UIPage::CHAOS},
        {"MOD", UIPage::MOD},
        {"FM", UIPage::FM},
        {"SEQ", UIPage::SEQUENCER},
        {"CONFIG", UIPage::CONFIG},
        {"DEBUG", UIPage::DEBUG}
    };

    int x = 0;
    const int tabCount = static_cast<int>(sizeof(tabs) / sizeof(tabs[0]));
    for (int i = 0; i < tabCount; ++i) {
        std::string text = " " + std::string(tabs[i].label) + " ";
        int textLen = static_cast<int>(text.size());

        if (currentPage == tabs[i].page) {
            attron(COLOR_SELECTION | A_BOLD);
            mvprintw(0, x, "%s", text.c_str());
            attroff(COLOR_SELECTION | A_BOLD);
        } else {
            attron(COLOR_TAB_INACTIVE);
            mvprintw(0, x, "%s", text.c_str());
            attroff(COLOR_TAB_INACTIVE);
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
    attron(COLOR_BORDER);
    mvhline(1, 0, '-', cols);
    attroff(COLOR_BORDER);
}

void UI::drawBar(int y, int x, const char* label, float value, float min, float max, int width) {
    float normalized = (value - min) / (max - min);
    int fillWidth = static_cast<int>(normalized * width);

    mvprintw(y, x, "%s", label);

    int labelLen = 0;
    while (label[labelLen] != '\0') labelLen++;

    int barStart = x + labelLen + 1;

    mvaddch(y, barStart, '[');
    attron(COLOR_MODULATED);
    for (int i = 0; i < width; ++i) {
        if (i < fillWidth) {
            mvaddch(y, barStart + 1 + i, '=');
        } else {
            mvaddch(y, barStart + 1 + i, ' ');
        }
    }
    attroff(COLOR_MODULATED);
    mvaddch(y, barStart + width + 1, ']');

    mvprintw(y, barStart + width + 3, "%.3f", value);
}

void UI::drawBarInactive(int y, int x, const char* label, float value, float min, float max, int width) {
    float normalized = (value - min) / (max - min);
    int fillWidth = static_cast<int>(normalized * width);

    attron(COLOR_LOCKED);
    mvprintw(y, x, "%s", label);

    int labelLen = 0;
    while (label[labelLen] != '\0') labelLen++;

    int barStart = x + labelLen + 1;

    mvaddch(y, barStart, '[');
    // Use locked color for the entire bar
    for (int i = 0; i < width; ++i) {
        if (i < fillWidth) {
            mvaddch(y, barStart + 1 + i, '=');
        } else {
            mvaddch(y, barStart + 1 + i, ' ');
        }
    }
    mvaddch(y, barStart + width + 1, ']');

    mvprintw(y, barStart + width + 3, "%.3f", value);
    attroff(COLOR_LOCKED);
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
    attron(COLOR_LABEL);
    mvprintw(0, x, "CPU:");
    attroff(COLOR_LABEL);

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

void UI::drawStatusMessage() {
    if (statusMessage.empty()) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (statusMessageExpiry.time_since_epoch().count() == 0 || now > statusMessageExpiry) {
        statusMessage.clear();
        return;
    }

    int maxY = getmaxy(stdscr);
    int maxX = getmaxx(stdscr);
    int row = maxY - 2;
    if (row < 0) {
        return;
    }

    mvhline(row, 0, ' ', maxX);

    std::string text = statusMessage;
    if (static_cast<int>(text.size()) > maxX - 2) {
        if (maxX > 4) {
            text = text.substr(0, maxX - 5) + "...";
        } else if (maxX > 0) {
            text = text.substr(0, maxX);
        } else {
            text.clear();
        }
    }

    if (!text.empty()) {
        attron(COLOR_HINT | A_BOLD);
        mvprintw(row, 1, "%s", text.c_str());
        attroff(COLOR_HINT | A_BOLD);
    }
}

void UI::drawHotkeyLine() {
    int maxY = getmaxy(stdscr);
    int maxX = getmaxx(stdscr);
    int row = maxY - 1;

    attron(COLOR_BORDER);
    mvhline(row - 1, 0, '-', maxX);
    attroff(COLOR_BORDER);

    if (numericInputActive) {
        mvprintw(row, 1, "Type value  |  Enter Confirm  |  Esc Cancel  |  Q Quit");
    } else if (sequencerScaleMenuActive) {
        mvprintw(row, 1, "Up/Down Choose Scale  |  Enter Confirm  |  Esc Cancel  |  Q Quit");
    } else if (textInputActive) {
        mvprintw(row, 1, "Type preset name  |  Enter Save  |  Esc Cancel  |  Q Quit");
    } else if (params->midiLearnActive.load()) {
        mvprintw(row, 1, "Move MIDI controller to assign  |  Esc Cancel  |  Q Quit");
    } else {
        if (currentPage == UIPage::DEBUG) {
            mvprintw(row, 1, "Tab Page  |  Ctrl+D Debug  |  R Reset Stats  |  Q Quit");
        } else if (currentPage == UIPage::FM) {
            mvprintw(row, 1, "Tab Page  |  Arrows Nav  |  -/= Adjust  |  _/+ Fine  |  g/G Rand  |  m/M Mut  |  Enter Type  |  l Lock  |  R Reset  |  Q Quit");
        } else if (currentPage == UIPage::MOD) {
            mvprintw(row, 1, "Tab Page  |  Arrows Nav  |  Enter Edit  |  g/G Rand  |  m/M Mut  |  l Lock Slot  |  Q Quit");
        } else if (currentPage == UIPage::SEQUENCER) {
            mvprintw(row, 1, "Tab Page  |  Arrows Nav  |  -/= Adjust  |  _/+ Fine  |  Enter Type  |  Backspace/Delete Clear Step  |  Q Quit");
        } else {
            mvprintw(row, 1, "Tab Page  |  Arrows Nav  |  -/= Adjust  |  _/+ Fine  |  g/G Rand  |  m/M Mut  |  Enter Type  |  l Lock  |  L Learn  |  R Reset  |  Q Quit");
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

        attron(COLOR_LOCKED | A_BOLD | A_REVERSE);  // Red/yellow bold reverse (attention-grabbing)
        mvprintw(0, x, "%s", indicator.c_str());
        attroff(COLOR_LOCKED | A_BOLD | A_REVERSE);
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
        case UIPage::COMPRESSOR:
            drawCompressorPage();
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
        case UIPage::DEBUG:
            drawDebugPage();
            break;
    }

    drawStatusMessage();
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
        attron(COLOR_SELECTION);
        for (int y = startY; y < startY + height; ++y) {
            for (int x = startX; x < startX + width; ++x) {
                mvaddch(y, x, ' ');
            }
        }
        attroff(COLOR_SELECTION);

        // Draw border
        attron(A_BOLD);
        mvhline(startY, startX, '-', width);
        mvhline(startY + height - 1, startX, '-', width);
        mvvline(startY, startX, '|', height);
        mvvline(startY, startX + width - 1, '|', height);
        attroff(A_BOLD);

        // Draw title
        attron(COLOR_SELECTION | A_BOLD);
        mvprintw(startY + 1, startX + 2, "Save Preset As:");
        attroff(COLOR_SELECTION | A_BOLD);

        // Draw input field
        mvprintw(startY + 3, startX + 2, "> %s_", textInputBuffer.c_str());

        // Draw hint
        attron(COLOR_HINT);
        mvprintw(startY + 5, startX + 2, "Press Enter to save, Esc to cancel");
        attroff(COLOR_HINT);
    } else if (numericInputActive) {
        int maxY = getmaxy(stdscr);
        int maxX = getmaxx(stdscr);
        int height = 7;
        int width = 40;
        int startY = (maxY - height) / 2;
        int startX = (maxX - width) / 2;

        // Draw box
        attron(COLOR_SELECTION);
        for (int y = startY; y < startY + height; ++y) {
            for (int x = startX; x < startX + width; ++x) {
                mvaddch(y, x, ' ');
            }
        }
        attroff(COLOR_SELECTION);

        // Draw border
        attron(A_BOLD);
        mvhline(startY, startX, '-', width);
        mvhline(startY + height - 1, startX, '-', width);
        mvvline(startY, startX, '|', height);
        mvvline(startY, startX + width - 1, '|', height);
        attroff(A_BOLD);

        const char* label = nullptr;
        if (numericInputIsMod) {
            label = "Modulation Amt (-99 to 99)";
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
            attron(COLOR_SELECTION | A_BOLD);
            mvprintw(startY + 1, startX + 2, "Enter %s:", label);
            attroff(COLOR_SELECTION | A_BOLD);
        }

        // Draw input field
        mvprintw(startY + 3, startX + 2, "> %s_", numericInputBuffer.c_str());

        // Draw hint
        attron(COLOR_HINT);
        if (numericInputIsSequencer) {
            if (sequencerNumericContext.field == SequencerNumericField::NOTE) {
                mvprintw(startY + 5, startX + 2, "Type note or play MIDI key, Esc cancel");
            } else if (sequencerNumericContext.field == SequencerNumericField::ROOT) {
                mvprintw(startY + 5, startX + 2, "Type root or play MIDI key, Esc cancel");
            } else {
                mvprintw(startY + 5, startX + 2, "Enter value, Esc cancel");
            }
        } else {
            mvprintw(startY + 5, startX + 2, "Press Enter to confirm, Esc to cancel");
        }
        attroff(COLOR_HINT);
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
                attron(COLOR_SELECTION | A_BOLD);
                mvprintw(y, startX + 2, "> %-20s", name);
                attroff(COLOR_SELECTION | A_BOLD);
            } else {
                mvprintw(y, startX + 2, "  %-20s", name);
            }
        }

        attron(COLOR_HINT);
        mvprintw(startY + height - 2, startX + 2, "Enter confirm, Esc cancel");
        attroff(COLOR_HINT);
    } else if (globalResetPopupActive) {
        int maxY = getmaxy(stdscr);
        int maxX = getmaxx(stdscr);
        int height = 10;
        int width = 44;
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
        mvprintw(startY + 1, startX + 2, "Global Reset");
        attroff(A_BOLD);

        const char* options[2] = {"Reset to Saved Preset", "Reset to Init Defaults"};
        for (int i = 0; i < 2; ++i) {
            int y = startY + 3 + i;
            bool selected = (i == globalResetPopupIndex);
            if (selected) {
                attron(COLOR_SELECTION | A_BOLD);
                mvprintw(y, startX + 2, "> %s", options[i]);
                attroff(COLOR_SELECTION | A_BOLD);
            } else {
                mvprintw(y, startX + 2, "  %s", options[i]);
            }
        }

        attron(COLOR_HINT);
        mvprintw(startY + height - 2, startX + 2, "Up/Down choose, Enter confirm, Esc cancel");
        attroff(COLOR_HINT);
    } else if (quitPopupActive) {
        int maxY = getmaxy(stdscr);
        int maxX = getmaxx(stdscr);
        int height = 8;
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
        mvprintw(startY + 1, startX + 2, "Quit Wakefield?");
        attroff(A_BOLD);

        mvprintw(startY + 3, startX + 2, "Are you sure you want to quit?");

        attron(COLOR_HINT);
        mvprintw(startY + height - 2, startX + 2, "Y to quit, N or Esc to cancel");
        attroff(COLOR_HINT);
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
        attron(COLOR_SELECTION | A_BOLD);
        mvprintw(startY + 1, startX + 2, "Load Sample - %s", sampleBrowserCurrentDir.c_str());
        attroff(COLOR_SELECTION | A_BOLD);

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
                attron(COLOR_SELECTION | A_BOLD);
                mvprintw(displayRow, startX + 2, "> %-54s", displayName.c_str());
                attroff(COLOR_SELECTION | A_BOLD);
            } else {
                mvprintw(displayRow, startX + 2, "  %-54s", displayName.c_str());
            }

            displayRow++;
        }

        // Draw hint
        attron(COLOR_HINT);
        mvprintw(startY + height - 2, startX + 2, "Enter: Select | Arrows: Navigate | Esc: Cancel");
        attroff(COLOR_HINT);
    }

    // Draw MIDI Learn popup if active (drawn last so it overlays everything)
    if (params->midiLearnActive.load()) {
        int maxY = getmaxy(stdscr);
        int maxX = getmaxx(stdscr);

        // Popup dimensions
        int popupWidth = 50;
        int popupHeight = 7;
        int startX = (maxX - popupWidth) / 2;
        int startY = (maxY - popupHeight) / 2;

        // Draw box border
        attron(COLOR_POPUP_BORDER | A_BOLD);
        for (int y = startY; y < startY + popupHeight; ++y) {
            mvhline(y, startX, ' ', popupWidth);
        }

        // Draw border
        mvaddch(startY, startX, ACS_ULCORNER);
        mvhline(startY, startX + 1, ACS_HLINE, popupWidth - 2);
        mvaddch(startY, startX + popupWidth - 1, ACS_URCORNER);

        for (int y = startY + 1; y < startY + popupHeight - 1; ++y) {
            mvaddch(y, startX, ACS_VLINE);
            mvaddch(y, startX + popupWidth - 1, ACS_VLINE);
        }

        mvaddch(startY + popupHeight - 1, startX, ACS_LLCORNER);
        mvhline(startY + popupHeight - 1, startX + 1, ACS_HLINE, popupWidth - 2);
        mvaddch(startY + popupHeight - 1, startX + popupWidth - 1, ACS_LRCORNER);

        // Draw content
        std::string title = "MIDI LEARN ACTIVE";
        int titleX = startX + (popupWidth - static_cast<int>(title.length())) / 2;
        mvprintw(startY + 2, titleX, "%s", title.c_str());

        int learnParamId = params->midiLearnParameterId.load();
        InlineParameter* learnParam = getParameter(learnParamId);
        if (learnParam) {
            std::string paramName = learnParam->name;
            std::string msg = "Move CC for: " + paramName;
            int msgX = startX + (popupWidth - static_cast<int>(msg.length())) / 2;
            mvprintw(startY + 4, msgX, "%s", msg.c_str());
        }

        attroff(COLOR_POPUP_BORDER | A_BOLD);
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
        "Random Amount",
        "Mutate Amount",
        "Global Reset",
        "Compressor",
        "CPU Monitor",
        // Instance controls
        "OSC Instances",
        "SAMP Instances",
        "LFO Instances",
        "ENV Instances",
        "CHAOS Instances"
    };

    mvprintw(row, col, "Actions:");
    row += 2;

    // Draw action buttons
    const int actionCount = 14;
    for (int i = 0; i < actionCount; ++i) {
        bool selected = (i == mainPageActionIndex && mainPageFocusLeft);

        if (selected) {
            attron(COLOR_SELECTION | A_BOLD);
            mvprintw(row, col, "> %s", actionLabels[i]);
            attroff(COLOR_SELECTION | A_BOLD);
        } else {
            mvprintw(row, col, "  %s", actionLabels[i]);
        }

        // Show percentages for Amount items
        if (i == 4) {  // Random Amount
            mvprintw(row, col + 20, "%.0f%%", globalRandomizePercentage);
        }
        if (i == 5) {  // Mutate Amount
            mvprintw(row, col + 20, "%.0f%%", globalMutatePercentage);
        }

        // Show compressor state
        if (i == 7) {  // Compressor
            const char* state = params->compressorEnabled.load() ? "ON" : "OFF";
            mvprintw(row, col + 20, "%s", state);
        }

        // Show CPU monitor state
        if (i == 8) {  // CPU Monitor
            const char* state = cpuMonitor.isEnabled() ? "ON" : "OFF";
            mvprintw(row, col + 20, "%s", state);
        }

        // Show instance counts for generator items
        if (i == 9) {
            mvprintw(row, col + 20, "%d", std::clamp(params->activeOscCount.load(), 1, 4));
        }
        if (i == 10) {
            mvprintw(row, col + 20, "%d", std::clamp(params->activeSamplerCount.load(), 1, 4));
        }
        if (i == 11) {
            mvprintw(row, col + 20, "%d", std::clamp(params->activeLfoCount.load(), 1, 4));
        }
        if (i == 12) {
            mvprintw(row, col + 20, "%d", std::clamp(params->activeEnvCount.load(), 1, 4));
        }
        if (i == 13) {
            mvprintw(row, col + 20, "%d", std::clamp(params->activeChaosCount.load(), 1, 4));
        }

        row++;
    }

    // Mixer on right side
    int rightCol = leftWidth + 4;
    int mixerRow = 3;

    // Mixer title
    attron(COLOR_SECTION_HEADER | A_BOLD);
    mvprintw(mixerRow++, rightCol, "MIXER");
    attroff(COLOR_SECTION_HEADER | A_BOLD);
    mixerRow++; // Blank line

    // Mixer header
    attron(COLOR_SECTION_HEADER);
    mvprintw(mixerRow++, rightCol, "Source   M S  Level");
    attroff(COLOR_SECTION_HEADER);

    // Draw oscillators
    int activeOsc = std::clamp(params->activeOscCount.load(), 1, 4);
    for (int i = 0; i < 4; ++i) {
        float level = params->getOscLevel(i);
        bool muted = params->oscMuted[i].load();
        bool solo = params->oscSolo[i].load();
        bool selected = (mainPageMixerChannel == i && !mainPageFocusLeft);
        bool inactive = (i >= activeOsc);

        if (inactive) attron(COLOR_LOCKED | A_DIM);

        if (selected) {
            attron(COLOR_SELECTION | A_BOLD);
            mvprintw(mixerRow, rightCol, ">OSC %d", i + 1);
        } else {
            mvprintw(mixerRow, rightCol, " OSC %d", i + 1);
        }

        // Draw mute/solo indicators
        if (muted) {
            if (!inactive) attron(COLOR_STATUS_MUTED | A_BOLD); else attron(COLOR_LOCKED);
            mvprintw(mixerRow, rightCol + 9, "M");
            if (!inactive) attroff(COLOR_STATUS_MUTED | A_BOLD); else attroff(COLOR_LOCKED);
        } else {
            mvprintw(mixerRow, rightCol + 9, "-");
        }

        if (solo) {
            if (!inactive) attron(COLOR_STATUS_SOLO | A_BOLD); else attron(COLOR_LOCKED);
            mvprintw(mixerRow, rightCol + 11, "S");
            if (!inactive) attroff(COLOR_STATUS_SOLO | A_BOLD); else attroff(COLOR_LOCKED);
        } else {
            mvprintw(mixerRow, rightCol + 11, "-");
        }

        // Draw compact bar (20 chars)
        if (inactive) drawBarInactive(mixerRow, rightCol + 14, "", level, 0.0f, 1.0f, 20);
        else drawBar(mixerRow, rightCol + 14, "", level, 0.0f, 1.0f, 20);

        if (selected) {
            attroff(COLOR_SELECTION | A_BOLD);
        }

        if (inactive) attroff(COLOR_LOCKED | A_DIM);

        mixerRow++;
    }

    mixerRow++; // Blank line

    // Draw samplers
    int activeSamp = std::clamp(params->activeSamplerCount.load(), 1, 4);
    for (int i = 0; i < 4; ++i) {
        float level = synth->getSamplerLevel(i);
        bool muted = params->samplerMuted[i].load();
        bool solo = params->samplerSolo[i].load();
        bool selected = (mainPageMixerChannel == i + 4 && !mainPageFocusLeft);
        bool inactive = (i >= activeSamp);

        if (inactive) attron(COLOR_LOCKED | A_DIM);

        if (selected) {
            attron(COLOR_SELECTION | A_BOLD);
            mvprintw(mixerRow, rightCol, ">SMP %d", i + 1);
        } else {
            mvprintw(mixerRow, rightCol, " SMP %d", i + 1);
        }

        // Draw mute/solo indicators
        if (muted) {
            if (!inactive) attron(COLOR_STATUS_MUTED | A_BOLD); else attron(COLOR_LOCKED);
            mvprintw(mixerRow, rightCol + 9, "M");
            if (!inactive) attroff(COLOR_STATUS_MUTED | A_BOLD); else attroff(COLOR_LOCKED);
        } else {
            mvprintw(mixerRow, rightCol + 9, "-");
        }

        if (solo) {
            if (!inactive) attron(COLOR_STATUS_SOLO | A_BOLD); else attron(COLOR_LOCKED);
            mvprintw(mixerRow, rightCol + 11, "S");
            if (!inactive) attroff(COLOR_STATUS_SOLO | A_BOLD); else attroff(COLOR_LOCKED);
        } else {
            mvprintw(mixerRow, rightCol + 11, "-");
        }

        // Draw compact bar (20 chars)
        if (inactive) drawBarInactive(mixerRow, rightCol + 14, "", level, 0.0f, 1.0f, 20);
        else drawBar(mixerRow, rightCol + 14, "", level, 0.0f, 1.0f, 20);

        if (selected) {
            attroff(COLOR_SELECTION | A_BOLD);
        }

        if (inactive) attroff(COLOR_LOCKED | A_DIM);

        mixerRow++;
    }

    mixerRow++; // Blank line

    // Draw chaos generators
    int activeChaos = std::clamp(params->activeChaosCount.load(), 1, 4);
    for (int i = 0; i < 4; ++i) {
        float level = params->getChaosLevel(i);
        bool muted = params->chaosMuted[i].load();
        bool solo = params->chaosSolo[i].load();
        bool selected = (mainPageMixerChannel == i + 8 && !mainPageFocusLeft);
        bool inactive = (i >= activeChaos);

        if (inactive) attron(COLOR_LOCKED | A_DIM);

        if (selected) {
            attron(COLOR_SELECTION | A_BOLD);
            mvprintw(mixerRow, rightCol, ">CHS %d", i + 1);
        } else {
            mvprintw(mixerRow, rightCol, " CHS %d", i + 1);
        }

        // Draw mute/solo indicators
        if (muted) {
            if (!inactive) attron(COLOR_STATUS_MUTED | A_BOLD); else attron(COLOR_LOCKED);
            mvprintw(mixerRow, rightCol + 9, "M");
            if (!inactive) attroff(COLOR_STATUS_MUTED | A_BOLD); else attroff(COLOR_LOCKED);
        } else {
            mvprintw(mixerRow, rightCol + 9, "-");
        }

        if (solo) {
            if (!inactive) attron(COLOR_STATUS_SOLO | A_BOLD); else attron(COLOR_LOCKED);
            mvprintw(mixerRow, rightCol + 11, "S");
            if (!inactive) attroff(COLOR_STATUS_SOLO | A_BOLD); else attroff(COLOR_LOCKED);
        } else {
            mvprintw(mixerRow, rightCol + 11, "-");
        }

        // Draw compact bar (20 chars)
        if (inactive) drawBarInactive(mixerRow, rightCol + 14, "", level, 0.0f, 1.0f, 20);
        else drawBar(mixerRow, rightCol + 14, "", level, 0.0f, 1.0f, 20);

        if (selected) {
            attroff(COLOR_SELECTION | A_BOLD);
        }

        if (inactive) attroff(COLOR_LOCKED | A_DIM);

        mixerRow++;
    }

    // Compressor section
    mixerRow += 2;
    attron(COLOR_SECTION_HEADER | A_BOLD);
    mvprintw(mixerRow++, rightCol, "COMPRESSOR");
    attroff(COLOR_SECTION_HEADER | A_BOLD);
    mixerRow++;

    bool compEnabled = params->compressorEnabled.load();
    float compThreshold = params->compressorThreshold.load();
    float compRatio = params->compressorRatio.load();
    float compMix = params->compressorMix.load();

    // Enabled toggle
    if (compEnabled) {
        attron(COLOR_STATUS_ACTIVE | A_BOLD);
        mvprintw(mixerRow, rightCol, "[ON]");
        attroff(COLOR_STATUS_ACTIVE | A_BOLD);
    } else {
        attron(COLOR_LOCKED);
        mvprintw(mixerRow, rightCol, "[OFF]");
        attroff(COLOR_LOCKED);
    }

    // Gain reduction meter (only show if enabled)
    if (compEnabled && synth) {
        float gr = synth->getCompressorGainReduction();
        if (gr < 0.0f) {
            int barLen = static_cast<int>(std::abs(gr) / 24.0f * 15);
            barLen = std::clamp(barLen, 0, 15);
            mvprintw(mixerRow, rightCol + 6, "GR:");
            attron(COLOR_VALUE | A_BOLD);
            for (int i = 0; i < barLen; ++i) {
                mvprintw(mixerRow, rightCol + 9 + i, "=");
            }
            attroff(COLOR_VALUE | A_BOLD);
            mvprintw(mixerRow, rightCol + 25, "%.1fdB", gr);
        } else {
            mvprintw(mixerRow, rightCol + 6, "GR: 0.0dB");
        }
    }
    mixerRow++;

    // Threshold
    mvprintw(mixerRow++, rightCol, "Thresh: %.1f dB", compThreshold);

    // Ratio
    const char* ratioLabel = compRatio >= 20.0f ? " (LIMIT)" : "";
    mvprintw(mixerRow++, rightCol, "Ratio:  %.1f:1%s", compRatio, ratioLabel);

    // Mix
    mvprintw(mixerRow++, rightCol, "Mix:    %.0f%%", compMix * 100.0f);

    mixerRow += 2;
    attron(COLOR_PAIR(8));
    mvprintw(mixerRow++, rightCol, "1-4: Select OSC | 5-8: Select SAMP");
    mvprintw(mixerRow++, rightCol, "Shift+1-4: Select CHAOS");
    mvprintw(mixerRow++, rightCol, "M: Mute | S: Solo | -/= lvl | _/+ fine");
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
                mvaddch(y, x, ' ' | COLOR_POPUP_BORDER);
            }
        }

        // Draw border
        attron(COLOR_POPUP_BORDER | A_BOLD);
        mvhline(boxTop, boxLeft, '-', boxWidth);
        mvhline(boxTop + boxHeight - 1, boxLeft, '-', boxWidth);
        mvvline(boxTop, boxLeft, '|', boxHeight);
        mvvline(boxTop, boxLeft + boxWidth - 1, '|', boxHeight);

        // Corners
        mvaddch(boxTop, boxLeft, '+');
        mvaddch(boxTop, boxLeft + boxWidth - 1, '+');
        mvaddch(boxTop + boxHeight - 1, boxLeft, '+');
        mvaddch(boxTop + boxHeight - 1, boxLeft + boxWidth - 1, '+');
        attroff(COLOR_POPUP_BORDER | A_BOLD);

        // Title
        attron(COLOR_SECTION_HEADER | A_BOLD);
        mvprintw(boxTop + 1, boxLeft + 2, "Load Preset");
        attroff(COLOR_SECTION_HEADER | A_BOLD);

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
                    attron(COLOR_SELECTION | A_BOLD);
                    mvprintw(y, boxLeft + 2, "> %s", presetBrowserPresets[idx].c_str());
                    attroff(COLOR_SELECTION | A_BOLD);
                } else {
                    mvprintw(y, boxLeft + 2, "  %s", presetBrowserPresets[idx].c_str());
                }
            }
        }

        // Instructions
        attron(COLOR_HINT);
        mvprintw(boxTop + boxHeight - 2, boxLeft + 2, "Enter=Load  Esc/Q=Cancel");
        attroff(COLOR_HINT);
    }
}
