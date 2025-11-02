#include "../../ui.h"

void UI::drawConfigPage() {
    int row = 3;

    // System information
    attron(A_BOLD);
    mvprintw(row++, 1, "DEVICE CONFIGURATION");
    attroff(A_BOLD);
    row++;

    attron(COLOR_PAGE_TITLE);
    mvprintw(row++, 2, "Audio and MIDI device configuration");
    attroff(COLOR_PAGE_TITLE);
    row++;

    // Audio device info
    attron(A_BOLD);
    mvprintw(row++, 1, "AUDIO DEVICE");
    attroff(A_BOLD);
    mvprintw(row++, 2, "Device: %s", audioDeviceName.c_str());
    mvprintw(row++, 2, "Sample Rate: %d Hz", audioSampleRate);
    mvprintw(row++, 2, "Buffer Size: %d samples", audioBufferSize);
    mvprintw(row++, 2, "Press [ / ] to change buffer size");

    if (bufferSizeChangeRequested && requestedBufferSize > 0) {
        attron(COLOR_STATUS_ACTIVE);
        mvprintw(row++, 4, "Pending: %d samples (applying)", requestedBufferSize);
        attroff(COLOR_STATUS_ACTIVE);
    }

    if (audioSampleRate > 0 && audioBufferSize > 0) {
        float latency = (audioBufferSize * 1000.0f) / audioSampleRate;
        mvprintw(row++, 2, "Latency: %.2f ms", latency);
    }

    row += 2;

    // MIDI device info
    attron(A_BOLD);
    mvprintw(row++, 1, "MIDI DEVICE");
    attroff(A_BOLD);
    mvprintw(row++, 2, "Device: %s", midiDeviceName.c_str());

    if (midiPortNum >= 0) {
        mvprintw(row++, 2, "Port: %d", midiPortNum);
        attron(COLOR_MODULATED);
        mvprintw(row++, 2, "Status: Connected");
        attroff(COLOR_MODULATED);
    } else {
        attron(COLOR_LOCKED);
        mvprintw(row++, 2, "Status: Not connected");
        attroff(COLOR_LOCKED);
    }

    row += 2;

    // Display info
    attron(A_BOLD);
    mvprintw(row++, 1, "DISPLAY");
    attroff(A_BOLD);
    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);
    mvprintw(row++, 2, "Terminal Size: %d cols x %d rows", maxX, maxY);
    mvprintw(row++, 2, "Character Grid: %d x %d", maxX, maxY);

    row += 2;

    // Build info
    attron(A_BOLD);
    mvprintw(row++, 1, "BUILD INFO");
    attroff(A_BOLD);
    mvprintw(row++, 2, "Max Voices: 8 (polyphonic)");
    mvprintw(row++, 2, "Waveforms: Sine, Square, Sawtooth, Triangle");
    mvprintw(row++, 2, "Filters: Lowpass, Highpass, Shelving");
    mvprintw(row++, 2, "Reverb: Greyhole (Faust 2.37.3)");

    row += 2;

    // UI Settings
    attron(A_BOLD);
    mvprintw(row++, 1, "UI SETTINGS");
    attroff(A_BOLD);
    row++;

    // Draw CPU monitor toggle using standard parameter display
    drawParametersPage(row);
}
