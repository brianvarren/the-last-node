#include "../ui.h"
#include "../synth.h"
#include "../preset.h"
#include "../theme.h"
#include <chrono>

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
    , numericInputIsMod(false)
    , currentPresetName("None")
    , textInputActive(false)
    , presetListIndex(0)
    , presetListScroll(0)
    , deviceChangeRequested(false)
    , requestedAudioDeviceId(-1)
    , requestedMidiPortNum(-1)
    , helpActive(false)
    , helpScrollOffset(0)
    , currentOscillatorIndex(0)
    , currentSamplerIndex(0)
    , currentLFOIndex(0)
    , currentEnvelopeIndex(0)
    , currentChaosIndex(0)
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
    , modMatrixDestinationModuleIndex(0)
    , modMatrixDestinationParamIndex(0)
    , modMatrixDestinationFocusColumn(1)
    , sampleBrowserActive(false)
    , sampleBrowserCurrentDir("../samples")
    , sampleBrowserSelectedIndex(0)
    , sampleBrowserScrollOffset(0)
    , mainPageActionIndex(0)
    , globalMutatePercentage(20.0f)
    , globalRandomizePercentage(50.0f)
    , mainPageFocusLeft(true)
    , mainPageMixerChannel(0)
    , presetBrowserActive(false)
    , presetBrowserSelectedIndex(0)
    , presetBrowserScrollOffset(0)
    , midiKeyboardMode(false)
    , midiKeyboardOctave(4)
    , hasOriginalTermios(false) {

    // Initialize LFO history buffers
    for (int i = 0; i < 4; ++i) {
        lfoHistoryBuffer[i].resize(LFO_HISTORY_SIZE, 0.0f);
        lfoHistoryWritePos[i] = 0;
    }

    // Load available presets
    refreshPresetList();
    if (!availablePresets.empty()) {
        presetListIndex = 0;
        presetListScroll = 0;
    }

    // Initialize parameter definitions
    initializeParameters();

    // Set initial selected parameter to first parameter on main page
    std::vector<int> initialParams = getParameterIdsForPage(UIPage::MAIN);
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
        modulationSlots[i].type = 1;             // Bidirectional (0-1 maps to full range)
    }

    // Initialize default modulation routing: ENV 1 → SAMP 1-4 Amp (slots 4-7)
    // This provides amplitude envelope control for samplers in KEY mode
    // Using bidirectional so envelope 0-1 maps to levelMod 0-1 (not 0.5-1.0)
    for (int i = 0; i < 4; ++i) {
        modulationSlots[4 + i].source = 4;       // ENV 1 (index 4)
        modulationSlots[4 + i].curve = 0;        // Linear curve
        modulationSlots[4 + i].amount = 100;     // 100% modulation
        modulationSlots[4 + i].destination = 32 + (i * 5);  // SAMP (i+1) Amp (indices 32, 37, 42, 47)
        modulationSlots[4 + i].type = 1;         // Bidirectional (gives full 0-1 range)
    }

    resetUndoHistory();

}

UI::~UI() {
    if (initialized) {
        noraw();
        endwin();
    }
    if (hasOriginalTermios) {
        tcsetattr(STDIN_FILENO, TCSANOW, &originalTermios);
    }
}

bool UI::initialize() {
    if (!hasOriginalTermios) {
        if (tcgetattr(STDIN_FILENO, &originalTermios) == 0) {
            hasOriginalTermios = true;
        } else {
            originalTermios = {};
        }
    }

    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    struct termios modifiedTermios;
    if (tcgetattr(STDIN_FILENO, &modifiedTermios) == 0) {
        modifiedTermios.c_cc[VSUSP] = _POSIX_VDISABLE;
#ifdef VDSUSP
        modifiedTermios.c_cc[VDSUSP] = _POSIX_VDISABLE;
#endif
        tcsetattr(STDIN_FILENO, TCSANOW, &modifiedTermios);
    }

    // Initialize global theme system
    if (!globalTheme) {
        globalTheme = new Theme();
        // Try to load theme from file, fallback to defaults if it fails
        std::string themePath = "themes/default.ini";
        if (!globalTheme->loadFromFile(themePath)) {
            // File not found or parse error - use built-in defaults
        }
        globalTheme->initializeColors();
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
        if (lastValidKey == 3) {  // Ctrl+C exits cleanly in raw mode
            return false;
        }
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
