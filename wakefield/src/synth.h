#ifndef SYNTH_H
#define SYNTH_H

#include <cmath>
#include <vector>
#include <mutex>
#include "voice.h"
#include "brainwave_osc.h"
#include "lfo.h"
#include "chaos.h"
#include "reverb.h"
#include "compressor.h"
#include "filters.hpp"
#include "sample_bank.h"
#include "modulation.h"
#include "fm_constants.h"

class UI; // Forward declaration
struct SynthParameters;  // Forward declaration
class Clock; // Forward declaration

constexpr int MAX_VOICES = 8;
// Note: OSCILLATORS_PER_VOICE and SAMPLERS_PER_VOICE are defined in voice.h

// Note source for oscillators and samplers
enum class NoteSource {
    NONE = 0,           // Free running, no note triggering
    EXTERNAL_MIDI = 1,  // Responds to external MIDI input
    TRACK_1 = 2,        // Sequencer track 1
    TRACK_2 = 3,        // Sequencer track 2
    TRACK_3 = 4,        // Sequencer track 3
    TRACK_4 = 5         // Sequencer track 4
};

class Synth {
public:
    Synth(float sampleRate);
    
    void process(float* output, unsigned int nFrames, unsigned int nChannels, float tempo = 120.0f);
    
    void noteOn(int midiNote, int velocity);
    void noteOff(int midiNote);

    // Sequencer track triggering (updates per-track state and triggers voices)
    void sequencerNoteOn(int trackIndex, int midiNote, int velocity, float gatePercent, float probability);
    void sequencerNoteOff(int trackIndex, int midiNote);

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
    void updateFilterParameters(int type, float cutoff, float gain,
                                float resonance, float drive, float feedbackHP,
                                float spread, float notchFeedback, float bandWidth);

