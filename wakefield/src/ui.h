#ifndef UI_H
#define UI_H

#include <ncursesw/curses.h>
#include <string>
#include <atomic>
#include <deque>
#include <mutex>
#include <vector>
#include <functional>
#include "oscillator.h"

class Synth;  // Forward declaration

// Filter types
enum class FilterType {
    LOWPASS = 0,
    HIGHPASS = 1,
    HIGHSHELF = 2,
    LOWSHELF = 3
};

// Reverb types
enum class ReverbType {
    GREYHOLE = 0,
    PLATE = 1,
    ROOM = 2,
    HALL = 3,
    SPRING = 4
};

// Parameter types for inline editing
enum class ParamType {
    FLOAT,
    INT, 
    ENUM,
    BOOL
};

// Inline parameter definition
struct InlineParameter {
    int id;
    ParamType type;
    std::string name;
    std::string unit;
    float min_val;
    float max_val;
    std::vector<std::string> enum_values; // For enum type
    bool supports_midi_learn;
    int page; // Which UIPage this parameter belongs to
};

// Parameters that can be controlled via UI
struct SynthParameters {
    std::atomic<float> attack{0.01f};
    std::atomic<float> decay{0.1f};
    std::atomic<float> sustain{0.7f};
    std::atomic<float> release{0.2f};
    std::atomic<float> masterVolume{0.5f};
    std::atomic<int> waveform{static_cast<int>(Waveform::SINE)};
    
    // Reverb parameters
    std::atomic<int> reverbType{static_cast<int>(ReverbType::GREYHOLE)};
    std::atomic<float> reverbDelayTime{0.5f};  // 0-1, maps to 0.001-1.45s
    std::atomic<float> reverbSize{0.5f};       // 0-1, maps to 0.5-3.0
    std::atomic<float> reverbDamping{0.5f};
    std::atomic<float> reverbMix{0.3f};
    std::atomic<float> reverbDecay{0.5f};
    std::atomic<bool> reverbEnabled{true};     // Always enabled now
    
    // Greyhole-specific parameters
    std::atomic<float> reverbDiffusion{0.5f};
    std::atomic<float> reverbModDepth{0.1f};
    std::atomic<float> reverbModFreq{2.0f};
    
    // Filter parameters
    std::atomic<bool> filterEnabled{false};
    std::atomic<int> filterType{0};  // 0=LP, 1=HP, 2=HighShelf, 3=LowShelf
    std::atomic<float> filterCutoff{1000.0f};
    std::atomic<float> filterGain{0.0f};  // For shelf filters (dB)
    
    // Generic MIDI CC Learn for new parameter system
    std::atomic<bool> midiLearnActive{false};
    std::atomic<int> midiLearnParameterId{-1};  // Which parameter ID to learn (-1 = none)
    std::atomic<double> midiLearnStartTime{0.0};  // Timestamp when MIDI learn started

    // MIDI CC mappings for all parameters (parameter ID -> CC number, -1 means not mapped)
    std::atomic<int> parameterCCMap[50];  // Support up to 50 parameters

    // Legacy - keep for backwards compatibility
    std::atomic<int> filterCutoffCC{-1};  // Which CC controls filter cutoff (-1 = none)
    
    // Legacy MIDI learn for compatibility
    std::atomic<bool> ccLearnMode{false};
    std::atomic<int> ccLearnTarget{-1};  // Which parameter to learn (-1 = none)
    
    // Looper parameters
    std::atomic<int> currentLoop{0};       // 0-3
    std::atomic<float> overdubMix{0.6f};   // global overdub wet amount
    
    // MIDI Learn for loop controls
    std::atomic<int> loopRecPlayCC{-1};
    std::atomic<int> loopOverdubCC{-1};
    std::atomic<int> loopStopCC{-1};
    std::atomic<int> loopClearCC{-1};
    std::atomic<bool> loopMidiLearnMode{false};
    std::atomic<int> loopMidiLearnTarget{-1};  // 0=rec, 1=overdub, 2=stop, 3=clear
    
    // Oscillator parameters - 4 independent oscillators per voice
    // Oscillator 1 (index 0)
    std::atomic<int> osc1Mode{0};              // 0=FREE, 1=KEY
    std::atomic<float> osc1Freq{440.0f};       // Base frequency (20-2000 Hz)
    std::atomic<float> osc1Morph{0.5f};        // Waveform morph (0.0001-0.9999)
    std::atomic<float> osc1Duty{0.5f};         // Pulse width (0.0-1.0)
    std::atomic<float> osc1Ratio{1.0f};        // FM8-style frequency ratio (0.125-16.0)
    std::atomic<float> osc1Offset{0.0f};       // FM8-style frequency offset Hz (-1000-1000)
    std::atomic<float> osc1VelocityWeight{0.0f}; // Velocity modulation amount (0.0-1.0)
    std::atomic<bool> osc1Flip{false};         // Polarity inversion
    std::atomic<float> osc1Level{1.0f};        // Oscillator level/gain (0.0-1.0)

