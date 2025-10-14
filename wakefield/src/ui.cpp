#include "ui.h"
#include "synth.h"
#include "preset.h"
#include "loop_manager.h"
#include "looper.h"
#include <algorithm>
#include <cmath>

// External reference to global loopManager from main.cpp
extern LoopManager* loopManager;

UI::UI(Synth* synth, SynthParameters* params)
    : synth(synth)
    , params(params)
    , initialized(false)
    , currentPage(UIPage::MAIN)
    , audioDeviceName("Unknown")
    , audioSampleRate(0)
    , audioBufferSize(0)
    , midiDeviceName("Not connected")
    , midiPortNum(-1)
    , currentAudioDeviceId(-1)
    , currentMidiPortNum(-1)
    , selectedParameterId(0)
    , numericInputActive(false)
    , midiLearnActive(false)
    , midiLearnParameterId(-1)
    , currentPresetName("None")
    , textInputActive(false)
    , deviceChangeRequested(false)
    , requestedAudioDeviceId(-1)
    , requestedMidiPortNum(-1)
    , waveformBuffer(WAVEFORM_BUFFER_SIZE, 0.0f)
    , waveformBufferWritePos(0) {
    
    // Load available presets
    refreshPresetList();
    
    // Initialize parameter definitions
    initializeParameters();
    
}

UI::~UI() {
    if (initialized) {
        endwin();
    }
}

bool UI::initialize() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);
        init_pair(4, COLOR_RED, COLOR_BLACK);
        init_pair(5, COLOR_WHITE, COLOR_BLUE);   // Active tab
        init_pair(6, COLOR_BLACK, COLOR_CYAN);   // Inactive tab
        
        // Check if we have 256 colors available
        if (COLORS >= 256) {
            // Use 256-color grayscale palette (colors 232-255 are grayscale)
            // Map to pairs 7-26 for 20 levels of gray
            for (int i = 0; i < 20; ++i) {
                int grayColor = 232 + i * 23 / 19;  // Map 0-19 to 232-255
                init_pair(7 + i, grayColor, COLOR_BLACK);
            }
        } else {
            // Fallback to 8-color mode with attributes
            // Use pairs 7-12 for basic grayscale (6 levels)
            init_pair(7, COLOR_BLACK, COLOR_BLACK);    // Level 0 - darkest
            init_pair(8, COLOR_BLACK, COLOR_BLACK);    // Level 1 - very dim (will use A_BOLD)
            init_pair(9, COLOR_WHITE, COLOR_BLACK);    // Level 2 - dim (will use A_DIM)
            init_pair(10, COLOR_WHITE, COLOR_BLACK);   // Level 3 - medium (will use A_NORMAL)
            init_pair(11, COLOR_WHITE, COLOR_BLACK);   // Level 4 - bright (will use A_BOLD)
            init_pair(12, COLOR_CYAN, COLOR_BLACK);    // Level 5 - brightest (cyan bold for extra pop)
        }
    }
    
    initialized = true;
    return true;
}

bool UI::update() {
    int ch = getch();
    
    if (ch != ERR) {
        handleInput(ch);
        
        if (ch == 'q' || ch == 'Q') {
            return false;
        }
    }
    
    return true;
}

void UI::setDeviceInfo(const std::string& audioDevice, int sampleRate, int bufferSize,
                       const std::string& midiDevice, int midiPort) {
    audioDeviceName = audioDevice;
    audioSampleRate = sampleRate;
    audioBufferSize = bufferSize;
    midiDeviceName = midiDevice;
    midiPortNum = midiPort;
}

void UI::setAvailableAudioDevices(const std::vector<std::pair<int, std::string>>& devices, int currentDeviceId) {
    availableAudioDevices = devices;
    currentAudioDeviceId = currentDeviceId;
}

void UI::setAvailableMidiDevices(const std::vector<std::pair<int, std::string>>& devices, int currentPort) {
    availableMidiDevices = devices;
    currentMidiPortNum = currentPort;
}

