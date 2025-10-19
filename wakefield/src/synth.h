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
    void processLFOs(float sampleRate, unsigned int nFrames);

    // Get LFO outputs (for modulation matrix)
    float getLFOOutput(int lfoIndex) const;

    // Modulation matrix processing
    struct ModulationOutputs {
        float osc1Pitch = 0.0f;
        float osc2Pitch = 0.0f;
        float osc3Pitch = 0.0f;
        float osc4Pitch = 0.0f;
        float osc1Morph = 0.0f;
        float osc2Morph = 0.0f;
        float osc3Morph = 0.0f;
        float osc4Morph = 0.0f;
        float osc1Duty = 0.0f;
        float osc2Duty = 0.0f;
        float osc3Duty = 0.0f;
        float osc4Duty = 0.0f;
        float osc1Ratio = 0.0f;
        float osc2Ratio = 0.0f;
        float osc3Ratio = 0.0f;
        float osc4Ratio = 0.0f;
        float osc1Offset = 0.0f;
        float osc2Offset = 0.0f;
        float osc3Offset = 0.0f;
        float osc4Offset = 0.0f;
        float osc1Level = 0.0f;
        float osc2Level = 0.0f;
        float osc3Level = 0.0f;
        float osc4Level = 0.0f;
        float filterCutoff = 0.0f;
        float filterResonance = 0.0f;
        float reverbMix = 0.0f;
        float reverbSize = 0.0f;
    };

    ModulationOutputs processModulationMatrix();
    float applyModCurve(float input, int curveType);
    float getModulationSource(int sourceIndex);

    int getActiveVoiceCount() const;
    void setOscillatorState(int index, BrainwaveMode mode, int shape,
                            float baseFreq, float morph, float duty,
                            float ratio, float offsetHz, bool flipPolarity, float level);

    // Get oscillator base level (for voice mixing)
    float getOscillatorBaseLevel(int index) const {
        if (index < 0 || index >= OSCILLATORS_PER_VOICE) return 0.0f;
        return oscillatorBaseLevels[index];
    }
    
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

    // Per-oscillator base levels (control-rate, set from UI/params)
    // These are combined with levelMod in voice mixing
    float oscillatorBaseLevels[OSCILLATORS_PER_VOICE] = {1.0f, 0.0f, 0.0f, 0.0f};
    
    int findFreeVoice();
    float midiNoteToFrequency(int midiNote);
};

#endif // SYNTH_H
