#ifndef UI_H
#define UI_H

#include <ncurses.h>
#include <string>
#include <atomic>
#include <deque>
#include <mutex>
#include "oscillator.h"

class Synth;  // Forward declaration

// Filter types
enum class FilterType {
    LOWPASS = 0,
    HIGHPASS = 1,
    HIGHSHELF = 2,
    LOWSHELF = 3
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
    std::atomic<float> reverbSize{0.5f};
    std::atomic<float> reverbDamping{0.5f};
    std::atomic<float> reverbMix{0.3f};
    std::atomic<float> reverbDecay{0.5f};
    std::atomic<bool> reverbEnabled{false};
    
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
};

enum class UIPage {
    MAIN,
    REVERB,
    FILTER,
    INFO
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
    
    // Add message to console
    void addConsoleMessage(const std::string& message);
    
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
    
    // Console message system
    std::deque<std::string> consoleMessages;
    std::mutex consoleMutex;
    static const int MAX_CONSOLE_MESSAGES = 5;
    
    // Handle keyboard input
    void handleInput(int ch);
    
    // Draw helper functions
    void drawTabs();
    void drawMainPage(int activeVoices);
    void drawReverbPage();
    void drawFilterPage();
    void drawInfoPage();
    void drawBar(int y, int x, const char* label, float value, float min, float max, int width);
    void drawConsole();
    void drawHotkeyLine();
};

#endif // UI_H