#ifndef SYNTH_H
#define SYNTH_H

#include <cmath>
#include <vector>
#include "voice.h"
#include "brainwave_osc.h"
#include "lfo.h"
#include "reverb.h"
#include "filters.hpp"
#include "sample_bank.h"

class UI; // Forward declaration
struct SynthParameters;  // Forward declaration

constexpr int MAX_VOICES = 8;
// Note: OSCILLATORS_PER_VOICE and SAMPLERS_PER_VOICE are defined in voice.h

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
        float osc1Amp = 0.0f;  // Renamed from osc1Level
        float osc2Amp = 0.0f;  // Renamed from osc2Level
        float osc3Amp = 0.0f;  // Renamed from osc3Level
        float osc4Amp = 0.0f;  // Renamed from osc4Level
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
                            float ratio, float offsetHz, float amp, float level);

    // Get oscillator base amp (for voice mixing with modulation)
    float getOscillatorBaseAmp(int index) const {
        if (index < 0 || index >= OSCILLATORS_PER_VOICE) return 0.0f;
        return oscillatorBaseAmps[index];
    }

    // Get oscillator base level (for voice mixing)
    float getOscillatorBaseLevel(int index) const {
        if (index < 0 || index >= OSCILLATORS_PER_VOICE) return 0.0f;
        return oscillatorBaseLevels[index];
    }

    // Sample bank management
    SampleBank* getSampleBank() { return &sampleBank; }
    const SampleBank* getSampleBank() const { return &sampleBank; }

    // Sampler control
    void setSamplerSample(int samplerIndex, int sampleIndex);
    void setSamplerLoopStart(int samplerIndex, float normalized);
    void setSamplerLoopLength(int samplerIndex, float normalized);
    void setSamplerCrossfadeLength(int samplerIndex, float normalized);
    void setSamplerPlaybackSpeed(int samplerIndex, float speed);
    void setSamplerTZFMDepth(int samplerIndex, float depth);
    void setSamplerPlaybackMode(int samplerIndex, PlaybackMode mode);
    void setSamplerLevel(int samplerIndex, float level);

    // Get sampler state (for UI)
    int getSamplerSampleIndex(int samplerIndex) const;
    float getSamplerLoopStart(int samplerIndex) const;
    float getSamplerLoopLength(int samplerIndex) const;
    float getSamplerCrossfadeLength(int samplerIndex) const;
    float getSamplerPlaybackSpeed(int samplerIndex) const;
    float getSamplerTZFMDepth(int samplerIndex) const;
    PlaybackMode getSamplerPlaybackMode(int samplerIndex) const;
    float getSamplerLevel(int samplerIndex) const;
    
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

    // Per-oscillator base amps (control-rate, modulation target)
    // Combined with ampMod, then multiplied by mix levels
    float oscillatorBaseAmps[OSCILLATORS_PER_VOICE] = {1.0f, 1.0f, 1.0f, 1.0f};

    // Per-oscillator mix levels (control-rate, static mixer)
    // These are multiplied with (amp + ampMod) in voice mixing
    float oscillatorBaseLevels[OSCILLATORS_PER_VOICE] = {1.0f, 0.0f, 0.0f, 0.0f};

    // Sample bank for all voices
    SampleBank sampleBank;

    // Current sample index for each sampler (shared across all voices)
    int currentSampleIndices[SAMPLERS_PER_VOICE] = {-1, -1, -1, -1};
    
    int findFreeVoice();
    float midiNoteToFrequency(int midiNote);
};

#endif // SYNTH_H
