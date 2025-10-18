#include "../ui.h"
#include "../sequencer.h"
#include "ui_utils.h"
#include <algorithm>
#include <deque>
#include <mutex>
#include <string>

void UI::drawTabs() {
    int cols = getmaxx(stdscr);

    struct TabInfo {
        const char* label;
        UIPage page;
    };

    const TabInfo tabs[] = {
        {"MAIN", UIPage::MAIN},
        {"OSCILLATOR", UIPage::OSCILLATOR},
        {"LFO", UIPage::LFO},
        {"ENV", UIPage::ENV},
        {"FM", UIPage::FM},
        {"REVERB", UIPage::REVERB},
        {"FILTER", UIPage::FILTER},
        {"LOOPER", UIPage::LOOPER},
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

void UI::addConsoleMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(consoleMutex);
    consoleMessages.push_back(message);
    if (consoleMessages.size() > 5) {  // Keep only last 5 messages
        consoleMessages.pop_front();
    }
}

void UI::drawConsole() {
    int maxY = getmaxy(stdscr);
    int maxX = getmaxx(stdscr);

    // Console starts 7 lines from bottom
    int consoleStart = maxY - 7;

    // Draw separator
    attron(COLOR_PAIR(1));
    mvhline(consoleStart, 0, '-', maxX);
    attroff(COLOR_PAIR(1));

    // Draw console title
    attron(A_BOLD);
    mvprintw(consoleStart + 1, 1, "CONSOLE");
    attroff(A_BOLD);

    // Draw messages
    std::lock_guard<std::mutex> lock(consoleMutex);
    int row = consoleStart + 2;
    for (const auto& msg : consoleMessages) {
        if (row >= maxY - 2) break;  // Don't overwrite hotkey line
        attron(COLOR_PAIR(2));
        mvprintw(row++, 2, "%s", msg.c_str());
        attroff(COLOR_PAIR(2));
    }
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
        mvprintw(row, 1, "Tab/Shift+Tab Page  |  Up/Dn Param  |  Left/Right Adjust  |  Enter Type  |  L Learn  |  Q Quit");
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

    switch (currentPage) {
        case UIPage::MAIN:
            drawMainPage(activeVoices);
            drawConsole();  // Console only on main page
            break;
        case UIPage::OSCILLATOR:
            drawOscillatorPage();
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
        case UIPage::REVERB:
            drawReverbPage();
            break;
        case UIPage::FILTER:
            drawFilterPage();
            break;
        case UIPage::LOOPER:
            drawLooperPage();
            break;
        case UIPage::SEQUENCER:
            drawSequencerPage();
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
        if (numericInputIsSequencer) {
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
    }

    refresh();
}