void UI::handleInput(int ch) {
    // Handle preset text input mode
    if (textInputActive) {
        handleTextInput(ch);
        return;
    }
    
    // Handle numeric input mode for parameters
    if (numericInputActive) {
        if (ch == '\n' || ch == KEY_ENTER) {
            finishNumericInput();
        } else if (ch == 27) {  // Escape key
            numericInputActive = false;
            numericInputBuffer.clear();
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            if (!numericInputBuffer.empty()) {
                numericInputBuffer.pop_back();
            }
        } else if (ch >= 32 && ch < 127) {  // Printable characters
            if (numericInputBuffer.length() < 20) {
                numericInputBuffer += static_cast<char>(ch);
            }
        }
        return;
    }
    
    // Get current page parameter IDs
    std::vector<int> pageParams = getParameterIdsForPage(currentPage);
    
    // Tab key cycles forward through pages
    if (ch == '\t') {
        if (currentPage == UIPage::MAIN) currentPage = UIPage::BRAINWAVE;
        else if (currentPage == UIPage::BRAINWAVE) currentPage = UIPage::REVERB;
        else if (currentPage == UIPage::REVERB) currentPage = UIPage::FILTER;
        else if (currentPage == UIPage::FILTER) currentPage = UIPage::LOOPER;
        else if (currentPage == UIPage::LOOPER) currentPage = UIPage::CONFIG;
        else currentPage = UIPage::MAIN;
        
        // Set selected parameter to first parameter on new page
        std::vector<int> newPageParams = getParameterIdsForPage(currentPage);
        if (!newPageParams.empty()) {
            selectedParameterId = newPageParams[0];
        }
        return;
    }
    
    // Up/Down arrows navigate between parameters on current page
    if (ch == KEY_UP) {
        if (!pageParams.empty()) {
            auto it = std::find(pageParams.begin(), pageParams.end(), selectedParameterId);
            if (it != pageParams.end() && it != pageParams.begin()) {
                selectedParameterId = *(--it);
            } else {
                selectedParameterId = pageParams.back(); // Wrap to end
            }
        }
        return;
    } else if (ch == KEY_DOWN) {
        if (!pageParams.empty()) {
            auto it = std::find(pageParams.begin(), pageParams.end(), selectedParameterId);
            if (it != pageParams.end() && (it + 1) != pageParams.end()) {
                selectedParameterId = *(++it);
            } else {
                selectedParameterId = pageParams.front(); // Wrap to beginning
            }
        }
        return;
    }
    
    // Left/Right arrows adjust parameter values
    if (ch == KEY_LEFT) {
        adjustParameter(selectedParameterId, false);
        return;
    } else if (ch == KEY_RIGHT) {
        adjustParameter(selectedParameterId, true);
        return;
    }
    
    // Enter key starts numeric input for current parameter
    if (ch == '\n' || ch == KEY_ENTER) {
        startNumericInput(selectedParameterId);
        return;
    }
    
    // 'L' key starts MIDI learn for current parameter
    if (ch == 'L' || ch == 'l') {
        startMidiLearn(selectedParameterId);
        return;
    }
    
    // Transport and looping hotkeys (keep these)
    if (ch == ' ') {
        // Spacebar behavior depends on current page
        if (currentPage == UIPage::LOOPER) {
            if (loopManager) {
                Looper* loop = loopManager->getCurrentLoop();
                if (loop) loop->pressRecPlay();
            }
        }
        return;
    }
    
    // Looper-specific hotkeys (only active on looper page)
    if (currentPage == UIPage::LOOPER) {
        switch (ch) {
            // Loop selection (1-4)
            case '1': params->currentLoop = 0; break;
            case '2': params->currentLoop = 1; break;
            case '3': params->currentLoop = 2; break;
            case '4': params->currentLoop = 3; break;
                
            // Overdub toggle (O/o)
            case 'O':
            case 'o':
                if (loopManager) {
                    Looper* loop = loopManager->getCurrentLoop();
                    if (loop) loop->pressOverdub();
                }
                break;
                
            // Stop (S/s)
            case 'S':
            case 's':
                if (loopManager) {
                    Looper* loop = loopManager->getCurrentLoop();
                    if (loop) loop->pressStop();
                }
                break;
                
            // Clear (C/c)
            case 'C':
            case 'c':
                if (loopManager) {
                    Looper* loop = loopManager->getCurrentLoop();
                    if (loop) loop->pressClear();
                }
                break;
                
            // Overdub mix ([/])
            case '[':
                params->overdubMix = std::max(0.0f, params->overdubMix.load() - 0.05f);
                break;
            case ']':
                params->overdubMix = std::min(1.0f, params->overdubMix.load() + 0.05f);
                break;
        }
    }
}

void UI::drawTabs() {
    int cols = getmaxx(stdscr);
    
    // Main tab
    if (currentPage == UIPage::MAIN) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 0, " MAIN ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 0, " MAIN ");
        attroff(COLOR_PAIR(6));
    }
    
    // Brainwave tab
    if (currentPage == UIPage::BRAINWAVE) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 6, " BRAINWAVE ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 6, " BRAINWAVE ");
        attroff(COLOR_PAIR(6));
    }
    
    // Reverb tab
    if (currentPage == UIPage::REVERB) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 17, " REVERB ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 17, " REVERB ");
        attroff(COLOR_PAIR(6));
    }
    
    // Filter tab
    if (currentPage == UIPage::FILTER) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 25, " FILTER ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 25, " FILTER ");
        attroff(COLOR_PAIR(6));
    }
    
    // Looper tab
    if (currentPage == UIPage::LOOPER) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 33, " LOOPER ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 33, " LOOPER ");
        attroff(COLOR_PAIR(6));
    }
    
    // Config tab
    if (currentPage == UIPage::CONFIG) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 41, " CONFIG ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 41, " CONFIG ");
        attroff(COLOR_PAIR(6));
    }
    
    
    // Fill rest of line
    for (int i = 44; i < cols; ++i) {
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

void UI::drawMainPage(int activeVoices) {
    int row = 3;
    int selectableRow = 0;
    
    // Preset section
    attron(A_BOLD);
    mvprintw(row++, 1, "PRESET");
    attroff(A_BOLD);
    row++;
    
    // Row 0: Preset selector
    if (selectedRow == selectableRow && !menuPopupActive) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(row++, 2, "> Preset: %s", currentPresetName.c_str());
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        mvprintw(row++, 2, "  Preset: %s", currentPresetName.c_str());
    }
    selectableRow++;
    
    row++;
    
    // Waveform section
    attron(A_BOLD);
    mvprintw(row++, 1, "WAVEFORM");
    attroff(A_BOLD);
    row++;
    
    Waveform currentWaveform = static_cast<Waveform>(params->waveform.load());
    
    // Row 1: Waveform selector
    if (selectedRow == selectableRow && !menuPopupActive) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(row++, 2, "> Waveform: %s", Oscillator::getWaveformName(currentWaveform));
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        mvprintw(row++, 2, "  Waveform: %s", Oscillator::getWaveformName(currentWaveform));
    }
    selectableRow++;
    
    row++;
    
    // Envelope section
    attron(A_BOLD);
    mvprintw(row++, 1, "ENVELOPE");
    attroff(A_BOLD);
    row++;
    
    drawBar(row++, 2, "Attack  (A/a)", params->attack.load(), 0.0f, 30.0f, 20);
    drawBar(row++, 2, "Decay   (D/d)", params->decay.load(), 0.0f, 30.0f, 20);
    drawBar(row++, 2, "Sustain (S/s)", params->sustain.load(), 0.0f, 1.0f, 20);
    drawBar(row++, 2, "Release (R/r)", params->release.load(), 0.0f, 30.0f, 20);
    
    row++;
    
    // Master section
    attron(A_BOLD);
    mvprintw(row++, 1, "MASTER");
    attroff(A_BOLD);
    row++;
    
    drawBar(row++, 2, "Volume  (+/-)", params->masterVolume.load(), 0.0f, 1.0f, 20);
    
    row++;
    
    // Voice info
    attron(A_BOLD);
    mvprintw(row++, 1, "VOICES");
    attroff(A_BOLD);
    row++;
    
    mvprintw(row++, 2, "Active: %d/8", activeVoices);
}

