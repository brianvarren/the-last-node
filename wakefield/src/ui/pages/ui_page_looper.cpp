#include "../../ui.h"
#include "../../looper.h"
#include "../../loop_manager.h"
#include <string>

// External reference to global object from main.cpp
extern LoopManager* loopManager;

void UI::drawLooperPage() {
    if (!loopManager) {
        mvprintw(3, 2, "Looper not initialized");
        return;
    }

    int row = 3;
    int currentLoop = params->currentLoop.load();

    // Title
    attron(A_BOLD);
    mvprintw(row++, 1, "LOOPER - Guitar Pedal Style");
    attroff(A_BOLD);
    row++;

    // Current loop indicator
    mvprintw(row++, 2, "Current Loop: ");
    attron(COLOR_PAIR(5) | A_BOLD);
    printw("%d", currentLoop + 1);
    attroff(COLOR_PAIR(5) | A_BOLD);
    printw("  (Press 1-4 to select)");
    row++;

    // Loop state grid
    attron(A_BOLD);
    mvprintw(row++, 1, "LOOP STATUS");
    attroff(A_BOLD);
    row++;

    const char* stateNames[] = {"Empty", "Recording", "Playing", "Overdubbing", "Stopped"};
    int stateColors[] = {1, 3, 2, 4, 6};  // gray, yellow, green, red, cyan

    for (int i = 0; i < 4; ++i) {
        Looper::State state = loopManager->getLoopState(i);
        float loopLen = loopManager->getLoopLength(i);
        float loopTime = loopManager->getLoopTime(i);

        // Loop number
        if (i == currentLoop) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(row, 2, ">");
            attroff(COLOR_PAIR(5) | A_BOLD);
        } else {
            mvprintw(row, 2, " ");
        }

        mvprintw(row, 3, "Loop %d: ", i + 1);

        // State with color
        attron(COLOR_PAIR(stateColors[static_cast<int>(state)]) | A_BOLD);
        printw("%-12s", stateNames[static_cast<int>(state)]);
        attroff(COLOR_PAIR(stateColors[static_cast<int>(state)]) | A_BOLD);

        // Time display
        if (loopLen > 0.0f) {
            int lenMin = static_cast<int>(loopLen) / 60;
            int lenSec = static_cast<int>(loopLen) % 60;
            int lenMs = static_cast<int>((loopLen - static_cast<int>(loopLen)) * 100);

            int timeMin = static_cast<int>(loopTime) / 60;
            int timeSec = static_cast<int>(loopTime) % 60;
            int timeMs = static_cast<int>((loopTime - static_cast<int>(loopTime)) * 100);

            printw("  %02d:%02d.%02d / %02d:%02d.%02d",
                   timeMin, timeSec, timeMs,
                   lenMin, lenSec, lenMs);
        } else {
            printw("  --:--:-- / --:--:--");
        }

        row++;
    }

    row += 2;

    // Global parameters
    attron(A_BOLD);
    mvprintw(row++, 1, "PARAMETERS");
    attroff(A_BOLD);
    row++;

    drawBar(row++, 2, "Overdub Mix ([/])", params->overdubMix.load(), 0.0f, 1.0f, 20);

    row += 2;

    // Controls section
    attron(A_BOLD);
    mvprintw(row++, 1, "CONTROLS");
    attroff(A_BOLD);
    row++;

    mvprintw(row++, 2, "Space - Rec/Play toggle");
    mvprintw(row++, 2, "O     - Overdub toggle");
    mvprintw(row++, 2, "S     - Stop current loop");
    mvprintw(row++, 2, "C     - Clear current loop");
    mvprintw(row++, 2, "1-4   - Select loop");
    mvprintw(row++, 2, "[/]   - Adjust overdub mix");

    row += 2;

    // MIDI Learn section
    attron(A_BOLD);
    mvprintw(row++, 1, "MIDI CC LEARN");
    attroff(A_BOLD);
    row++;

    if (params->loopMidiLearnMode.load()) {
        const char* targetNames[] = {"Rec/Play", "Overdub", "Stop", "Clear"};
        int target = params->loopMidiLearnTarget.load();

        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(row++, 2, ">>> LEARNING MODE ACTIVE <<<");
        attroff(COLOR_PAIR(3) | A_BOLD);

        if (target >= 0 && target < 4) {
            mvprintw(row++, 2, "Move a MIDI controller to assign it to: %s", targetNames[target]);
        }
    } else {
        mvprintw(row++, 2, "Press M to cycle through MIDI learn targets");
    }

    // Show current CC mappings
    int recPlayCC = params->loopRecPlayCC.load();
    int overdubCC = params->loopOverdubCC.load();
    int stopCC = params->loopStopCC.load();
    int clearCC = params->loopClearCC.load();

    mvprintw(row++, 2, "Rec/Play CC: %s", recPlayCC >= 0 ? (std::string("CC#") + std::to_string(recPlayCC)).c_str() : "Not assigned");
    mvprintw(row++, 2, "Overdub CC:  %s", overdubCC >= 0 ? (std::string("CC#") + std::to_string(overdubCC)).c_str() : "Not assigned");
    mvprintw(row++, 2, "Stop CC:     %s", stopCC >= 0 ? (std::string("CC#") + std::to_string(stopCC)).c_str() : "Not assigned");
    mvprintw(row++, 2, "Clear CC:    %s", clearCC >= 0 ? (std::string("CC#") + std::to_string(clearCC)).c_str() : "Not assigned");

    row += 2;

    // Usage guide
    attron(A_BOLD);
    mvprintw(row++, 1, "USAGE GUIDE");
    attroff(A_BOLD);
    row++;

    mvprintw(row++, 2, "1. Press Space to start recording on selected loop");
    mvprintw(row++, 2, "2. Press Space again to start playback");
    mvprintw(row++, 2, "3. Press O to overdub additional layers");
    mvprintw(row++, 2, "4. Use 1-4 to switch between loops and layer sounds");
    mvprintw(row++, 2, "5. Adjust [/] to control overdub mix (prevents clipping)");
}
