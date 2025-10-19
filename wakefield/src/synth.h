#ifndef SYNTH_H
#define SYNTH_H

#include <cmath>
#include <vector>
#include "voice.h"
#include "brainwave_osc.h"
#include "lfo.h"
#include "reverb.h"
#include "filters.hpp"

class UI; // Forward declaration
struct SynthParameters;  // Forward declaration

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

    // Link to SynthParameters for FM matrix access
    void setParams(SynthParameters* params_ptr);

    // Brainwave oscillator control
    void updateBrainwaveParameters(BrainwaveMode mode, int shape, float freq, float morph, float duty,
                                   float ratio, float offsetHz,
                                   bool flipPolarity, float level);
    
    // Reverb control
    void setReverbEnabled(bool enabled) { reverbEnabled = enabled; }
    void updateReverbParameters(float delayTime, float size, float damping, float mix, float decay, 
                                float diffusion, float modDepth, float modFreq);
    
    // Filter control
    void setFilterEnabled(bool enabled) { filterEnabled = enabled; }
    void updateFilterParameters(int type, float cutoff, float gain);

    // LFO control
    void updateLFOParameters(int lfoIndex, float period, int syncMode, int shape, float morph,
                             float duty, bool flip, bool resetOnNote, float tempo);

    // LFO processing (called per audio buffer)
    void processLFOs(float sampleRate);

    // Get LFO outputs (for modulation matrix)
    float getLFOOutput(int lfoIndex) const;

    int getActiveVoiceCount() const;
    
private:
    float sampleRate;
    float masterVolume;
    bool reverbEnabled;
    bool filterEnabled;
    int currentFilterType;
    UI* ui;
    SynthParameters* params;  // Pointer to parameters (for FM matrix)

    std::vector<Voice> voices;
    GreyholeReverb reverb;

    // 4 global LFOs for modulation
    LFO lfos[4];

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