void UI::drawBrainwavePage() {
    int row = 3;
    int selectableRow = 0;
    
    // Mode section
    attron(A_BOLD);
    mvprintw(row++, 1, "OSCILLATOR MODE");
    attroff(A_BOLD);
    row++;
    
    const char* modeNames[] = {"FREE", "KEY"};
    int mode = params->brainwaveMode.load();
    
    // Row 0: Mode selector
    if (selectedRow == selectableRow && !menuPopupActive) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(row++, 2, "> Mode: %s", modeNames[mode]);
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        mvprintw(row++, 2, "  Mode: %s", modeNames[mode]);
    }
    selectableRow++;
    
    // Mode description
    if (mode == 0) {
        attron(COLOR_PAIR(3));
        mvprintw(row++, 2, "FREE: User controls frequency directly");
        attroff(COLOR_PAIR(3));
    } else {
        attron(COLOR_PAIR(3));
        mvprintw(row++, 2, "KEY: MIDI note sets frequency, freq param is offset");
        attroff(COLOR_PAIR(3));
    }
    
    row += 2;
    
    // Wavetable parameters section
    attron(A_BOLD);
    mvprintw(row++, 1, "WAVETABLE PARAMETERS");
    attroff(A_BOLD);
    row++;
    
    if (mode == 0) {
        drawBar(row++, 2, "Frequency (W/w)", params->brainwaveFreq.load(), 20.0f, 2000.0f, 20);
    } else {
        drawBar(row++, 2, "Freq Offset (W/w)", params->brainwaveFreq.load(), 20.0f, 2000.0f, 20);
    }
    drawBar(row++, 2, "Morph (M/m)     ", params->brainwaveMorph.load(), 0.0001f, 0.9999f, 20);
    drawBar(row++, 2, "Duty (P/p)      ", params->brainwaveDuty.load(), 0.0f, 1.0f, 20);
    
    int octave = params->brainwaveOctave.load();
    if (octave > 0) {
        mvprintw(row++, 2, "Octave (O/o): +%d", octave);
    } else if (octave < 0) {
        mvprintw(row++, 2, "Octave (O/o): %d", octave);
    } else {
        mvprintw(row++, 2, "Octave (O/o): 0");
    }
    
    row += 2;
    
    // LFO section
    attron(A_BOLD);
    mvprintw(row++, 1, "LFO MODULATION");
    attroff(A_BOLD);
    row++;
    
    // LFO status
    mvprintw(row, 2, "LFO: ");
    if (params->brainwaveLFOEnabled.load()) {
        attron(COLOR_PAIR(2) | A_BOLD);
        addstr("ON");
        attroff(COLOR_PAIR(2) | A_BOLD);
    } else {
        attron(COLOR_PAIR(4));
        addstr("OFF");
        attroff(COLOR_PAIR(4));
    }
    mvprintw(row++, 20, "(Space to toggle)");
    
    if (params->brainwaveLFOEnabled.load()) {
        mvprintw(row++, 2, "LFO Speed (L/l): %d/9", params->brainwaveLFOSpeed.load());
        
        if (mode == 1) {
            attron(COLOR_PAIR(3));
            mvprintw(row++, 2, "Note: LFO rate scales with MIDI note frequency");
            attroff(COLOR_PAIR(3));
        }
    }
    
    row += 2;
    
    // Help section
    attron(A_BOLD);
    mvprintw(row++, 1, "CONTROLS");
    attroff(A_BOLD);
    row++;
    
    mvprintw(row++, 2, "W/w:   Adjust frequency/offset");
    mvprintw(row++, 2, "M/m:   Adjust morph position");
    mvprintw(row++, 2, "P/p:   Adjust pulse duty");
    mvprintw(row++, 2, "O/o:   Increase/decrease octave (-3 to +3)");
    mvprintw(row++, 2, "Space: Toggle LFO on/off");
    mvprintw(row++, 2, "L/l:   Adjust LFO speed");
    
}

