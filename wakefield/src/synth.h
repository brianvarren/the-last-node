#ifndef SYNTH_H
#define SYNTH_H

#include <cmath>
#include <vector>
#include "voice.h"
#include "brainwave_osc.h"
#include "lfo.h"
#include "chaos.h"
#include "reverb.h"
#include "filters.hpp"
#include "sample_bank.h"
#include "modulation.h"

class UI; // Forward declaration
struct SynthParameters;  // Forward declaration
class Clock; // Forward declaration

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

    // Link global clock for modulation sources and synced modules
    void setClock(Clock* clockPtr) { clock = clockPtr; }
    Clock* getClock() const { return clock; }

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

    // Chaos processing (called per audio buffer)
    void processChaos(unsigned int nFrames);

    // Get chaos outputs (for modulation matrix and FM)
    float getChaosOutput(int chaosIndex) const;

    // Access modulation slot definitions (read-only)
    const ModulationSlot* getModulationSlot(int index) const;

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
        // Sampler 1 modulation
        float samp1Pitch = 0.0f;
        float samp1LoopStart = 0.0f;
        float samp1LoopLength = 0.0f;
        float samp1Crossfade = 0.0f;
        float samp1Amp = 0.0f;
        // Sampler 2 modulation
        float samp2Pitch = 0.0f;
        float samp2LoopStart = 0.0f;
        float samp2LoopLength = 0.0f;
        float samp2Crossfade = 0.0f;
        float samp2Amp = 0.0f;
        // Sampler 3 modulation
        float samp3Pitch = 0.0f;
        float samp3LoopStart = 0.0f;
        float samp3LoopLength = 0.0f;
        float samp3Crossfade = 0.0f;
        float samp3Amp = 0.0f;
        // Sampler 4 modulation
        float samp4Pitch = 0.0f;
        float samp4LoopStart = 0.0f;
        float samp4LoopLength = 0.0f;
        float samp4Crossfade = 0.0f;
        float samp4Amp = 0.0f;
        // LFO modulation
        float lfoPeriod[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float lfoMorph[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float lfoDuty[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        // Mixer modulation
        float mixerMasterVolume = 0.0f;
        float mixerOscLevel[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float mixerSamplerLevel[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        // Clock target modulation
        float sequencerPhase[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float samplerPhase[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    };

    ModulationOutputs processModulationMatrix(const Voice* voiceContext = nullptr);
    float applyModCurve(float input, int curveType);
    float getModulationSource(int sourceIndex, const Voice* voiceContext = nullptr);

    int getActiveVoiceCount() const;

    // Voice envelope debugging
    bool isVoiceActive(int voiceIndex) const;
    float getVoiceEnvelopeValue(int voiceIndex) const;
    int getVoiceNote(int voiceIndex) const;

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
    void setSamplerOctave(int samplerIndex, int octave);
    void setSamplerTune(int samplerIndex, float tune);
    void setSamplerSyncMode(int samplerIndex, int mode);
    void setSamplerNoteReset(int samplerIndex, bool enabled);
    void setSamplerLevel(int samplerIndex, float level);
    void setSamplerKeyMode(int samplerIndex, bool enabled);
    void saveSamplerPhase(int samplerIndex, uint64_t phase);  // Save phase for Note Reset

    // Get sampler state (for UI)
    int getSamplerSampleIndex(int samplerIndex) const;
    float getSamplerLoopStart(int samplerIndex) const;
    float getSamplerLoopLength(int samplerIndex) const;
    float getSamplerCrossfadeLength(int samplerIndex) const;
    float getSamplerPlaybackSpeed(int samplerIndex) const;
    float getSamplerTZFMDepth(int samplerIndex) const;
    PlaybackMode getSamplerPlaybackMode(int samplerIndex) const;
    float getSamplerLevel(int samplerIndex) const;
    bool getSamplerKeyMode(int samplerIndex) const;
    int getSamplerOctave(int samplerIndex) const;
    float getSamplerTune(int samplerIndex) const;
    int getSamplerSyncMode(int samplerIndex) const;
    bool getSamplerNoteReset(int samplerIndex) const;
    float getModulatedOscLevel(int index) const;
    float getMixerSamplerLevelMod(int index) const;
    float getMixerOscLevelMod(int index) const;

    // Chaos generator control
    void setChaosParameter(int chaosIndex, float value);
    void setChaosClockFreq(int chaosIndex, float freq);
    void setChaosFastMode(int chaosIndex, bool fast);
    void setChaosCubicInterp(int chaosIndex, bool cubic);
    void resetChaosGenerator(int chaosIndex);

private:
    float sampleRate;
    float masterVolume;
    bool reverbEnabled;
    bool filterEnabled;
    int currentFilterType;
    UI* ui;
    SynthParameters* params;  // Pointer to parameters (for FM matrix)
    Clock* clock;

    std::vector<Voice> voices;
    GreyholeReverb reverb;

    // 4 global LFOs for modulation
    LFO lfos[4];

    // 4 global chaos generators for modulation
    ChaosGenerator chaos[4];
    float chaosOutputs[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // Cached chaos outputs
    ModulationOutputs lastGlobalModOutputs;

    int samplerPhaseSource[SAMPLERS_PER_VOICE] = {
        kClockModSourceIndex, kClockModSourceIndex,
        kClockModSourceIndex, kClockModSourceIndex
    };
    int samplerPhaseType[SAMPLERS_PER_VOICE] = {0, 0, 0, 0};

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
    bool samplerKeyModes[SAMPLERS_PER_VOICE] = {true, true, true, true};
    Sampler freeSamplers[SAMPLERS_PER_VOICE];

    // Sampler pitch parameters (octave and tune)
    int samplerOctaves[SAMPLERS_PER_VOICE] = {0, 0, 0, 0};     // -5 to +5
    float samplerTunes[SAMPLERS_PER_VOICE] = {0.0f, 0.0f, 0.0f, 0.0f};  // -1.0 to +1.0 (±6 semitones)

    // Sampler sync and note reset parameters
    int samplerSyncModes[SAMPLERS_PER_VOICE] = {0, 0, 0, 0};  // 0=Off, 1=On, 2=Trip, 3=Dot
    bool samplerNoteResets[SAMPLERS_PER_VOICE] = {true, true, true, true};  // true=On, false=Off

    // Last phase position for Note Reset (persists across voices)
    uint64_t samplerLastPhases[SAMPLERS_PER_VOICE] = {0, 0, 0, 0};

    int findFreeVoice();
    float midiNoteToFrequency(int midiNote);
    void refreshSamplerPhaseDrivers();
    float normalizePhaseForDriver(float value, int type) const;
};

#endif // SYNTH_H
