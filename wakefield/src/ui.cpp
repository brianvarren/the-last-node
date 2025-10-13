#include "ui.h"
#include "synth.h"
#include "preset.h"
#include <cmath>

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
    , selectedRow(0)
    , menuPopupActive(false)
    , popupSelectedIndex(0)
    , popupItemCount(0)
    , currentMenuType(MenuType::NONE)
    , currentPresetName("None")
    , textInputActive(false)
    , deviceChangeRequested(false)
    , requestedAudioDeviceId(-1)
    , requestedMidiPortNum(-1)
    , testOscPhase(0.0f)
    , testOscFreq(0.2f)
    , scopeFadeTime(2.0f) {
    
    // Load available presets
    refreshPresetList();
    
    // Initialize oscilloscope buffer
    for (int x = 0; x < SCOPE_WIDTH; ++x) {
        for (int y = 0; y < SCOPE_HEIGHT; ++y) {
            scopeBuffer[x][y] = 0.0f;
        }
    }
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
        
        // Grayscale colors for oscilloscope (pairs 7-11)
        // These will fade from dim to bright
        init_pair(7, COLOR_BLACK, COLOR_BLACK);    // Darkest (off)
        init_pair(8, COLOR_BLACK, COLOR_BLACK);    // Very dim
        init_pair(9, COLOR_WHITE, COLOR_BLACK);    // Dim
        init_pair(10, COLOR_WHITE, COLOR_BLACK);   // Medium
        init_pair(11, COLOR_WHITE, COLOR_BLACK);   // Bright (full)
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
    const float smallStep = 0.01f;
    const float largeStep = 0.1f;
    
    // Handle text input mode
    if (textInputActive) {
        handleTextInput(ch);
        return;
    }
    
    // Handle popup menu navigation
    if (menuPopupActive) {
        if (ch == KEY_UP) {
            popupSelectedIndex--;
            if (popupSelectedIndex < 0) popupSelectedIndex = popupItemCount - 1;
            return;
        } else if (ch == KEY_DOWN) {
            popupSelectedIndex++;
            if (popupSelectedIndex >= popupItemCount) popupSelectedIndex = 0;
            return;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            // Select current item
            std::vector<std::string> items = getMenuItems(currentMenuType);
            if (popupSelectedIndex >= 0 && popupSelectedIndex < static_cast<int>(items.size())) {
                std::string selected = items[popupSelectedIndex];
                
                switch (currentMenuType) {
                    case MenuType::WAVEFORM:
                        if (selected == "Sine") params->waveform = static_cast<int>(Waveform::SINE);
                        else if (selected == "Square") params->waveform = static_cast<int>(Waveform::SQUARE);
                        else if (selected == "Sawtooth") params->waveform = static_cast<int>(Waveform::SAWTOOTH);
                        else if (selected == "Triangle") params->waveform = static_cast<int>(Waveform::TRIANGLE);
                        break;
                    case MenuType::FILTER_TYPE:
                        if (selected == "Lowpass") params->filterType = 0;
                        else if (selected == "Highpass") params->filterType = 1;
                        else if (selected == "High Shelf") params->filterType = 2;
                        else if (selected == "Low Shelf") params->filterType = 3;
                        break;
                    case MenuType::REVERB_TYPE:
                        if (selected == "Greyhole") params->reverbType = static_cast<int>(ReverbType::GREYHOLE);
                        else if (selected == "Plate") params->reverbType = static_cast<int>(ReverbType::PLATE);
                        else if (selected == "Room") params->reverbType = static_cast<int>(ReverbType::ROOM);
                        else if (selected == "Hall") params->reverbType = static_cast<int>(ReverbType::HALL);
                        else if (selected == "Spring") params->reverbType = static_cast<int>(ReverbType::SPRING);
                        break;
                    case MenuType::PRESET:
                        if (selected == "[Save New...]") {
                            startTextInput();
                        } else {
                            // Load preset
                            loadPreset(selected);
                            currentPresetName = selected;
                        }
                        break;
                    case MenuType::AUDIO_DEVICE:
                        // Find selected device in available list
                        for (const auto& dev : availableAudioDevices) {
                            if (selected.find(dev.second) != std::string::npos) {
                                if (dev.first != currentAudioDeviceId) {
                                    requestedAudioDeviceId = dev.first;
                                    requestedMidiPortNum = currentMidiPortNum;  // Keep current MIDI
                                    deviceChangeRequested = true;
                                }
                                break;
                            }
                        }
                        break;
                    case MenuType::MIDI_DEVICE:
                        // Find selected device in available list
                        for (const auto& dev : availableMidiDevices) {
                            if (selected.find(dev.second) != std::string::npos) {
                                if (dev.first != currentMidiPortNum) {
                                    requestedAudioDeviceId = currentAudioDeviceId;  // Keep current audio
                                    requestedMidiPortNum = dev.first;
                                    deviceChangeRequested = true;
                                }
                                break;
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
            closePopupMenu();
            return;
        } else if (ch == 27) {  // Escape key
            closePopupMenu();
            return;
        }
        return;
    }
    
    // Page navigation (when no popup active)
    if (ch == KEY_LEFT) {
        if (currentPage == UIPage::REVERB) currentPage = UIPage::MAIN;
        else if (currentPage == UIPage::FILTER) currentPage = UIPage::REVERB;
        else if (currentPage == UIPage::CONFIG) currentPage = UIPage::FILTER;
        else if (currentPage == UIPage::TEST) currentPage = UIPage::CONFIG;
        selectedRow = 0;
        return;
    } else if (ch == KEY_RIGHT) {
        if (currentPage == UIPage::MAIN) currentPage = UIPage::REVERB;
        else if (currentPage == UIPage::REVERB) currentPage = UIPage::FILTER;
        else if (currentPage == UIPage::FILTER) currentPage = UIPage::CONFIG;
        else if (currentPage == UIPage::CONFIG) currentPage = UIPage::TEST;
        selectedRow = 0;
        return;
    } else if (ch == '\t') {  // Tab key cycles forward
        if (currentPage == UIPage::MAIN) currentPage = UIPage::REVERB;
        else if (currentPage == UIPage::REVERB) currentPage = UIPage::FILTER;
        else if (currentPage == UIPage::FILTER) currentPage = UIPage::CONFIG;
        else if (currentPage == UIPage::CONFIG) currentPage = UIPage::TEST;
        else currentPage = UIPage::MAIN;
        selectedRow = 0;
        return;
    }
    
    // Row navigation
    if (ch == KEY_UP) {
        selectedRow--;
        if (selectedRow < 0) selectedRow = getSelectableRowCount() - 1;
        return;
    } else if (ch == KEY_DOWN) {
        selectedRow++;
        if (selectedRow >= getSelectableRowCount()) selectedRow = 0;
        return;
    } else if (ch == '\n' || ch == KEY_ENTER) {
        // Open appropriate popup menu based on current page and selected row
        if (currentPage == UIPage::MAIN) {
            if (selectedRow == 0) openPopupMenu(MenuType::PRESET);
            else if (selectedRow == 1) openPopupMenu(MenuType::WAVEFORM);
        } else if (currentPage == UIPage::REVERB) {
            if (selectedRow == 0) openPopupMenu(MenuType::REVERB_TYPE);
        } else if (currentPage == UIPage::FILTER) {
            if (selectedRow == 0) openPopupMenu(MenuType::FILTER_TYPE);
        } else if (currentPage == UIPage::CONFIG) {
            if (selectedRow == 0) openPopupMenu(MenuType::AUDIO_DEVICE);
            else if (selectedRow == 1) openPopupMenu(MenuType::MIDI_DEVICE);
        }
        return;
    }
    
    // Spacebar to toggle on/off
    if (ch == ' ') {
        if (currentPage == UIPage::FILTER) {
            params->filterEnabled = !params->filterEnabled.load();
        } else if (currentPage == UIPage::REVERB) {
            params->reverbEnabled = !params->reverbEnabled.load();
        }
        return;
    }
    
    // Page-specific controls (keep letter key shortcuts)
    if (currentPage == UIPage::MAIN) {
        switch (ch) {
            // Attack control (A/a)
            case 'A':
                params->attack = std::min(2.0f, params->attack.load() + largeStep);
                break;
            case 'a':
                params->attack = std::max(0.001f, params->attack.load() - largeStep);
                break;
                
            // Decay control (D/d)
            case 'D':
                params->decay = std::min(2.0f, params->decay.load() + largeStep);
                break;
            case 'd':
                params->decay = std::max(0.001f, params->decay.load() - largeStep);
                break;
                
            // Sustain control (S/s)
            case 'S':
                params->sustain = std::min(1.0f, params->sustain.load() + smallStep);
                break;
            case 's':
                params->sustain = std::max(0.0f, params->sustain.load() - smallStep);
                break;
                
            // Release control (R/r)
            case 'R':
                params->release = std::min(2.0f, params->release.load() + largeStep);
                break;
            case 'r':
                params->release = std::max(0.001f, params->release.load() - largeStep);
                break;
                
            // Master volume
            case '+':
            case '=':
                params->masterVolume = std::min(1.0f, params->masterVolume.load() + smallStep);
                break;
            case '-':
            case '_':
                params->masterVolume = std::max(0.0f, params->masterVolume.load() - smallStep);
                break;
        }
    } else if (currentPage == UIPage::REVERB) {
        switch (ch) {
            // DelayTime (T/t)
            case 'T':
                params->reverbDelayTime = std::min(1.0f, params->reverbDelayTime.load() + smallStep);
                break;
            case 't':
                params->reverbDelayTime = std::max(0.0f, params->reverbDelayTime.load() - smallStep);
                break;
                
            // Size (Z/z)
            case 'Z':
                params->reverbSize = std::min(1.0f, params->reverbSize.load() + smallStep);
                break;
            case 'z':
                params->reverbSize = std::max(0.0f, params->reverbSize.load() - smallStep);
                break;
                
            // Damping (X/x)
            case 'X':
                params->reverbDamping = std::min(1.0f, params->reverbDamping.load() + smallStep);
                break;
            case 'x':
                params->reverbDamping = std::max(0.0f, params->reverbDamping.load() - smallStep);
                break;
                
            // Mix (C/c)
            case 'C':
                params->reverbMix = std::min(1.0f, params->reverbMix.load() + smallStep);
                break;
            case 'c':
                params->reverbMix = std::max(0.0f, params->reverbMix.load() - smallStep);
                break;
                
            // Decay (V/v)
            case 'V':
                params->reverbDecay = std::min(0.99f, params->reverbDecay.load() + smallStep);
                break;
            case 'v':
                params->reverbDecay = std::max(0.0f, params->reverbDecay.load() - smallStep);
                break;
                
            // Diffusion (B/b)
            case 'B':
                params->reverbDiffusion = std::min(0.99f, params->reverbDiffusion.load() + smallStep);
                break;
            case 'b':
                params->reverbDiffusion = std::max(0.0f, params->reverbDiffusion.load() - smallStep);
                break;
                
            // Mod Depth (N/n)
            case 'N':
                params->reverbModDepth = std::min(1.0f, params->reverbModDepth.load() + smallStep);
                break;
            case 'n':
                params->reverbModDepth = std::max(0.0f, params->reverbModDepth.load() - smallStep);
                break;
                
            // Mod Freq (M/m)
            case 'M':
                params->reverbModFreq = std::min(10.0f, params->reverbModFreq.load() + smallStep);
                break;
            case 'm':
                params->reverbModFreq = std::max(0.0f, params->reverbModFreq.load() - smallStep);
                break;
        }
    } else if (currentPage == UIPage::FILTER) {
        switch (ch) {
            // Cutoff (F/f) - logarithmic scaling for frequency
            case 'F': {
                float current = params->filterCutoff.load();
                params->filterCutoff = std::min(20000.0f, current * 1.1f);
                break;
            }
            case 'f': {
                float current = params->filterCutoff.load();
                params->filterCutoff = std::max(20.0f, current / 1.1f);
                break;
            }
                
            // Gain (G/g) - for shelf filters
            case 'G':
                params->filterGain = std::min(24.0f, params->filterGain.load() + 1.0f);
                break;
            case 'g':
                params->filterGain = std::max(-24.0f, params->filterGain.load() - 1.0f);
                break;
                
            // MIDI CC Learn (L/l)
            case 'L':
            case 'l':
                params->ccLearnMode = !params->ccLearnMode.load();
                if (params->ccLearnMode.load()) {
                    params->ccLearnTarget = 0;  // 0 = filter cutoff
                } else {
                    params->ccLearnTarget = -1;
                }
                break;
                
            // Clear CC mapping
            case 'K':
            case 'k':
                params->filterCutoffCC = -1;
                break;
        }
    } else if (currentPage == UIPage::TEST) {
        switch (ch) {
            // Fade time control (F/f)
            case 'F':
                scopeFadeTime = std::min(5.0f, scopeFadeTime + 0.5f);
                break;
            case 'f':
                scopeFadeTime = std::max(0.5f, scopeFadeTime - 0.5f);
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
    
    // Reverb tab
    if (currentPage == UIPage::REVERB) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 6, " REVERB ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 6, " REVERB ");
        attroff(COLOR_PAIR(6));
    }
    
    // Filter tab
    if (currentPage == UIPage::FILTER) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 14, " FILTER ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 14, " FILTER ");
        attroff(COLOR_PAIR(6));
    }
    
    // Config tab
    if (currentPage == UIPage::CONFIG) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 22, " CONFIG ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 22, " CONFIG ");
        attroff(COLOR_PAIR(6));
    }
    
    // Test tab
    if (currentPage == UIPage::TEST) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 30, " TEST ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 30, " TEST ");
        attroff(COLOR_PAIR(6));
    }
    
    // Fill rest of line
    for (int i = 36; i < cols; ++i) {
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
    
    drawBar(row++, 2, "Attack  (A/a)", params->attack.load(), 0.0f, 2.0f, 20);
    drawBar(row++, 2, "Decay   (D/d)", params->decay.load(), 0.0f, 2.0f, 20);
    drawBar(row++, 2, "Sustain (S/s)", params->sustain.load(), 0.0f, 1.0f, 20);
    drawBar(row++, 2, "Release (R/r)", params->release.load(), 0.0f, 2.0f, 20);
    
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
        case UIPage::REVERB:
            drawReverbPage();
            break;
        case UIPage::FILTER:
            drawFilterPage();
            break;
        case UIPage::CONFIG:
            drawConfigPage();
            break;
        case UIPage::TEST:
            drawTestPage();
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
        case UIPage::REVERB: return 1;  // Reverb type
        case UIPage::FILTER: return 1;  // Filter type
        case UIPage::CONFIG: return 2;  // Audio and MIDI devices
        case UIPage::TEST: return 0;  // No selectable rows
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

void UI::updateTestOscillator(float deltaTime) {
    // Advance oscillator phase
    testOscPhase += 2.0f * M_PI * testOscFreq * deltaTime;
    
    // Wrap phase to maintain phase-lock (0 to 2π)
    while (testOscPhase >= 2.0f * M_PI) {
        testOscPhase -= 2.0f * M_PI;
    }
    
    // Decay all pixels in the buffer based on fade time
    float decayRate = 1.0f / scopeFadeTime;  // How fast to fade (per second)
    float decayAmount = decayRate * deltaTime;
    
    for (int x = 0; x < SCOPE_WIDTH; ++x) {
        for (int y = 0; y < SCOPE_HEIGHT; ++y) {
            scopeBuffer[x][y] -= decayAmount;
            if (scopeBuffer[x][y] < 0.0f) {
                scopeBuffer[x][y] = 0.0f;
            }
        }
    }
    
    // Calculate current sine wave value and position
    float sineValue = std::sin(testOscPhase);  // -1 to +1
    
    // Map phase (0 to 2π) to horizontal position (0 to SCOPE_WIDTH-1)
    int xPos = static_cast<int>((testOscPhase / (2.0f * M_PI)) * (SCOPE_WIDTH - 1));
    
    // Map sine value (-1 to +1) to vertical position (0 to SCOPE_HEIGHT-1)
    // Center of display is at SCOPE_HEIGHT/2
    int yPos = static_cast<int>((SCOPE_HEIGHT / 2) - (sineValue * (SCOPE_HEIGHT / 2 - 1)));
    
    // Clamp to buffer bounds
    if (xPos >= 0 && xPos < SCOPE_WIDTH && yPos >= 0 && yPos < SCOPE_HEIGHT) {
        scopeBuffer[xPos][yPos] = 1.0f;  // Maximum intensity at current position
    }
}

void UI::drawTestPage() {
    int row = 3;
    
    // Title
    attron(A_BOLD);
    mvprintw(row++, 1, "OSCILLOSCOPE TEST PAGE");
    attroff(A_BOLD);
    row++;
    
    // Info section
    mvprintw(row++, 2, "Frequency: %.2f Hz", testOscFreq);
    mvprintw(row++, 2, "Phase:     %.2f rad (%.1f deg)", testOscPhase, testOscPhase * 180.0f / M_PI);
    row++;
    
    // Fade time control
    drawBar(row++, 2, "Fade Time (F/f)", scopeFadeTime, 0.5f, 5.0f, 20);
    row += 2;
    
    // Draw oscilloscope display
    attron(A_BOLD);
    mvprintw(row++, 1, "SCOPE DISPLAY");
    attroff(A_BOLD);
    row++;
    
    // Draw top border
    int scopeStartX = 2;
    int scopeStartY = row;
    mvaddch(scopeStartY, scopeStartX, '+');
    for (int x = 0; x < SCOPE_WIDTH; ++x) {
        mvaddch(scopeStartY, scopeStartX + 1 + x, '-');
    }
    mvaddch(scopeStartY, scopeStartX + 1 + SCOPE_WIDTH, '+');
    
    // Draw scope content with grayscale fade
    for (int y = 0; y < SCOPE_HEIGHT; ++y) {
        mvaddch(scopeStartY + 1 + y, scopeStartX, '|');
        
        for (int x = 0; x < SCOPE_WIDTH; ++x) {
            float intensity = scopeBuffer[x][y];
            char displayChar = '+';
            int colorPair = 7;  // Default: darkest (off)
            int attr = 0;
            
            // Map intensity (0.0 to 1.0) to grayscale using color and attributes
            if (intensity >= 0.8f) {
                colorPair = 11;
                attr = A_BOLD;  // Brightest - white bold
            } else if (intensity >= 0.6f) {
                colorPair = 10;
                attr = A_NORMAL;  // Bright - white normal
            } else if (intensity >= 0.4f) {
                colorPair = 9;
                attr = A_DIM;  // Medium - white dim
            } else if (intensity >= 0.2f) {
                colorPair = 8;
                attr = A_DIM;  // Dim - black dim
            } else if (intensity >= 0.05f) {
                colorPair = 7;
                attr = A_DIM;  // Very dim - black dim
            } else {
                displayChar = ' ';  // Off - just space
            }
            
            if (displayChar != ' ') {
                attron(COLOR_PAIR(colorPair) | attr);
                mvaddch(scopeStartY + 1 + y, scopeStartX + 1 + x, displayChar);
                attroff(COLOR_PAIR(colorPair) | attr);
            } else {
                mvaddch(scopeStartY + 1 + y, scopeStartX + 1 + x, displayChar);
            }
        }
        
        mvaddch(scopeStartY + 1 + y, scopeStartX + 1 + SCOPE_WIDTH, '|');
    }
    
    // Draw bottom border
    mvaddch(scopeStartY + 1 + SCOPE_HEIGHT, scopeStartX, '+');
    for (int x = 0; x < SCOPE_WIDTH; ++x) {
        mvaddch(scopeStartY + 1 + SCOPE_HEIGHT, scopeStartX + 1 + x, '-');
    }
    mvaddch(scopeStartY + 1 + SCOPE_HEIGHT, scopeStartX + 1 + SCOPE_WIDTH, '+');
    
    // Instructions
    row = scopeStartY + SCOPE_HEIGHT + 3;
    attron(COLOR_PAIR(3));
    mvprintw(row++, 2, "Use F/f to adjust fade time");
    mvprintw(row++, 2, "The trace shows a phase-locked %.2f Hz sine wave with grayscale fading", testOscFreq);
    attroff(COLOR_PAIR(3));
}