    // Oscillator 2 (index 1)
    std::atomic<int> osc2Mode{0};
    std::atomic<float> osc2Freq{440.0f};
    std::atomic<float> osc2Morph{0.5f};
    std::atomic<float> osc2Duty{0.5f};
    std::atomic<float> osc2Ratio{1.0f};
    std::atomic<float> osc2Offset{0.0f};
    std::atomic<float> osc2VelocityWeight{0.0f};
    std::atomic<bool> osc2Flip{false};
    std::atomic<float> osc2Level{0.0f};        // Default: off

    // Oscillator 3 (index 2)
    std::atomic<int> osc3Mode{0};
    std::atomic<float> osc3Freq{440.0f};
    std::atomic<float> osc3Morph{0.5f};
    std::atomic<float> osc3Duty{0.5f};
    std::atomic<float> osc3Ratio{1.0f};
    std::atomic<float> osc3Offset{0.0f};
    std::atomic<float> osc3VelocityWeight{0.0f};
    std::atomic<bool> osc3Flip{false};
    std::atomic<float> osc3Level{0.0f};        // Default: off

    // Oscillator 4 (index 3)
    std::atomic<int> osc4Mode{0};
    std::atomic<float> osc4Freq{440.0f};
    std::atomic<float> osc4Morph{0.5f};
    std::atomic<float> osc4Duty{0.5f};
    std::atomic<float> osc4Ratio{1.0f};
    std::atomic<float> osc4Offset{0.0f};
    std::atomic<float> osc4VelocityWeight{0.0f};
    std::atomic<bool> osc4Flip{false};
    std::atomic<float> osc4Level{0.0f};        // Default: off

    // Constructor to initialize CC map
    SynthParameters() {
        for (int i = 0; i < 50; ++i) {
            parameterCCMap[i] = -1;  // -1 means no CC assigned
        }
    }
};

enum class UIPage {
    MAIN,
    OSCILLATOR,
    LFO,
    REVERB,
    FILTER,
    LOOPER,
    SEQUENCER,
    CONFIG
};

class UI {
public:
    UI(Synth* synth, SynthParameters* params);
    ~UI();
    
    // Initialize ncurses
    bool initialize();
    
    // Main UI loop (call this periodically)
    // Returns false if user wants to quit
    bool update();
    
    // Draw the UI
    void draw(int activeVoices);

    // Sequencer info field identifiers (exposed for shared lookup tables)
    enum class SequencerInfoField {
        TEMPO = 0,
        ROOT,
        SCALE,
        EUCLID_HITS,
        EUCLID_STEPS,
        SUBDIVISION,
        MUTATE_AMOUNT,
        MUTED,
        SOLO,
        ACTIVE_COUNT,
        LOCKED_COUNT
    };
    
    
    // Set device info
    void setDeviceInfo(const std::string& audioDevice, int sampleRate, int bufferSize,
                       const std::string& midiDevice, int midiPort);
    
    // Set available devices
    void setAvailableAudioDevices(const std::vector<std::pair<int, std::string>>& devices, int currentDeviceId);
    void setAvailableMidiDevices(const std::vector<std::pair<int, std::string>>& devices, int currentPort);
    
    // Add message to console
    void addConsoleMessage(const std::string& message);

    // Get parameter name by ID (public for MIDI handler)
    std::string getParameterName(int id);
    
    // Waveform buffer for oscilloscope
    void writeToWaveformBuffer(float sample);

    // Preset management
    void loadPreset(const std::string& filename);
    void savePreset(const std::string& filename);
    
    // Device change request (returns true if restart requested)
    bool isDeviceChangeRequested() const { return deviceChangeRequested; }
    int getRequestedAudioDevice() const { return requestedAudioDeviceId; }
    int getRequestedMidiDevice() const { return requestedMidiPortNum; }
    void clearDeviceChangeRequest() { deviceChangeRequested = false; }
    
private:
    Synth* synth;
    SynthParameters* params;
    bool initialized;
    UIPage currentPage;
    
    // Device information
    std::string audioDeviceName;
    int audioSampleRate;
    int audioBufferSize;
    std::string midiDeviceName;
    int midiPortNum;
    int currentAudioDeviceId;
    int currentMidiPortNum;
    
    // Available devices
    std::vector<std::pair<int, std::string>> availableAudioDevices;  // id, name
    std::vector<std::pair<int, std::string>> availableMidiDevices;   // port, name
    