void UI::drawReverbPage() {
    int row = 3;
    int selectableRow = 0;
    
    // Reverb type section
    attron(A_BOLD);
    mvprintw(row++, 1, "REVERB TYPE");
    attroff(A_BOLD);
    row++;
    
    // Get current reverb type name
    const char* reverbTypeNames[] = {"Greyhole", "Plate", "Room", "Hall", "Spring"};
    int reverbType = params->reverbType.load();
    const char* currentReverbType = reverbTypeNames[reverbType];
    
    // Row 0: Reverb type selector
    if (selectedRow == selectableRow && !menuPopupActive) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(row++, 2, "> Type: %s", currentReverbType);
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        mvprintw(row++, 2, "  Type: %s", currentReverbType);
    }
    selectableRow++;
    
    // Show status
    mvprintw(row, 2, "Status: ");
    if (params->reverbEnabled.load()) {
        attron(COLOR_PAIR(2) | A_BOLD);
        addstr("ON");
        attroff(COLOR_PAIR(2) | A_BOLD);
    } else {
        attron(COLOR_PAIR(4));
        addstr("OFF");
        attroff(COLOR_PAIR(4));
    }
    mvprintw(row++, 20, "(Space to toggle)");
    
    row += 2;
    
    // Only show Greyhole parameters if that's selected
    if (reverbType == static_cast<int>(ReverbType::GREYHOLE)) {
        attron(A_BOLD);
        mvprintw(row++, 1, "GREYHOLE PARAMETERS");
        attroff(A_BOLD);
        row++;
        
        float delayTime = 0.001f + params->reverbDelayTime.load() * 1.449f;
        float size = 0.5f + params->reverbSize.load() * 2.5f;
        
        drawBar(row++, 2, "DelayTime (T/t)", delayTime, 0.001f, 1.45f, 20);
        drawBar(row++, 2, "Damping   (X/x)", params->reverbDamping.load(), 0.0f, 0.99f, 20);
        drawBar(row++, 2, "Size      (Z/z)", size, 0.5f, 3.0f, 20);
        drawBar(row++, 2, "Diffusion (B/b)", params->reverbDiffusion.load(), 0.0f, 0.99f, 20);
        drawBar(row++, 2, "Feedback  (V/v)", params->reverbDecay.load(), 0.0f, 1.0f, 20);
        drawBar(row++, 2, "ModDepth  (N/n)", params->reverbModDepth.load(), 0.0f, 1.0f, 20);
        drawBar(row++, 2, "ModFreq   (M/m)", params->reverbModFreq.load(), 0.0f, 10.0f, 20);
        drawBar(row++, 2, "Mix       (C/c)", params->reverbMix.load(), 0.0f, 1.0f, 20);
    } else {
        // Show placeholder for other reverb types
        attron(COLOR_PAIR(3));
        mvprintw(row++, 2, "This reverb type is not yet implemented.");
        mvprintw(row++, 2, "Only Greyhole is currently functional.");
        attroff(COLOR_PAIR(3));
    }
}

void UI::drawFilterPage() {
    int row = 3;
    int selectableRow = 0;
    
    // Filter type section
    attron(A_BOLD);
    mvprintw(row++, 1, "FILTER TYPE");
    attroff(A_BOLD);
    row++;
    
    const char* filterNames[] = {"Lowpass", "Highpass", "High Shelf", "Low Shelf"};
    int filterType = params->filterType.load();
    
    // Row 0: Filter type selector
    if (selectedRow == selectableRow && !menuPopupActive) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(row++, 2, "> Type: %s", filterNames[filterType]);
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        mvprintw(row++, 2, "  Type: %s", filterNames[filterType]);
    }
    selectableRow++;
    
    // Show status
    mvprintw(row, 2, "Status: ");
    if (params->filterEnabled.load()) {
        attron(COLOR_PAIR(2) | A_BOLD);
        addstr("ON");
        attroff(COLOR_PAIR(2) | A_BOLD);
    } else {
        attron(COLOR_PAIR(4));
        addstr("OFF");
        attroff(COLOR_PAIR(4));
    }
    mvprintw(row++, 20, "(Space to toggle)");
    
    row += 2;
    
    // Filter parameters
    attron(A_BOLD);
    mvprintw(row++, 1, "PARAMETERS");
    attroff(A_BOLD);
    row++;
    
    drawBar(row++, 2, "Cutoff (F/f)", params->filterCutoff.load(), 20.0f, 20000.0f, 20);
    
    // Only show gain for shelf filters
    if (filterType >= 2) {
        drawBar(row++, 2, "Gain (G/g)  ", params->filterGain.load(), -24.0f, 24.0f, 20);
    }
    
    row += 2;
    
    // MIDI CC Learn section
    attron(A_BOLD);
    mvprintw(row++, 1, "MIDI CC LEARN");
    attroff(A_BOLD);
    row++;
    
    // CC Learn status
    if (params->ccLearnMode.load()) {
        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(row++, 2, ">>> LEARNING MODE ACTIVE <<<");
        attroff(COLOR_PAIR(3) | A_BOLD);
        mvprintw(row++, 2, "Move a MIDI controller to assign it to Filter Cutoff");
    } else {
        mvprintw(row++, 2, "Press L to learn MIDI CC for Filter Cutoff");
    }
    
    // Show current CC mapping
    int ccNum = params->filterCutoffCC.load();
    if (ccNum >= 0) {
        mvprintw(row, 2, "Filter Cutoff CC: ");
        attron(COLOR_PAIR(2) | A_BOLD);
        printw("CC#%d", ccNum);
        attroff(COLOR_PAIR(2) | A_BOLD);
        printw("  (Press K to clear)");
    } else {
        mvprintw(row, 2, "Filter Cutoff CC: Not assigned");
    }
    row++;
    
    row += 2;
    
    // Parameter descriptions
    attron(A_BOLD);
    mvprintw(row++, 1, "PARAMETER GUIDE");
    attroff(A_BOLD);
    row++;
    
    mvprintw(row++, 2, "Lowpass:    Removes high frequencies");
    mvprintw(row++, 2, "Highpass:   Removes low frequencies");
    mvprintw(row++, 2, "High Shelf: Boost/cut high frequencies");
    mvprintw(row++, 2, "Low Shelf:  Boost/cut low frequencies");
}

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

