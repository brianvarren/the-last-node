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

// Menu types for popup menus
enum class MenuType {
    NONE,
    WAVEFORM,
    FILTER_TYPE,
    REVERB_TYPE,
    PRESET,
    AUDIO_DEVICE,
    MIDI_DEVICE
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
    
    // MIDI CC Learn
    std::atomic<bool> ccLearnMode{false};
    std::atomic<int> ccLearnTarget{-1};  // Which parameter to learn (-1 = none)
    std::atomic<int> filterCutoffCC{-1};  // Which CC controls filter cutoff (-1 = none)
    
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
    std::atomic<float> brainwaveMorph{0.5f};   // 0.0-1.0, maps to frames 0-255
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
    CONFIG,
    TEST
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
    
    // Update test oscillator (call before draw)
    void updateTestOscillator(float deltaTime);
    
    // Update brainwave oscilloscope (call before draw)
    void updateBrainwaveOscilloscope(float deltaTime);
    
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
    
    // Menu navigation state
    int selectedRow;
    bool menuPopupActive;
    int popupSelectedIndex;
    int popupItemCount;
    MenuType currentMenuType;
    
    // Preset management
    std::string currentPresetName;
    std::vector<std::string> availablePresets;
    bool textInputActive;
    std::string textInputBuffer;
    
    // Test oscilloscope state
    static const int SCOPE_WIDTH = 80;
    static const int SCOPE_HEIGHT = 20;
    static const int BRAIN_SCOPE_WIDTH = 64;
    static const int BRAIN_SCOPE_HEIGHT = 40;
    float testOscPhase;
    float testOscFreq;
    float scopeFadeTime;
    float scopeBuffer[80][20];  // intensity buffer for fade effect
    
    // Brainwave oscilloscope
    float scopeBuffer2[BRAIN_SCOPE_WIDTH][BRAIN_SCOPE_HEIGHT];  // second scope buffer for brainwave page
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
    void drawBrainwavePage();
    void drawReverbPage();
    void drawFilterPage();
    void drawLooperPage();
    void drawConfigPage();
    void drawTestPage();
    void drawBar(int y, int x, const char* label, float value, float min, float max, int width);
    void drawConsole();
    void drawHotkeyLine();
    
    // Popup menu helpers
    void drawPopupMenu(const std::vector<std::string>& items, const std::string& title);
    void openPopupMenu(MenuType menuType);
    void closePopupMenu();
    void refreshPresetList();
    
    // Helper to get menu items for a given menu type
    std::vector<std::string> getMenuItems(MenuType menuType);
    
    // Helper to get selectable row count for current page
    int getSelectableRowCount();
};

#endif // UI_H