#ifndef UI_H
#define UI_H

#include <ncurses.h>
#include <string>
#include <atomic>
#include <deque>
#include <mutex>
#include <vector>
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
    
    // Brainwave oscillator parameters
    std::atomic<int> brainwaveMode{0};         // 0=FREE, 1=KEY
    std::atomic<float> brainwaveFreq{440.0f};  // Base frequency or offset (20-2000 Hz)
    std::atomic<float> brainwaveMorph{0.5f};   // 0.0001-0.9999, maps to frames 0-255
    std::atomic<float> brainwaveDuty{0.5f};    // 0.0-1.0, pulse width
    std::atomic<int> brainwaveOctave{0};       // -3 to +3 octave offset (0 = no shift)
    std::atomic<bool> brainwaveLFOEnabled{false};
    std::atomic<int> brainwaveLFOSpeed{0};     // 0-9 index
};

enum class UIPage {
    MAIN,
    BRAINWAVE,
    REVERB,
    FILTER,
    LOOPER,
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
    
    
    // Set device info
    void setDeviceInfo(const std::string& audioDevice, int sampleRate, int bufferSize,
                       const std::string& midiDevice, int midiPort);
    
    // Set available devices
    void setAvailableAudioDevices(const std::vector<std::pair<int, std::string>>& devices, int currentDeviceId);
    void setAvailableMidiDevices(const std::vector<std::pair<int, std::string>>& devices, int currentPort);
    
    // Add message to console
    void addConsoleMessage(const std::string& message);
    
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
    void drawBrainwavePage();
    void drawReverbPage();
    void drawFilterPage();
    void drawLooperPage();
    void drawConfigPage();
    void drawBar(int y, int x, const char* label, float value, float min, float max, int width);
    void drawConsole();
    void drawHotkeyLine();
    
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
};

#endif // UI_H