void UI::drawConfigPage() {
    int row = 3;
    int selectableRow = 0;
    
    // System information
    attron(A_BOLD);
    mvprintw(row++, 1, "DEVICE CONFIGURATION");
    attroff(A_BOLD);
    row++;
    
    attron(COLOR_PAIR(3));
    mvprintw(row++, 2, "Select a device to restart with new configuration");
    attroff(COLOR_PAIR(3));
    row++;
    
    // Audio device section
    attron(A_BOLD);
    mvprintw(row++, 1, "AUDIO DEVICE");
    attroff(A_BOLD);
    row++;
    
    // Row 0: Audio device selector
    if (selectedRow == selectableRow && !menuPopupActive) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(row++, 2, "> Device: %s", audioDeviceName.c_str());
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        mvprintw(row++, 2, "  Device: %s", audioDeviceName.c_str());
    }
    selectableRow++;
    
    mvprintw(row++, 4, "Sample Rate: %d Hz", audioSampleRate);
    mvprintw(row++, 4, "Buffer Size: %d samples", audioBufferSize);
    
    if (audioSampleRate > 0 && audioBufferSize > 0) {
        float latency = (audioBufferSize * 1000.0f) / audioSampleRate;
        mvprintw(row++, 4, "Latency:     %.2f ms", latency);
    }
    
    row += 2;
    
    // MIDI device section
    attron(A_BOLD);
    mvprintw(row++, 1, "MIDI DEVICE");
    attroff(A_BOLD);
    row++;
    
    // Row 1: MIDI device selector
    if (selectedRow == selectableRow && !menuPopupActive) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(row++, 2, "> Device: %s", midiDeviceName.c_str());
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        mvprintw(row++, 2, "  Device: %s", midiDeviceName.c_str());
    }
    selectableRow++;
    
    if (midiPortNum >= 0) {
        mvprintw(row++, 4, "Port:        %d", midiPortNum);
        attron(COLOR_PAIR(2));
        mvprintw(row++, 4, "Status:      Connected");
        attroff(COLOR_PAIR(2));
    } else {
        attron(COLOR_PAIR(4));
        mvprintw(row++, 4, "Status:      Not connected");
        attroff(COLOR_PAIR(4));
    }
    
    row += 2;
    
    // Build info
    attron(A_BOLD);
    mvprintw(row++, 1, "BUILD INFO");
    attroff(A_BOLD);
    row++;
    
    mvprintw(row++, 2, "Max Voices:  8 (polyphonic)");
    mvprintw(row++, 2, "Waveforms:   Sine, Square, Sawtooth, Triangle");
    mvprintw(row++, 2, "Filters:     Lowpass, Highpass, Shelving");
    mvprintw(row++, 2, "Reverb:      Greyhole (Faust 2.37.3)");
}

void UI::addConsoleMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(consoleMutex);
    consoleMessages.push_back(message);
    if (consoleMessages.size() > MAX_CONSOLE_MESSAGES) {
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

void UI::drawHotkeyLine() {
    int maxY = getmaxy(stdscr);
    int maxX = getmaxx(stdscr);
    int row = maxY - 1;
    
    attron(COLOR_PAIR(1));
    mvhline(row - 1, 0, '-', maxX);
    attroff(COLOR_PAIR(1));
    
    if (menuPopupActive || textInputActive) {
        mvprintw(row, 1, "↑↓ Navigate  |  Enter Select  |  Esc Cancel  |  Q Quit");
    } else {
        mvprintw(row, 1, "Tab/←→ Page  |  ↑↓ Select  |  Enter Open Menu  |  Q Quit");
    }
}

void UI::draw(int activeVoices) {
    erase();  // Use erase() instead of clear() - doesn't cause flicker
    
    drawTabs();
    
    switch (currentPage) {
        case UIPage::MAIN:
            drawMainPage(activeVoices);
            drawConsole();  // Console only on main page
            break;
        case UIPage::BRAINWAVE:
            drawBrainwavePage();
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
        case UIPage::CONFIG:
            drawConfigPage();
            break;
    }
    
    drawHotkeyLine();  // Always draw hotkey line at bottom
    
    // Draw popup menu if active
    if (menuPopupActive) {
        std::vector<std::string> items = getMenuItems(currentMenuType);
        std::string title;
        switch (currentMenuType) {
            case MenuType::WAVEFORM: title = "Select Waveform"; break;
            case MenuType::FILTER_TYPE: title = "Select Filter Type"; break;
            case MenuType::REVERB_TYPE: title = "Select Reverb Type"; break;
            case MenuType::PRESET: title = "Preset Management"; break;
            case MenuType::AUDIO_DEVICE: title = "Audio Device"; break;
            case MenuType::MIDI_DEVICE: title = "MIDI Device"; break;
            default: title = "Menu"; break;
        }
        drawPopupMenu(items, title);
    }
    
    // Draw text input overlay if active
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
    }
    
    refresh();
}

// Helper functions

int UI::getSelectableRowCount() {
    switch (currentPage) {
        case UIPage::MAIN: return 2;  // Preset and Waveform
        case UIPage::BRAINWAVE: return 1;  // Mode selector
        case UIPage::REVERB: return 1;  // Reverb type
        case UIPage::FILTER: return 1;  // Filter type
        case UIPage::CONFIG: return 2;  // Audio and MIDI devices
        default: return 0;
    }
}

