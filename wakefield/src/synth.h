#ifndef SYNTH_H
#define SYNTH_H

#include <cmath>
#include <vector>
#include "voice.h"
#include "brainwave_osc.h"
#include "reverb.h"
#include "filters.hpp"

class UI; // Forward declaration

constexpr int MAX_VOICES = 8;
// Note: OSCILLATORS_PER_VOICE is defined in voice.h

class Synth {
public:
    Synth(float sampleRate);
    
    void process(float* output, unsigned int nFrames, unsigned int nChannels);
    
    void noteOn(int midiNote, int velocity);
    void noteOff(int midiNote);
    
    void updateEnvelopeParameters(float attack, float decay, float sustain, float release);
    void setMasterVolume(float volume) { masterVolume = volume; }
    
    // Link to UI for oscilloscope
    void setUI(UI* ui_ptr) { ui = ui_ptr; }

    // Brainwave oscillator control
    void updateBrainwaveParameters(BrainwaveMode mode, float freq, float morph, float duty,
                                     int octave, bool lfoEnabled, int lfoSpeed);
    
    // Reverb control
    void setReverbEnabled(bool enabled) { reverbEnabled = enabled; }
    void updateReverbParameters(float delayTime, float size, float damping, float mix, float decay, 
                                float diffusion, float modDepth, float modFreq);
    
    // Filter control
    void setFilterEnabled(bool enabled) { filterEnabled = enabled; }
    void updateFilterParameters(int type, float cutoff, float gain);
    
    int getActiveVoiceCount() const;
    
private:
    float sampleRate;
    float masterVolume;
    bool reverbEnabled;
    bool filterEnabled;
    int currentFilterType;
    UI* ui;
    
    std::vector<Voice> voices;
    GreyholeReverb reverb;
    
    // Stereo filters (left and right channel)
    OnePoleTPT filterL;
    OnePoleTPT filterR;
    OnePoleHighShelfBLT highShelfL;
    OnePoleHighShelfBLT highShelfR;
    OnePoleLowShelfBLT lowShelfL;
    OnePoleLowShelfBLT lowShelfR;
    
    int findFreeVoice();
    float midiNoteToFrequency(int midiNote);
};

#endif // SYNTH_H