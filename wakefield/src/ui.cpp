#include "ui.h"
#include "synth.h"
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
    , midiPortNum(-1) {
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

void UI::handleInput(int ch) {
    const float smallStep = 0.01f;
    const float largeStep = 0.1f;
    
    // Page navigation
    if (ch == KEY_LEFT) {
        if (currentPage == UIPage::REVERB) currentPage = UIPage::MAIN;
        else if (currentPage == UIPage::FILTER) currentPage = UIPage::REVERB;
        else if (currentPage == UIPage::INFO) currentPage = UIPage::FILTER;
        return;
    } else if (ch == KEY_RIGHT) {
        if (currentPage == UIPage::MAIN) currentPage = UIPage::REVERB;
        else if (currentPage == UIPage::REVERB) currentPage = UIPage::FILTER;
        else if (currentPage == UIPage::FILTER) currentPage = UIPage::INFO;
        return;
    } else if (ch == '\t') {  // Tab key cycles forward
        if (currentPage == UIPage::MAIN) currentPage = UIPage::REVERB;
        else if (currentPage == UIPage::REVERB) currentPage = UIPage::FILTER;
        else if (currentPage == UIPage::FILTER) currentPage = UIPage::INFO;
        else currentPage = UIPage::MAIN;
        return;
    }
    
    // Page-specific controls
    if (currentPage == UIPage::MAIN) {
        switch (ch) {
            // Waveform selection (1-4)
            case '1':
                params->waveform = static_cast<int>(Waveform::SINE);
                break;
            case '2':
                params->waveform = static_cast<int>(Waveform::SQUARE);
                break;
            case '3':
                params->waveform = static_cast<int>(Waveform::SAWTOOTH);
                break;
            case '4':
                params->waveform = static_cast<int>(Waveform::TRIANGLE);
                break;
                
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
            case KEY_UP:
            case '+':
            case '=':
                params->masterVolume = std::min(1.0f, params->masterVolume.load() + smallStep);
                break;
            case KEY_DOWN:
            case '-':
            case '_':
                params->masterVolume = std::max(0.0f, params->masterVolume.load() - smallStep);
                break;
        }
    } else if (currentPage == UIPage::REVERB) {
        switch (ch) {
            // Enable/disable reverb
            case ' ':  // Spacebar
                params->reverbEnabled = !params->reverbEnabled.load();
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
            // Enable/disable filter
            case ' ':  // Spacebar
                params->filterEnabled = !params->filterEnabled.load();
                break;
                
            // Filter type selection
            case '1':
                params->filterType = 0;  // Lowpass
                break;
            case '2':
                params->filterType = 1;  // Highpass
                break;
            case '3':
                params->filterType = 2;  // High shelf
                break;
            case '4':
                params->filterType = 3;  // Low shelf
                break;
                
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
    
    // Info tab
    if (currentPage == UIPage::INFO) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 22, " INFO ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 22, " INFO ");
        attroff(COLOR_PAIR(6));
    }
    
    // Fill rest of line
    for (int i = 28; i < cols; ++i) {
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
    
    // Waveform section
    attron(A_BOLD);
    mvprintw(row++, 1, "WAVEFORM");
    attroff(A_BOLD);
    row++;
    
    Waveform currentWaveform = static_cast<Waveform>(params->waveform.load());
    
    mvprintw(row, 2, "1:Sine  2:Square  3:Sawtooth  4:Triangle");
    row++;
    mvprintw(row++, 2, "Current: ");
    attron(COLOR_PAIR(2) | A_BOLD);
    addstr(Oscillator::getWaveformName(currentWaveform));
    attroff(COLOR_PAIR(2) | A_BOLD);
    
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
    
    mvprintw(row++, 2, "Active: ");
    
    int voiceX = 10;
    if (activeVoices >= 7) {
        attron(COLOR_PAIR(4) | A_BOLD);
    } else if (activeVoices >= 4) {
        attron(COLOR_PAIR(3));
    } else {
        attron(COLOR_PAIR(2));
    }
    
    mvprintw(row - 1, voiceX, "%d/8", activeVoices);
    attroff(COLOR_PAIR(4) | A_BOLD);
    attroff(COLOR_PAIR(3));
    attroff(COLOR_PAIR(2));
    
    mvprintw(row++, 2, "Usage:  [");
    attron(COLOR_PAIR(2));
    for (int i = 0; i < 8; ++i) {
        if (i < activeVoices) {
            addch('*');
        } else {
            addch('-');
        }
    }
    attroff(COLOR_PAIR(2));
    addch(']');
}

void UI::drawReverbPage() {
    int row = 3;
    
    // Reverb status
    attron(A_BOLD);
    mvprintw(row++, 1, "REVERB STATUS");
    attroff(A_BOLD);
    row++;
    
    mvprintw(row, 2, "Enabled: ");
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
    
    // Reverb parameters
    attron(A_BOLD);
    mvprintw(row++, 1, "PARAMETERS");
    attroff(A_BOLD);
    row++;
    
    // Greyhole parameters with actual DSP ranges
    // DelayTime and Size both map from reverbSize (0-1) per setSize() in reverb.cpp
    float delayTime = 0.001f + params->reverbSize.load() * 1.449f;
    float size = 0.5f + params->reverbSize.load() * 2.5f;
    drawBar(row++, 2, "DelayTime (Z/z)", delayTime, 0.001f, 1.45f, 20);
    drawBar(row++, 2, "Damping   (X/x)", params->reverbDamping.load(), 0.0f, 0.99f, 20);
    drawBar(row++, 2, "Size      (Z/z)", size, 0.5f, 3.0f, 20);
    drawBar(row++, 2, "Diffusion (B/b)", params->reverbDiffusion.load(), 0.0f, 0.99f, 20);
    drawBar(row++, 2, "Feedback  (V/v)", params->reverbDecay.load(), 0.0f, 1.0f, 20);
    
    // Our dry/wet mix control (not a Greyhole DSP parameter)
    drawBar(row++, 2, "Mix       (C/c)", params->reverbMix.load(), 0.0f, 1.0f, 20);
    
    row++;
    
    // Greyhole modulation parameters
    attron(COLOR_PAIR(3));
    mvprintw(row++, 2, "GREYHOLE CONTROLS");
    attroff(COLOR_PAIR(3));
    
    drawBar(row++, 2, "ModDepth  (N/n)", params->reverbModDepth.load(), 0.0f, 1.0f, 20);
    drawBar(row++, 2, "ModFreq   (M/m)", params->reverbModFreq.load(), 0.0f, 10.0f, 20);
    
    row += 2;
    
    // Parameter descriptions
    attron(A_BOLD);
    mvprintw(row++, 1, "PARAMETER GUIDE");
    attroff(A_BOLD);
    row++;
    
    mvprintw(row++, 2, "DelayTime: Greyhole delay time (0.001-1.45s)");
    mvprintw(row++, 2, "Damping:   High frequency absorption (0.0-0.99)");
    mvprintw(row++, 2, "Size:      Greyhole room size (0.5-3.0)");
    mvprintw(row++, 2, "Diffusion: Reverb density/smoothness (0.0-0.99)");
    mvprintw(row++, 2, "Feedback:  Reverb tail length (0.0-1.0)");
    mvprintw(row++, 2, "Mix:       Dry/wet balance (0.0-1.0)");
    mvprintw(row++, 2, "ModDepth:  Chorus effect intensity (0.0-1.0)");
    mvprintw(row++, 2, "ModFreq:   Chorus modulation speed (0.0-10.0 Hz)");
}

void UI::drawFilterPage() {
    int row = 3;
    
    // Filter status
    attron(A_BOLD);
    mvprintw(row++, 1, "FILTER STATUS");
    attroff(A_BOLD);
    row++;
    
    mvprintw(row, 2, "Enabled: ");
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
    
    // Filter type selection
    attron(A_BOLD);
    mvprintw(row++, 1, "FILTER TYPE");
    attroff(A_BOLD);
    row++;
    
    mvprintw(row++, 2, "1:Lowpass  2:Highpass  3:HighShelf  4:LowShelf");
    
    const char* filterNames[] = {"Lowpass", "Highpass", "High Shelf", "Low Shelf"};
    int filterType = params->filterType.load();
    mvprintw(row++, 2, "Current: ");
    attron(COLOR_PAIR(2) | A_BOLD);
    addstr(filterNames[filterType]);
    attroff(COLOR_PAIR(2) | A_BOLD);
    
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

void UI::drawInfoPage() {
    int row = 3;
    
    // System information
    attron(A_BOLD);
    mvprintw(row++, 1, "SYSTEM INFORMATION");
    attroff(A_BOLD);
    row++;
    
    // Audio device info
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(row++, 2, "AUDIO DEVICE");
    attroff(COLOR_PAIR(2) | A_BOLD);
    row++;
    
    mvprintw(row++, 4, "Device:      %s", audioDeviceName.c_str());
    mvprintw(row++, 4, "Sample Rate: %d Hz", audioSampleRate);
    mvprintw(row++, 4, "Buffer Size: %d samples", audioBufferSize);
    
    if (audioSampleRate > 0 && audioBufferSize > 0) {
        float latency = (audioBufferSize * 1000.0f) / audioSampleRate;
        mvprintw(row++, 4, "Latency:     %.2f ms", latency);
    }
    
    row += 2;
    
    // MIDI device info
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(row++, 2, "MIDI DEVICE");
    attroff(COLOR_PAIR(2) | A_BOLD);
    row++;
    
    mvprintw(row++, 4, "Device:      %s", midiDeviceName.c_str());
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
    
    // Reverb engine info
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(row++, 2, "DSP ENGINE");
    attroff(COLOR_PAIR(2) | A_BOLD);
    row++;
    
    mvprintw(row++, 4, "Reverb:      Greyhole (Faust 2.37.3)");
    mvprintw(row++, 4, "Algorithm:   Nested Diffusion Network");
    mvprintw(row++, 4, "Delay Taps:  1302 prime numbers");
    mvprintw(row++, 4, "Modulation:  LFO-based chorus effect");
    
    row += 2;
    
    // Build info
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(row++, 2, "BUILD INFO");
    attroff(COLOR_PAIR(2) | A_BOLD);
    row++;
    
    mvprintw(row++, 4, "Max Voices:  8 (polyphonic)");
    mvprintw(row++, 4, "Waveforms:   Sine, Square, Sawtooth, Triangle");
    mvprintw(row++, 4, "Filters:     Lowpass, Highpass, Shelving");
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
    
    // Draw hotkey line at bottom
    row = maxY - 1;
    attron(COLOR_PAIR(1));
    mvhline(row - 1, 0, '-', maxX);
    attroff(COLOR_PAIR(1));
    mvprintw(row, 1, "Tab/Arrows Navigate  |  Q Quit");
}

void UI::draw(int activeVoices) {
    erase();  // Use erase() instead of clear() - doesn't cause flicker
    
    drawTabs();
    
    switch (currentPage) {
        case UIPage::MAIN:
            drawMainPage(activeVoices);
            break;
        case UIPage::REVERB:
            drawReverbPage();
            break;
        case UIPage::FILTER:
            drawFilterPage();
            break;
        case UIPage::INFO:
            drawInfoPage();
            break;
    }
    
    drawConsole();
    
    refresh();
}