std::vector<std::string> UI::getMenuItems(MenuType menuType) {
    std::vector<std::string> items;
    
    switch (menuType) {
        case MenuType::WAVEFORM:
            items.push_back("Sine");
            items.push_back("Square");
            items.push_back("Sawtooth");
            items.push_back("Triangle");
            break;
            
        case MenuType::FILTER_TYPE:
            items.push_back("Lowpass");
            items.push_back("Highpass");
            items.push_back("High Shelf");
            items.push_back("Low Shelf");
            break;
            
        case MenuType::REVERB_TYPE:
            items.push_back("Greyhole");
            items.push_back("Plate");
            items.push_back("Room");
            items.push_back("Hall");
            items.push_back("Spring");
            break;
            
        case MenuType::PRESET:
            items.push_back("[Save New...]");
            for (const auto& preset : availablePresets) {
                items.push_back(preset);
            }
            break;
            
        case MenuType::AUDIO_DEVICE:
            for (const auto& dev : availableAudioDevices) {
                std::string item = dev.second;
                if (dev.first == currentAudioDeviceId) {
                    item += " (current)";
                }
                items.push_back(item);
            }
            if (items.empty()) {
                items.push_back("No audio devices available");
            }
            break;
            
        case MenuType::MIDI_DEVICE:
            for (const auto& dev : availableMidiDevices) {
                std::string item = dev.second;
                if (dev.first == currentMidiPortNum) {
                    item += " (current)";
                }
                items.push_back(item);
            }
            if (items.empty()) {
                items.push_back("No MIDI devices available");
            }
            break;
            
        default:
            break;
    }
    
    return items;
}

void UI::openPopupMenu(MenuType menuType) {
    currentMenuType = menuType;
    menuPopupActive = true;
    popupSelectedIndex = 0;
    
    std::vector<std::string> items = getMenuItems(menuType);
    popupItemCount = items.size();
}

void UI::closePopupMenu() {
    menuPopupActive = false;
    currentMenuType = MenuType::NONE;
    popupSelectedIndex = 0;
    popupItemCount = 0;
}

void UI::drawPopupMenu(const std::vector<std::string>& items, const std::string& title) {
    int maxY = getmaxy(stdscr);
    int maxX = getmaxx(stdscr);
    
    int height = items.size() + 4;  // Items + border + title + padding
    int width = 50;
    
    // Adjust width based on content
    for (const auto& item : items) {
        if (static_cast<int>(item.length()) + 6 > width) {
            width = item.length() + 6;
        }
    }
    
    int startY = (maxY - height) / 2;
    int startX = (maxX - width) / 2;
    
    // Draw filled box background
    attron(COLOR_PAIR(6));
    for (int y = startY; y < startY + height; ++y) {
        for (int x = startX; x < startX + width; ++x) {
            mvaddch(y, x, ' ');
        }
    }
    attroff(COLOR_PAIR(6));
    
    // Draw border
    attron(A_BOLD);
    mvhline(startY, startX, '-', width);
    mvhline(startY + height - 1, startX, '-', width);
    mvvline(startY, startX, '|', height);
    mvvline(startY, startX + width - 1, '|', height);
    attroff(A_BOLD);
    
    // Draw title
    attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(startY + 1, startX + 2, "%s", title.c_str());
    attroff(COLOR_PAIR(5) | A_BOLD);
    
    // Draw items
    for (size_t i = 0; i < items.size(); ++i) {
        int itemY = startY + 3 + i;
        
        if (static_cast<int>(i) == popupSelectedIndex) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(itemY, startX + 2, "> %s", items[i].c_str());
            attroff(COLOR_PAIR(5) | A_BOLD);
        } else {
            mvprintw(itemY, startX + 2, "  %s", items[i].c_str());
        }
    }
}

void UI::refreshPresetList() {
    availablePresets = PresetManager::listPresets();
}

void UI::loadPreset(const std::string& filename) {
    if (PresetManager::loadPreset(filename, params)) {
        currentPresetName = filename;
    }
}

void UI::savePreset(const std::string& filename) {
    if (PresetManager::savePreset(filename, params)) {
        currentPresetName = filename;
        refreshPresetList();
    }
}

void UI::startTextInput() {
    textInputActive = true;
    textInputBuffer.clear();
    closePopupMenu();
}

void UI::handleTextInput(int ch) {
    if (ch == '\n' || ch == KEY_ENTER) {
        // Save preset with entered name
        if (!textInputBuffer.empty()) {
            savePreset(textInputBuffer);
        }
        finishTextInput();
    } else if (ch == 27) {  // Escape
        finishTextInput();
    } else if (ch == KEY_BACKSPACE || ch == 127) {
        if (!textInputBuffer.empty()) {
            textInputBuffer.pop_back();
        }
    } else if (ch >= 32 && ch < 127) {  // Printable characters
        if (textInputBuffer.length() < 30) {
            textInputBuffer += static_cast<char>(ch);
        }
    }
}

void UI::finishTextInput() {
    textInputActive = false;
    textInputBuffer.clear();
}

