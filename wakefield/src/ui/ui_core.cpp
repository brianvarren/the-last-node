#include "../ui.h"
#include "../synth.h"
#include "../preset.h"
#include "../theme.h"
#include <limits>
#include <algorithm>
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
    , bufferSizeChangeRequested(false)
    , requestedBufferSize(-1)
    , bufferSizeOptions{64, 128, 256, 512, 1024}
    , helpActive(false)
    , helpScrollOffset(0)
    , currentOscillatorIndex(0)
    , currentSamplerIndex(0)
    , currentLFOIndex(0)
    , currentEnvelopeIndex(0)
    , currentChaosIndex(0)
    , fmMatrixCursorRow(0)
    , fmMatrixCursorCol(1)  // Start at 0→1 (first valid FM routing)
    , fmFocusArea(0)
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
    , sampleBrowserCurrentDir("samples")
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
    , hasOriginalTermios(false)
    , statusMessage()
    , statusMessageExpiry() {

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

    resetUndoHistory();
    resetAudioDebugStats();

    // Default: lock all FM matrix cells (immune to randomize/mutate until explicitly unlocked)
    for (int t = 0; t < kFMTargetCount; ++t) {
        for (int s = 0; s < kFMSourceCount; ++s) {
            fmMatrixLocked[t][s] = true;
        }
    }

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

void UI::setSampleDirectory(const std::string& path) {
    if (!path.empty()) {
        sampleBrowserCurrentDir = path;
    }
}

void UI::updateAudioDebugStats(uint32_t durationMicros,
                               float peak,
                               uint32_t activeVoices,
                               uint32_t intervalMicros,
                               int schedulerPolicy,
                               int schedulerPriority,
                               int callbackCpu) {
    audioDebugState.currentMicros.store(durationMicros, std::memory_order_relaxed);
    audioDebugState.currentPeak.store(peak, std::memory_order_relaxed);
    audioDebugState.currentVoices.store(activeVoices, std::memory_order_relaxed);
    audioDebugState.totalMicros.fetch_add(durationMicros, std::memory_order_relaxed);
    audioDebugState.callbackCount.fetch_add(1, std::memory_order_relaxed);

    // Update min micros
    uint32_t minMicros = audioDebugState.minMicros.load(std::memory_order_relaxed);
    while (durationMicros < minMicros &&
           !audioDebugState.minMicros.compare_exchange_weak(minMicros, durationMicros,
                                                           std::memory_order_relaxed)) {
        // minMicros updated with current value by compare_exchange
    }

    // Update max micros
    uint32_t maxMicros = audioDebugState.maxMicros.load(std::memory_order_relaxed);
    while (durationMicros > maxMicros &&
           !audioDebugState.maxMicros.compare_exchange_weak(maxMicros, durationMicros,
                                                           std::memory_order_relaxed)) {
    }

    // Update max peak
    float maxPeak = audioDebugState.maxPeak.load(std::memory_order_relaxed);
    while (peak > maxPeak &&
           !audioDebugState.maxPeak.compare_exchange_weak(maxPeak, peak,
                                                          std::memory_order_relaxed)) {
    }

    // Update max voices
    uint32_t maxVoices = audioDebugState.maxVoices.load(std::memory_order_relaxed);
    while (activeVoices > maxVoices &&
           !audioDebugState.maxVoices.compare_exchange_weak(maxVoices, activeVoices,
                                                            std::memory_order_relaxed)) {
    }

    if (intervalMicros > 0) {
        audioDebugState.currentIntervalMicros.store(intervalMicros, std::memory_order_relaxed);
        audioDebugState.totalIntervalMicros.fetch_add(intervalMicros, std::memory_order_relaxed);

        uint32_t minInterval = audioDebugState.minIntervalMicros.load(std::memory_order_relaxed);
        while (intervalMicros < minInterval &&
               !audioDebugState.minIntervalMicros.compare_exchange_weak(minInterval, intervalMicros,
                                                                       std::memory_order_relaxed)) {
        }

        uint32_t maxInterval = audioDebugState.maxIntervalMicros.load(std::memory_order_relaxed);
        while (intervalMicros > maxInterval &&
               !audioDebugState.maxIntervalMicros.compare_exchange_weak(maxInterval, intervalMicros,
                                                                        std::memory_order_relaxed)) {
        }
    }

    if (schedulerPolicy >= 0) {
        audioDebugState.schedulerPolicy.store(schedulerPolicy, std::memory_order_relaxed);
    }
    if (schedulerPriority >= 0) {
        audioDebugState.schedulerPriority.store(schedulerPriority, std::memory_order_relaxed);
    }
    if (callbackCpu >= 0) {
        audioDebugState.callbackCpu.store(callbackCpu, std::memory_order_relaxed);
    }
}

void UI::resetAudioDebugStats() {
    audioDebugState.currentMicros.store(0, std::memory_order_relaxed);
    audioDebugState.minMicros.store(std::numeric_limits<uint32_t>::max(), std::memory_order_relaxed);
    audioDebugState.maxMicros.store(0, std::memory_order_relaxed);
    audioDebugState.currentPeak.store(0.0f, std::memory_order_relaxed);
    audioDebugState.maxPeak.store(0.0f, std::memory_order_relaxed);
    audioDebugState.currentVoices.store(0, std::memory_order_relaxed);
    audioDebugState.maxVoices.store(0, std::memory_order_relaxed);
    audioDebugState.callbackCount.store(0, std::memory_order_relaxed);
    audioDebugState.totalMicros.store(0, std::memory_order_relaxed);
    audioDebugState.currentIntervalMicros.store(0, std::memory_order_relaxed);
    audioDebugState.minIntervalMicros.store(std::numeric_limits<uint32_t>::max(), std::memory_order_relaxed);
    audioDebugState.maxIntervalMicros.store(0, std::memory_order_relaxed);
    audioDebugState.totalIntervalMicros.store(0, std::memory_order_relaxed);
    audioDebugState.schedulerPolicy.store(-1, std::memory_order_relaxed);
    audioDebugState.schedulerPriority.store(-1, std::memory_order_relaxed);
    audioDebugState.callbackCpu.store(-1, std::memory_order_relaxed);
}

void UI::requestAudioBufferSizeChange(int newSize) {
    if (newSize <= 0) {
        return;
    }
    requestedBufferSize = newSize;
    bufferSizeChangeRequested = true;
}

void UI::cycleAudioBufferSize(int direction) {
    if (direction == 0 || bufferSizeOptions.empty()) {
        return;
    }

    auto ensureOption = [&](int size) {
        if (size <= 0) {
            return;
        }
        if (std::find(bufferSizeOptions.begin(), bufferSizeOptions.end(), size) == bufferSizeOptions.end()) {
            bufferSizeOptions.push_back(size);
            std::sort(bufferSizeOptions.begin(), bufferSizeOptions.end());
            bufferSizeOptions.erase(std::unique(bufferSizeOptions.begin(), bufferSizeOptions.end()),
                                    bufferSizeOptions.end());
        }
    };

    if (audioBufferSize > 0) {
        ensureOption(audioBufferSize);
    }

    auto it = std::find(bufferSizeOptions.begin(), bufferSizeOptions.end(), audioBufferSize);
    int idx = 0;
    if (it != bufferSizeOptions.end()) {
        idx = static_cast<int>(std::distance(bufferSizeOptions.begin(), it));
    }

    int optionCount = static_cast<int>(bufferSizeOptions.size());
    int newIdx = (idx + direction) % optionCount;
    if (newIdx < 0) {
        newIdx += optionCount;
    }

    int newSize = bufferSizeOptions[newIdx];
    if (newSize == audioBufferSize) {
        addConsoleMessage("Audio buffer already " + std::to_string(newSize) + " samples");
        return;
    }

    requestAudioBufferSizeChange(newSize);
    addConsoleMessage("Switching audio buffer to " + std::to_string(newSize) + " samples...");
}

void UI::addConsoleMessage(const std::string& message) {
    statusMessage = message;
    statusMessageExpiry = std::chrono::steady_clock::now() + std::chrono::seconds(3);
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