    // Compressor control
    void setCompressorEnabled(bool enabled) { compressorEnabled = enabled; }
    void updateCompressorParameters(float threshold, float ratio, float attack,
                                    float release, float knee, float mix,
                                    bool autoMakeup, float manualMakeup, bool rmsMode);
    float getCompressorGainReduction() const;  // For metering (in dB)

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
    float getChaosOutputY(int chaosIndex) const;
    // Get per-sample chaos outputs from buffers (for audio-rate FM)
    float getChaosOutputAtFrame(int chaosIndex, unsigned int frameIndex) const;
    float getChaosOutputYAtFrame(int chaosIndex, unsigned int frameIndex) const;

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
        float oscAmpController[OSCILLATORS_PER_VOICE] = {0.0f, 0.0f, 0.0f, 0.0f};
        bool oscAmpControllerActive[OSCILLATORS_PER_VOICE] = {false, false, false, false};
        float filterCutoff = 0.0f;
        float filterResonance = 0.0f;
        float filterDrive = 0.0f;
        float filterWidth = 0.0f;
        float filterNotchFeedback = 0.0f;
        float filterSpread = 0.0f;
        float filterDryWet = 0.0f;
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
        float samplerAmpController[SAMPLERS_PER_VOICE] = {0.0f, 0.0f, 0.0f, 0.0f};
        bool samplerAmpControllerActive[SAMPLERS_PER_VOICE] = {false, false, false, false};
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
        // FM modulation
        float fmGlobalDepth = 0.0f;
        float fmDepth[kFMTargetCount][kFMSourceCount] = {{0.0f}};
        // Chaos modulation
        float chaosClockFreq[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float chaosParameter[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float chaosLevel[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    };

    ModulationOutputs processModulationMatrix(const Voice* voiceContext = nullptr);
    float applyModCurve(float input, int curveType);
    float getModulationSource(int sourceIndex, const Voice* voiceContext = nullptr);

    int getActiveVoiceCount() const;

    // Voice envelope debugging
    bool isVoiceActive(int voiceIndex) const;
    float getVoiceEnvelopeValue(int voiceIndex) const;
    float getVoiceEnvelopeValue(int voiceIndex, int envIndex) const;
    int getVoiceNote(int voiceIndex) const;

    void setOscillatorState(int index, int shapeIndex,
                             float baseFreq, float morph,
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
    float getFMDepthMod(int target, int source) const;

    // Chaos generator control
    void setChaosParameter(int chaosIndex, float value);
    void setChaosClockFreq(int chaosIndex, float freq);
    void setChaosFastMode(int chaosIndex, bool fast);
    void setChaosInterpMode(int chaosIndex, int mode);
    void resetChaosGenerator(int chaosIndex);
    void resetAudioState();

    // Filter access for debugging
    const Bandpass2PoleZdf* getBandpass2FilterL() const { return &bandpass2FilterL; }

private:
    float sampleRate;
    float masterVolume;
    bool reverbEnabled;
    bool filterEnabled;
    bool compressorEnabled;
    int currentFilterType;
    UI* ui;
    SynthParameters* params;  // Pointer to parameters (for FM matrix)
    Clock* clock;

    std::vector<Voice> voices;
    GreyholeReverb reverb;
    Compressor compressor;

    // 4 global LFOs for modulation
    LFO lfos[4];

    // 4 global chaos generators for modulation
    ChaosGenerator chaos[4];
    float chaosOutputs[4] = {0.0f, 0.0f, 0.0f, 0.0f};  // Cached chaos outputs
    // Per-buffer chaos output traces for audio mixing (filled by processChaos)
    std::vector<float> chaosBufferX[4];
    std::vector<float> chaosBufferY[4];
    // Diff mode and last-sample state for whitening
    bool chaosDiffMode = false;
    float chaosLastX[4] = {0.0f,0.0f,0.0f,0.0f};
    float chaosLastY[4] = {0.0f,0.0f,0.0f,0.0f};

    // Per-track sequencer state (for modulation sources and routing)
    struct SequencerTrackState {
        int note = 60;          // Current MIDI note (60 = middle C)
        int velocity = 100;     // Current velocity (0-127)
        float gatePercent = 0.5f;  // Gate length as percentage (0.0-1.0)
        float probability = 1.0f;  // Probability (0.0-1.0)
        bool active = false;    // Is a note currently active on this track?
    };
    SequencerTrackState trackState[4];  // State for tracks 1-4

public:
    void setChaosDiffMode(bool enabled) { chaosDiffMode = enabled; }
    ModulationOutputs lastGlobalModOutputs;

    int samplerPhaseSource[SAMPLERS_PER_VOICE] = {
        kClockModSourceIndex, kClockModSourceIndex,
        kClockModSourceIndex, kClockModSourceIndex
    };
    int samplerPhaseType[SAMPLERS_PER_VOICE] = {0, 0, 0, 0};
    std::vector<float> fmSourceBuffer;
    std::vector<float> fmSourceBufferPrev;

    // Per-voice temporary buffers for parallel processing
    std::vector<float> voiceBuffers[MAX_VOICES];
    std::vector<float> voiceFmTraces[MAX_VOICES]; // per-voice, per-frame FM sources (osc + sampler)

    // Protect shared state writes invoked from voices (e.g., sampler phase save)
    std::mutex samplerPhaseMutex;

    // Stereo filters (left and right channel)
    OnePoleTPT filterL;
    OnePoleTPT filterR;
    Ladder8PoleZdf ladderFilterL;
    Ladder8PoleZdf ladderFilterR;
    LadderDiodeZdf diodeFilterL;
    LadderDiodeZdf diodeFilterR;
    LadderBandpassZdf bandpassFilterL;
    LadderBandpassZdf bandpassFilterR;
    Ladder8PoleBandpassZdf bandpass8FilterL;
    Ladder8PoleBandpassZdf bandpass8FilterR;
    Bandpass2PoleZdf bandpass2FilterL;
    Bandpass2PoleZdf bandpass2FilterR;
    DualNotchZdf notchFilterL;
    DualNotchZdf notchFilterR;
    OnePoleHighShelfBLT highShelfL;
    OnePoleHighShelfBLT highShelfR;
    OnePoleLowShelfBLT lowShelfL;
    OnePoleLowShelfBLT lowShelfR;

    // Smoothed filter parameters for musical UI control
    float smoothedFilterCutoff = 1000.0f;
    float smoothedFilterResonance = 0.0f;
    float smoothedFilterDrive = 1.0f;
    float smoothedFilterWidth = 0.5f;
    float smoothedFilterNotchFeedback = 0.0f;
    float smoothedFilterSpread = 0.0f;
    float smoothedFilterDryWet = 1.0f;

    // Filter smoothing coefficient (same as FM depth for responsive control)
    static constexpr float kFilterSmoothingAlpha = 0.05f;

    // Per-oscillator base amps (control-rate, modulation target)
    // Combined with ampMod, then multiplied by mix levels
    float oscillatorBaseAmps[OSCILLATORS_PER_VOICE] = {1.0f, 1.0f, 1.0f, 1.0f};

    // Per-oscillator mix levels (control-rate, static mixer)
    // These are multiplied with (amp + ampMod) in voice mixing
    // Phase 5: Reduced from 0.8 to 0.6 for better headroom with normalization
    float oscillatorBaseLevels[OSCILLATORS_PER_VOICE] = {0.6f, 0.6f, 0.6f, 0.6f};

    // Sample bank for all voices
    SampleBank sampleBank;

    // Current sample index for each sampler (shared across all voices)
    int currentSampleIndices[SAMPLERS_PER_VOICE] = {-1, -1, -1, -1};
    bool samplerKeyModes[SAMPLERS_PER_VOICE] = {true, true, true, true};
    Sampler freeSamplers[SAMPLERS_PER_VOICE];

    // Free sampler modulation interpolation (to prevent crackling)
    float prevFreeSamplerPitchMod[SAMPLERS_PER_VOICE] = {0.0f, 0.0f, 0.0f, 0.0f};
    float prevFreeSamplerLoopStartMod[SAMPLERS_PER_VOICE] = {0.0f, 0.0f, 0.0f, 0.0f};
    float prevFreeSamplerLoopLengthMod[SAMPLERS_PER_VOICE] = {0.0f, 0.0f, 0.0f, 0.0f};
    float prevFreeSamplerCrossfadeMod[SAMPLERS_PER_VOICE] = {0.0f, 0.0f, 0.0f, 0.0f};
    float prevFreeSamplerLevelMod[SAMPLERS_PER_VOICE] = {0.0f, 0.0f, 0.0f, 0.0f};
    float prevFreeSamplerLevelOffset[SAMPLERS_PER_VOICE] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Sampler pitch parameters (octave and tune)
    int samplerOctaves[SAMPLERS_PER_VOICE] = {0, 0, 0, 0};     // -5 to +5
    float samplerTunes[SAMPLERS_PER_VOICE] = {0.0f, 0.0f, 0.0f, 0.0f};  // -1.0 to +1.0 (±6 semitones)

    // Sampler sync and note reset parameters
    int samplerSyncModes[SAMPLERS_PER_VOICE] = {0, 0, 0, 0};  // 0=Off, 1=On, 2=Trip, 3=Dot
    bool samplerNoteResets[SAMPLERS_PER_VOICE] = {true, true, true, true};  // true=On, false=Off
    float currentTempo = 120.0f;  // Current tempo for sampler sync

    // Last phase position for Note Reset (persists across voices)
    uint64_t samplerLastPhases[SAMPLERS_PER_VOICE] = {0, 0, 0, 0};

    int findFreeVoice();
    float midiNoteToFrequency(int midiNote);
    void refreshSamplerPhaseDrivers();
    float normalizePhaseForDriver(float value, int type) const;

    // Free-running voice management
    void spawnFreeRunningVoice(int oscIndex);
    void killFreeRunningVoice(int oscIndex);
    bool hasFreeRunningVoice() const {
        for (bool active : freeRunningVoiceActive) {
            if (active) return true;
        }
        return false;
    }

    // Soft clipping utility (Phase 3)
    inline float softClip(float x, float threshold = 0.9f) const {
        // Sanitize input - if NaN or Inf, return 0
        if (!std::isfinite(x)) {
            return 0.0f;
        }

        // Hard clamp to prevent tanh from receiving extreme values
        x = std::clamp(x, -10.0f, 10.0f);

        if (x > threshold) {
            float excess = x - threshold;
            return threshold + (1.0f - threshold) * std::tanh(excess / (1.0f - threshold));
        } else if (x < -threshold) {
            float excess = x + threshold;
            return -threshold + (1.0f - threshold) * std::tanh(excess / (1.0f - threshold));
        }
        return x;
    }

    bool freeRunningVoiceActive[OSCILLATORS_PER_VOICE] = {false, false, false, false};
    int freeRunningVoiceIndex[OSCILLATORS_PER_VOICE] = {-1, -1, -1, -1};
    bool oscillatorNoteSourceFree[OSCILLATORS_PER_VOICE] = {false, false, false, false};

    // Voice stealing: global counter for voice priority/age tracking
    uint64_t voiceCounter = 0;
};

#endif // SYNTH_H