    // Console message system
    std::deque<std::string> consoleMessages;
    std::mutex consoleMutex;
    static const int MAX_CONSOLE_MESSAGES = 5;
    
    // Parameter navigation state  
    int selectedParameterId;
    bool numericInputActive;
    std::string numericInputBuffer;
    
    // Preset management
    std::string currentPresetName;
    std::vector<std::string> availablePresets;
    bool textInputActive;
    std::string textInputBuffer;
    
    static const int WAVEFORM_BUFFER_SIZE = 8192;
    std::vector<float> waveformBuffer;
    std::atomic<int> waveformBufferWritePos;
    
    // Device change request
    bool deviceChangeRequested;
    int requestedAudioDeviceId;
    int requestedMidiPortNum;

    // Help system
    bool helpActive;
    int helpScrollOffset;
    void showHelp();
    void hideHelp();
    void drawHelpPage();
    std::string getHelpContent(UIPage page);

    // Text input for preset names
    void startTextInput();
    void handleTextInput(int ch);
    void finishTextInput();
    
    // Handle keyboard input
    void handleInput(int ch);
    
    // Draw helper functions
    void drawTabs();
    void drawMainPage(int activeVoices);
    void drawParametersPage();  // Generic parameter page drawing
    void drawOscillatorPage();
    void drawLFOPage();
    void drawReverbPage();
    void drawFilterPage();
    void drawLooperPage();
    void drawSequencerPage();
    void drawConfigPage();
    void drawBar(int y, int x, const char* label, float value, float min, float max, int width);
    void drawConsole();
    void drawHotkeyLine();

    // Sequencer helpers
    bool handleSequencerInput(int ch);
    void adjustSequencerTrackerField(int row, int column, int direction);
    void adjustSequencerInfoField(int infoIndex, int direction);
    void executeSequencerAction(int actionRow, int actionColumn);
    void startSequencerNumericInput(int row, int column);
    void startSequencerInfoNumericInput(int infoIndex);
    void applySequencerNumericInput(const std::string& text);
    void cancelSequencerNumericInput();
    void startSequencerScaleMenu();
    void handleSequencerScaleMenuInput(int ch);
    void finishSequencerScaleMenu(bool applySelection);
    void ensureSequencerSelectionInRange();
    
    // Preset management helpers
    void refreshPresetList();
    
    // Parameter management
    std::vector<InlineParameter> parameters;
    void initializeParameters();
    InlineParameter* getParameter(int id);
    std::vector<int> getParameterIdsForPage(UIPage page);
    void adjustParameter(int id, bool increase);
    void setParameterValue(int id, float value);
    float getParameterValue(int id);
    std::string getParameterDisplayString(int id);
    void startNumericInput(int id);
    void finishNumericInput();
    void startMidiLearn(int id);
    void finishMidiLearn();

    // Oscillator/LFO UI state
    int currentOscillatorIndex;  // 0-3: which oscillator is selected on OSCILLATOR page
    int currentLFOIndex;          // 0-3: which LFO is selected on LFO page

    // Sequencer UI state
    enum class SequencerTrackerColumn {
        LOCK = 0,
        NOTE = 1,
        VELOCITY = 2,
        GATE = 3,
        PROBABILITY = 4
    };

    enum class SequencerNumericField {
        NONE = 0,
        NOTE,
        VELOCITY,
        GATE,
        PROBABILITY,
        TEMPO,
        SCALE,
        ROOT,
        EUCLID_HITS,
        EUCLID_STEPS,
        EUCLID_ROTATION,
        SUBDIVISION,
        MUTATE_AMOUNT,
        MUTED,
        SOLO
    };

    struct SequencerNumericContext {
        SequencerNumericField field;
        int row;      // For tracker editing (row index)
        int column;   // For tracker editing (column index)
        int infoIndex; // For right pane editing
        SequencerNumericContext()
            : field(SequencerNumericField::NONE)
            , row(-1)
            , column(-1)
            , infoIndex(-1) {}
    };

    int sequencerSelectedRow;
    int sequencerSelectedColumn;
    bool sequencerFocusRightPane;
    int sequencerRightSelection;
    float sequencerMutateAmount;  // Percentage 0-100

    // Actions section state
    bool sequencerFocusActionsPane;
    int sequencerActionsRow;     // 0=Generate, 1=Clear, 2=Randomize, 3=Mutate, 4=Rotate
    int sequencerActionsColumn;  // 0=All, 1=Note, 2=Vel, 3=Gate, 4=Prob

    // Scale selection menu
    bool sequencerScaleMenuActive;
    int sequencerScaleMenuIndex;

    bool numericInputIsSequencer;
    SequencerNumericContext sequencerNumericContext;
};

#endif // UI_H
