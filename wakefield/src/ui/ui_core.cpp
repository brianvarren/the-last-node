#include "../ui.h"
#include "../synth.h"
#include "../preset.h"
#include <chrono>

UI::UI(Synth* synth, SynthParameters* params)
    : synth(synth)
    , params(params)
    , initialized(false)
    , currentPage(UIPage::OSCILLATOR)
    , audioDeviceName("Unknown")
    , audioSampleRate(0)
    , audioBufferSize(0)
    , midiDeviceName("Not connected")
    , midiPortNum(-1)
    , currentAudioDeviceId(-1)
    , currentMidiPortNum(-1)
    , selectedParameterId(0)
    , numericInputActive(false)
    , numericInputIsMod(false)
    , currentPresetName("None")
    , textInputActive(false)
    , deviceChangeRequested(false)
    , requestedAudioDeviceId(-1)
    , requestedMidiPortNum(-1)
    , helpActive(false)
    , helpScrollOffset(0)
    , currentOscillatorIndex(0)
    , currentSamplerIndex(0)
    , currentLFOIndex(0)
    , currentEnvelopeIndex(0)
    , fmMatrixCursorRow(0)
    , fmMatrixCursorCol(1)  // Start at 0→1 (first valid FM routing)
    , modMatrixCursorRow(0)
    , modMatrixCursorCol(0)
    , waveformBuffer(WAVEFORM_BUFFER_SIZE, 0.0f)
    , waveformBufferWritePos(0)
    , sequencerSelectedRow(0)
    , sequencerSelectedColumn(static_cast<int>(SequencerTrackerColumn::NOTE))
    , sequencerFocusRightPane(false)
    , sequencerRightSelection(0)
    , sequencerMutateAmount(20.0f)
    , sequencerFocusActionsPane(false)
    , sequencerActionsRow(0)
    , sequencerActionsColumn(0)
    , sequencerScaleMenuActive(false)
    , sequencerScaleMenuIndex(0)
    , numericInputIsSequencer(false)
    , modMatrixMenuActive(false)
    , modMatrixMenuIndex(0)
    , modMatrixMenuColumn(0)
    , sampleBrowserActive(false)
    , sampleBrowserCurrentDir("samples")
    , sampleBrowserSelectedIndex(0)
    , sampleBrowserScrollOffset(0) {

    // Initialize LFO history buffers
    for (int i = 0; i < 4; ++i) {
        lfoHistoryBuffer[i].resize(LFO_HISTORY_SIZE, 0.0f);
        lfoHistoryWritePos[i] = 0;
    }

    // Load available presets
    refreshPresetList();

    // Initialize parameter definitions
    initializeParameters();

    // Set initial selected parameter to first parameter on main page
    std::vector<int> initialParams = getParameterIdsForPage(UIPage::OSCILLATOR);
    if (!initialParams.empty()) {
        selectedParameterId = initialParams[0];  // Start with first parameter
    }

    // Initialize default modulation routing: ENV 1 → OSC 1-4 Amp (slots 0-3)
    // This provides amplitude envelope control separate from mix levels
    for (int i = 0; i < 4; ++i) {
        modulationSlots[i].source = 4;           // ENV 1 (index 4)
        modulationSlots[i].curve = 0;            // Linear curve
        modulationSlots[i].amount = 100;         // 100% modulation
        modulationSlots[i].destination = i * 6 + 5;  // OSC (i+1) Amp (indices 5, 11, 17, 23)
        modulationSlots[i].type = 0;             // Unidirectional
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

    // Start CPU monitoring
    cpuMonitor.start();

    initialized = true;
    return true;
}

bool UI::update() {
    // Drain all pending input to prevent buildup when keys are held down
    // This ensures that releasing arrow keys stops parameter adjustment immediately
    int ch;
    int lastValidKey = ERR;

    while ((ch = getch()) != ERR) {
        lastValidKey = ch;
    }

    // Process only the most recent key if any were detected
    if (lastValidKey != ERR) {
        handleInput(lastValidKey);

        if (lastValidKey == 'q' || lastValidKey == 'Q') {
            return false;
        }
    }

    // Check for MIDI learn timeout (10 seconds)
    if (params->midiLearnActive.load()) {
        auto now = std::chrono::steady_clock::now();
        double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();
        double startTime = params->midiLearnStartTime.load();

        if (currentTime - startTime > 10.0) {
            // Timeout - cancel MIDI learn
            finishMidiLearn();
            addConsoleMessage("MIDI Learn timeout - cancelled");
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

void UI::writeToWaveformBuffer(float sample) {
    int pos = waveformBufferWritePos.load(std::memory_order_relaxed);
    waveformBuffer[pos] = sample;
    waveformBufferWritePos.store((pos + 1) % WAVEFORM_BUFFER_SIZE, std::memory_order_relaxed);
}

void UI::writeToLFOHistory(int lfoIndex, float amplitude) {
    if (lfoIndex < 0 || lfoIndex >= 4) return;
    if (lfoHistoryBuffer[lfoIndex].empty()) return;  // Safety check during shutdown

    int pos = lfoHistoryWritePos[lfoIndex];
    lfoHistoryBuffer[lfoIndex][pos] = amplitude;
    lfoHistoryWritePos[lfoIndex] = (pos + 1) % LFO_HISTORY_SIZE;
}
