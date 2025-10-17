#include "../../ui.h"

void UI::drawConfigPage() {
    int row = 3;

    // System information
    attron(A_BOLD);
    mvprintw(row++, 1, "DEVICE CONFIGURATION");
    attroff(A_BOLD);
    row++;

    attron(COLOR_PAIR(3));
    mvprintw(row++, 2, "Audio and MIDI device configuration");
    attroff(COLOR_PAIR(3));
    row++;

    // Audio device info
    attron(A_BOLD);
    mvprintw(row++, 1, "AUDIO DEVICE");
    attroff(A_BOLD);
    mvprintw(row++, 2, "Device: %s", audioDeviceName.c_str());
    mvprintw(row++, 2, "Sample Rate: %d Hz", audioSampleRate);
    mvprintw(row++, 2, "Buffer Size: %d samples", audioBufferSize);

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
        attron(COLOR_PAIR(2));
        mvprintw(row++, 2, "Status: Connected");
        attroff(COLOR_PAIR(2));
    } else {
        attron(COLOR_PAIR(4));
        mvprintw(row++, 2, "Status: Not connected");
        attroff(COLOR_PAIR(4));
    }

    row += 2;

    // Build info
    attron(A_BOLD);
    mvprintw(row++, 1, "BUILD INFO");
    attroff(A_BOLD);
    mvprintw(row++, 2, "Max Voices: 8 (polyphonic)");
    mvprintw(row++, 2, "Waveforms: Sine, Square, Sawtooth, Triangle");
    mvprintw(row++, 2, "Filters: Lowpass, Highpass, Shelving");
    mvprintw(row++, 2, "Reverb: Greyhole (Faust 2.37.3)");
}