// Parameter management functions
void UI::initializeParameters() {
    parameters.clear();
    
    // MAIN page parameters
    parameters.push_back({1, ParamType::ENUM, "Waveform", "", 0, 3, {"Sine", "Square", "Sawtooth", "Triangle"}, false, static_cast<int>(UIPage::MAIN)});
    parameters.push_back({2, ParamType::FLOAT, "Attack", "s", 0.001f, 30.0f, {}, false, static_cast<int>(UIPage::MAIN)});
    parameters.push_back({3, ParamType::FLOAT, "Decay", "s", 0.001f, 30.0f, {}, false, static_cast<int>(UIPage::MAIN)});
    parameters.push_back({4, ParamType::FLOAT, "Sustain", "", 0.0f, 1.0f, {}, false, static_cast<int>(UIPage::MAIN)});
    parameters.push_back({5, ParamType::FLOAT, "Release", "s", 0.001f, 30.0f, {}, false, static_cast<int>(UIPage::MAIN)});
    parameters.push_back({6, ParamType::FLOAT, "Master Volume", "", 0.0f, 1.0f, {}, false, static_cast<int>(UIPage::MAIN)});
    
    // BRAINWAVE page parameters
    parameters.push_back({10, ParamType::ENUM, "Mode", "", 0, 1, {"FREE", "KEY"}, false, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({11, ParamType::FLOAT, "Frequency", "Hz", 20.0f, 2000.0f, {}, false, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({12, ParamType::FLOAT, "Morph", "", 0.0001f, 0.9999f, {}, false, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({13, ParamType::FLOAT, "Duty", "", 0.0f, 1.0f, {}, false, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({14, ParamType::INT, "Octave", "", -3, 3, {}, false, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({15, ParamType::BOOL, "LFO Enabled", "", 0, 1, {}, false, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({16, ParamType::INT, "LFO Speed", "", 0, 9, {}, false, static_cast<int>(UIPage::BRAINWAVE)});
    
    // REVERB page parameters  
    parameters.push_back({20, ParamType::ENUM, "Reverb Type", "", 0, 4, {"Greyhole", "Plate", "Room", "Hall", "Spring"}, false, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({21, ParamType::BOOL, "Reverb Enabled", "", 0, 1, {}, false, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({22, ParamType::FLOAT, "Delay Time", "", 0.0f, 1.0f, {}, false, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({23, ParamType::FLOAT, "Size", "", 0.0f, 1.0f, {}, false, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({24, ParamType::FLOAT, "Damping", "", 0.0f, 0.99f, {}, false, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({25, ParamType::FLOAT, "Mix", "", 0.0f, 1.0f, {}, false, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({26, ParamType::FLOAT, "Decay", "", 0.0f, 1.0f, {}, false, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({27, ParamType::FLOAT, "Diffusion", "", 0.0f, 0.99f, {}, false, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({28, ParamType::FLOAT, "Mod Depth", "", 0.0f, 1.0f, {}, false, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({29, ParamType::FLOAT, "Mod Freq", "", 0.0f, 10.0f, {}, false, static_cast<int>(UIPage::REVERB)});
    
    // FILTER page parameters
    parameters.push_back({30, ParamType::ENUM, "Filter Type", "", 0, 3, {"Lowpass", "Highpass", "High Shelf", "Low Shelf"}, false, static_cast<int>(UIPage::FILTER)});
    parameters.push_back({31, ParamType::BOOL, "Filter Enabled", "", 0, 1, {}, false, static_cast<int>(UIPage::FILTER)});
    parameters.push_back({32, ParamType::FLOAT, "Cutoff", "Hz", 20.0f, 20000.0f, {}, true, static_cast<int>(UIPage::FILTER)});
    parameters.push_back({33, ParamType::FLOAT, "Gain", "dB", -24.0f, 24.0f, {}, false, static_cast<int>(UIPage::FILTER)});
    
    // LOOPER page parameters
    parameters.push_back({40, ParamType::INT, "Current Loop", "", 0, 3, {}, false, static_cast<int>(UIPage::LOOPER)});
    parameters.push_back({41, ParamType::FLOAT, "Overdub Mix", "", 0.0f, 1.0f, {}, false, static_cast<int>(UIPage::LOOPER)});
}

InlineParameter* UI::getParameter(int id) {
    for (auto& param : parameters) {
        if (param.id == id) {
            return &param;
        }
    }
    return nullptr;
}

std::vector<int> UI::getParameterIdsForPage(UIPage page) {
    std::vector<int> ids;
    int pageInt = static_cast<int>(page);
    for (const auto& param : parameters) {
        if (param.page == pageInt) {
            ids.push_back(param.id);
        }
    }
    return ids;
}

float UI::getParameterValue(int id) {
    switch (id) {
        case 1: return static_cast<float>(params->waveform.load());
        case 2: return params->attack.load();
        case 3: return params->decay.load();
        case 4: return params->sustain.load();
        case 5: return params->release.load();
        case 6: return params->masterVolume.load();
        case 10: return static_cast<float>(params->brainwaveMode.load());
        case 11: return params->brainwaveFreq.load();
        case 12: return params->brainwaveMorph.load();
        case 13: return params->brainwaveDuty.load();
        case 14: return static_cast<float>(params->brainwaveOctave.load());
        case 15: return params->brainwaveLFOEnabled.load() ? 1.0f : 0.0f;
        case 16: return static_cast<float>(params->brainwaveLFOSpeed.load());
        case 20: return static_cast<float>(params->reverbType.load());
        case 21: return params->reverbEnabled.load() ? 1.0f : 0.0f;
        case 22: return params->reverbDelayTime.load();
        case 23: return params->reverbSize.load();
        case 24: return params->reverbDamping.load();
        case 25: return params->reverbMix.load();
        case 26: return params->reverbDecay.load();
        case 27: return params->reverbDiffusion.load();
        case 28: return params->reverbModDepth.load();
        case 29: return params->reverbModFreq.load();
        case 30: return static_cast<float>(params->filterType.load());
        case 31: return params->filterEnabled.load() ? 1.0f : 0.0f;
        case 32: return params->filterCutoff.load();
        case 33: return params->filterGain.load();
        case 40: return static_cast<float>(params->currentLoop.load());
        case 41: return params->overdubMix.load();
        default: return 0.0f;
    }
}

void UI::setParameterValue(int id, float value) {
    switch (id) {
        case 1: params->waveform = static_cast<int>(value); break;
        case 2: params->attack = value; break;
        case 3: params->decay = value; break;
        case 4: params->sustain = value; break;
        case 5: params->release = value; break;
        case 6: params->masterVolume = value; break;
        case 10: params->brainwaveMode = static_cast<int>(value); break;
        case 11: params->brainwaveFreq = value; break;
        case 12: params->brainwaveMorph = value; break;
        case 13: params->brainwaveDuty = value; break;
        case 14: params->brainwaveOctave = static_cast<int>(value); break;
        case 15: params->brainwaveLFOEnabled = (value > 0.5f); break;
        case 16: params->brainwaveLFOSpeed = static_cast<int>(value); break;
        case 20: params->reverbType = static_cast<int>(value); break;
        case 21: params->reverbEnabled = (value > 0.5f); break;
        case 22: params->reverbDelayTime = value; break;
        case 23: params->reverbSize = value; break;
        case 24: params->reverbDamping = value; break;
        case 25: params->reverbMix = value; break;
        case 26: params->reverbDecay = value; break;
        case 27: params->reverbDiffusion = value; break;
        case 28: params->reverbModDepth = value; break;
        case 29: params->reverbModFreq = value; break;
        case 30: params->filterType = static_cast<int>(value); break;
        case 31: params->filterEnabled = (value > 0.5f); break;
        case 32: params->filterCutoff = value; break;
        case 33: params->filterGain = value; break;
        case 40: params->currentLoop = static_cast<int>(value); break;
        case 41: params->overdubMix = value; break;
    }
}

void UI::adjustParameter(int id, bool increase) {
    InlineParameter* param = getParameter(id);
    if (!param) return;
    
    float currentValue = getParameterValue(id);
    float newValue = currentValue;
    
    switch (param->type) {
        case ParamType::FLOAT: {
            float step = (param->max_val - param->min_val) * 0.01f; // 1% steps
            
            // Special exponential handling for certain parameters
            if (id == 2 || id == 3 || id == 5) { // Attack, Decay, Release - exponential scaling
                if (increase) {
                    newValue = std::min(param->max_val, currentValue * 1.1f);
                } else {
                    newValue = std::max(param->min_val, currentValue / 1.1f);
                }
            } else if (id == 11) { // Brainwave frequency - musical scaling
                if (increase) {
                    newValue = std::min(param->max_val, currentValue * 1.059463f); // semitone ratio
                } else {
                    newValue = std::max(param->min_val, currentValue / 1.059463f);
                }
            } else if (id == 32) { // Filter cutoff - logarithmic scaling
                if (increase) {
                    newValue = std::min(param->max_val, currentValue * 1.1f);
                } else {
                    newValue = std::max(param->min_val, currentValue / 1.1f);
                }
            } else {
                // Linear scaling for other parameters
                if (increase) {
                    newValue = std::min(param->max_val, currentValue + step);
                } else {
                    newValue = std::max(param->min_val, currentValue - step);
                }
            }
            break;
        }
        case ParamType::INT: {
            int intValue = static_cast<int>(currentValue);
            if (increase) {
                intValue = std::min(static_cast<int>(param->max_val), intValue + 1);
            } else {
                intValue = std::max(static_cast<int>(param->min_val), intValue - 1);
            }
            newValue = static_cast<float>(intValue);
            break;
        }
        case ParamType::ENUM: {
            int enumValue = static_cast<int>(currentValue);
            if (increase) {
                enumValue = std::min(static_cast<int>(param->max_val), enumValue + 1);
            } else {
                enumValue = std::max(static_cast<int>(param->min_val), enumValue - 1);
            }
            newValue = static_cast<float>(enumValue);
            break;
        }
        case ParamType::BOOL: {
            newValue = (currentValue > 0.5f) ? 0.0f : 1.0f;
            break;
        }
    }
    
    setParameterValue(id, newValue);
}

std::string UI::getParameterDisplayString(int id) {
    InlineParameter* param = getParameter(id);
    if (!param) return "";
    
    float value = getParameterValue(id);
    
    switch (param->type) {
        case ParamType::FLOAT: {
            if (param->unit.empty()) {
                return std::to_string(value);
            } else {
                return std::to_string(value) + " " + param->unit;
            }
        }
        case ParamType::INT: {
            return std::to_string(static_cast<int>(value));
        }
        case ParamType::ENUM: {
            int enumIndex = static_cast<int>(value);
            if (enumIndex >= 0 && enumIndex < static_cast<int>(param->enum_values.size())) {
                return param->enum_values[enumIndex];
            }
            return "Unknown";
        }
        case ParamType::BOOL: {
            return (value > 0.5f) ? "ON" : "OFF";
        }
    }
    return "";
}

void UI::startNumericInput(int id) {
    numericInputActive = true;
    selectedParameterId = id;
    numericInputBuffer.clear();
}

void UI::finishNumericInput() {
    if (!numericInputActive) return;
    
    InlineParameter* param = getParameter(selectedParameterId);
    if (param && !numericInputBuffer.empty()) {
        try {
            float value = std::stof(numericInputBuffer);
            value = std::max(param->min_val, std::min(param->max_val, value));
            setParameterValue(selectedParameterId, value);
        } catch (...) {
            // Invalid input, ignore
        }
    }
    
    numericInputActive = false;
    numericInputBuffer.clear();
}

void UI::startMidiLearn(int id) {
    InlineParameter* param = getParameter(id);
    if (param && param->supports_midi_learn) {
        midiLearnActive = true;
        midiLearnParameterId = id;
        addConsoleMessage("MIDI Learn active for " + param->name + " - move a controller");
    }
}

void UI::finishMidiLearn() {
    midiLearnActive = false;
    midiLearnParameterId = -1;
}

void UI::writeToWaveformBuffer(float sample) {
    int pos = waveformBufferWritePos.load(std::memory_order_relaxed);
    waveformBuffer[pos] = sample;
    waveformBufferWritePos.store((pos + 1) % WAVEFORM_BUFFER_SIZE, std::memory_order_relaxed);
}

