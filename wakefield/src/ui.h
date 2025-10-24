#ifndef UI_H
#define UI_H

#include <ncursesw/curses.h>
#include <algorithm>
#include <string>
#include <atomic>
#include <vector>
#include <functional>
#include "oscillator.h"
#include "cpu_monitor.h"
#include "modulation.h"

class Synth;  // Forward declaration
struct SampleData;  // Forward declaration

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
    std::atomic<int> osc1Mode{1};              // 0=FREE, 1=KEY
    std::atomic<float> osc1Freq{440.0f};       // Base frequency (20-2000 Hz)
    std::atomic<float> osc1Morph{0.5f};        // Waveform morph (0.0001-0.9999)
    std::atomic<int> osc1Shape{0};            // 0=Saw, 1=Pulse
    std::atomic<float> osc1Duty{0.5f};         // Pulse width (0.0-1.0)
    std::atomic<float> osc1Ratio{1.0f};        // FM8-style frequency ratio (0.125-16.0)
    std::atomic<float> osc1Offset{0.0f};       // FM8-style frequency offset Hz (-1000-1000)
    std::atomic<float> osc1Amp{1.0f};          // Amplitude (modulation target, 0.0-1.0)
    std::atomic<float> osc1Level{1.0f};        // Mix level (static, 0.0-1.0)

    // Oscillator 2 (index 1)
    std::atomic<int> osc2Mode{1};
    std::atomic<float> osc2Freq{440.0f};
    std::atomic<float> osc2Morph{0.5f};
    std::atomic<int> osc2Shape{0};
    std::atomic<float> osc2Duty{0.5f};
    std::atomic<float> osc2Ratio{1.0f};
    std::atomic<float> osc2Offset{0.0f};
    std::atomic<float> osc2Amp{1.0f};
    std::atomic<float> osc2Level{0.0f};        // Default: off

    // Oscillator 3 (index 2)
    std::atomic<int> osc3Mode{1};
    std::atomic<float> osc3Freq{440.0f};
    std::atomic<float> osc3Morph{0.5f};
    std::atomic<int> osc3Shape{0};
    std::atomic<float> osc3Duty{0.5f};
    std::atomic<float> osc3Ratio{1.0f};
    std::atomic<float> osc3Offset{0.0f};
    std::atomic<float> osc3Amp{1.0f};
    std::atomic<float> osc3Level{0.0f};        // Default: off

    // Oscillator 4 (index 3)
    std::atomic<int> osc4Mode{1};
    std::atomic<float> osc4Freq{440.0f};
    std::atomic<float> osc4Morph{0.5f};
    std::atomic<int> osc4Shape{0};
    std::atomic<float> osc4Duty{0.5f};
    std::atomic<float> osc4Ratio{1.0f};
    std::atomic<float> osc4Offset{0.0f};
    std::atomic<float> osc4Amp{1.0f};
    std::atomic<float> osc4Level{0.0f};        // Default: off

    // Mixer mute/solo state (4 OSC + 4 SAMP = 8 channels)
    std::atomic<bool> oscMuted[4]{false, false, false, false};
    std::atomic<bool> oscSolo[4]{false, false, false, false};
    std::atomic<bool> samplerMuted[4]{false, false, false, false};
    std::atomic<bool> samplerSolo[4]{false, false, false, false};

    // LFO parameters - 4 global modulation sources
    std::atomic<float> lfo1Period{1.0f};
    std::atomic<int> lfo1SyncMode{0};          // 0=Off,1=On,2=Triplet,3=Dotted
    std::atomic<float> lfo1Morph{0.5f};
    std::atomic<float> lfo1Duty{0.5f};
    std::atomic<bool> lfo1Flip{false};
    std::atomic<bool> lfo1ResetOnNote{false};
    std::atomic<int> lfo1Shape{0}; // 0=PD, 1=Pulse

    std::atomic<float> lfo2Period{1.0f};
    std::atomic<int> lfo2SyncMode{0};
    std::atomic<float> lfo2Morph{0.5f};
    std::atomic<float> lfo2Duty{0.5f};
    std::atomic<bool> lfo2Flip{false};
    std::atomic<bool> lfo2ResetOnNote{false};
    std::atomic<int> lfo2Shape{0}; // 0=PD, 1=Pulse

    std::atomic<float> lfo3Period{1.0f};
    std::atomic<int> lfo3SyncMode{0};
    std::atomic<float> lfo3Morph{0.5f};
    std::atomic<float> lfo3Duty{0.5f};
    std::atomic<bool> lfo3Flip{false};
    std::atomic<bool> lfo3ResetOnNote{false};
    std::atomic<int> lfo3Shape{0}; // 0=PD, 1=Pulse

    std::atomic<float> lfo4Period{1.0f};
    std::atomic<int> lfo4SyncMode{0};
    std::atomic<float> lfo4Morph{0.5f};
    std::atomic<float> lfo4Duty{0.5f};
    std::atomic<bool> lfo4Flip{false};
    std::atomic<bool> lfo4ResetOnNote{false};
    std::atomic<int> lfo4Shape{0}; // 0=PD, 1=Pulse

    std::atomic<float> lfoVisualValue[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    std::atomic<float> lfoVisualPhase[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    float getLfoVisualValue(int index) const {
        if (index < 0 || index >= 4) return 0.0f;
        return lfoVisualValue[index].load();
    }

    float getLfoVisualPhase(int index) const {
        if (index < 0 || index >= 4) return 0.0f;
        return lfoVisualPhase[index].load();
    }

    void setLfoVisualState(int index, float value, float phase) {
        if (index < 0 || index >= 4) return;
        lfoVisualValue[index].store(value);
        lfoVisualPhase[index].store(phase);
    }

    // Envelope parameters - 4 independent modulation envelopes
    std::atomic<float> env1Attack{0.01f};
    std::atomic<float> env1Decay{0.1f};
    std::atomic<float> env1Sustain{0.7f};
    std::atomic<float> env1Release{0.2f};
    std::atomic<float> env1AttackBend{0.5f};
    std::atomic<float> env1ReleaseBend{0.5f};

    std::atomic<float> env2Attack{0.01f};
    std::atomic<float> env2Decay{0.1f};
    std::atomic<float> env2Sustain{0.7f};
    std::atomic<float> env2Release{0.2f};
    std::atomic<float> env2AttackBend{0.5f};
    std::atomic<float> env2ReleaseBend{0.5f};

    std::atomic<float> env3Attack{0.01f};
    std::atomic<float> env3Decay{0.1f};
    std::atomic<float> env3Sustain{0.7f};
    std::atomic<float> env3Release{0.2f};
    std::atomic<float> env3AttackBend{0.5f};
    std::atomic<float> env3ReleaseBend{0.5f};

    std::atomic<float> env4Attack{0.01f};
    std::atomic<float> env4Decay{0.1f};
    std::atomic<float> env4Sustain{0.7f};
    std::atomic<float> env4Release{0.2f};
    std::atomic<float> env4AttackBend{0.5f};
    std::atomic<float> env4ReleaseBend{0.5f};

    // Chaos parameters - 4 global chaos generators (Ikeda map)
    std::atomic<float> chaos1Parameter{0.6f};      // u parameter (0-2, ~0.6 is chaotic)
    std::atomic<float> chaos1ClockFreq{1.0f};      // Clock frequency in Hz (0.01-1000)
    std::atomic<bool> chaos1CubicInterp{false};    // Cubic interpolation vs linear
    std::atomic<bool> chaos1FastMode{false};       // Audio rate vs clocked

    std::atomic<float> chaos2Parameter{0.6f};
    std::atomic<float> chaos2ClockFreq{1.0f};
    std::atomic<bool> chaos2CubicInterp{false};
    std::atomic<bool> chaos2FastMode{false};

    std::atomic<float> chaos3Parameter{0.6f};
    std::atomic<float> chaos3ClockFreq{1.0f};
    std::atomic<bool> chaos3CubicInterp{false};
    std::atomic<bool> chaos3FastMode{false};

    std::atomic<float> chaos4Parameter{0.6f};
    std::atomic<float> chaos4ClockFreq{1.0f};
    std::atomic<bool> chaos4CubicInterp{false};
    std::atomic<bool> chaos4FastMode{false};

    // FM Matrix - audio-rate frequency modulation routing
    // fmMatrix[target][source] = depth (0.0 to 1.0)
    // Example: fmMatrix[2][0] = 0.5 means OSC1 modulates OSC3 at 50% depth
    // 8 total: OSC1-4 (indices 0-3) + SAMP1-4 (indices 4-7)
    std::atomic<float> fmMatrix[8][8];  // 8 targets × 8 sources (4 OSC + 4 SAMP)

    // Constructor to initialize CC map and FM matrix
    SynthParameters() {
        for (int i = 0; i < 50; ++i) {
            parameterCCMap[i] = -1;  // -1 means no CC assigned
        }
        // Initialize FM matrix to zero (no FM routing by default)
        for (int target = 0; target < 8; ++target) {
            for (int source = 0; source < 8; ++source) {
                fmMatrix[target][source] = 0.0f;
            }
        }
    }

    int getOscMode(int index) const {
        switch (index) {
            case 0: return osc1Mode.load();
            case 1: return osc2Mode.load();
            case 2: return osc3Mode.load();
            case 3: return osc4Mode.load();
            default: return osc1Mode.load();
        }
    }

    void setOscMode(int index, int value) {
        switch (index) {
            case 0: osc1Mode = value; break;
            case 1: osc2Mode = value; break;
            case 2: osc3Mode = value; break;
            case 3: osc4Mode = value; break;
            default: osc1Mode = value; break;
        }
    }

    int getOscShape(int index) const {
        switch (index) {
            case 0: return osc1Shape.load();
            case 1: return osc2Shape.load();
            case 2: return osc3Shape.load();
            case 3: return osc4Shape.load();
            default: return osc1Shape.load();
        }
    }

    void setOscShape(int index, int value) {
        switch (index) {
            case 0: osc1Shape = value; break;
            case 1: osc2Shape = value; break;
            case 2: osc3Shape = value; break;
            case 3: osc4Shape = value; break;
            default: osc1Shape = value; break;
        }
    }

    float getOscFrequency(int index) const {
        switch (index) {
            case 0: return osc1Freq.load();
            case 1: return osc2Freq.load();
            case 2: return osc3Freq.load();
            case 3: return osc4Freq.load();
            default: return osc1Freq.load();
        }
    }

    void setOscFrequency(int index, float value) {
        switch (index) {
            case 0: osc1Freq = value; break;
            case 1: osc2Freq = value; break;
            case 2: osc3Freq = value; break;
            case 3: osc4Freq = value; break;
            default: osc1Freq = value; break;
        }
    }

    float getOscMorph(int index) const {
        switch (index) {
            case 0: return osc1Morph.load();
            case 1: return osc2Morph.load();
            case 2: return osc3Morph.load();
            case 3: return osc4Morph.load();
            default: return osc1Morph.load();
        }
    }

    void setOscMorph(int index, float value) {
        switch (index) {
            case 0: osc1Morph = value; break;
            case 1: osc2Morph = value; break;
            case 2: osc3Morph = value; break;
            case 3: osc4Morph = value; break;
            default: osc1Morph = value; break;
        }
    }

    float getOscDuty(int index) const {
        switch (index) {
            case 0: return osc1Duty.load();
            case 1: return osc2Duty.load();
            case 2: return osc3Duty.load();
            case 3: return osc4Duty.load();
            default: return osc1Duty.load();
        }
    }

    void setOscDuty(int index, float value) {
        switch (index) {
            case 0: osc1Duty = value; break;
            case 1: osc2Duty = value; break;
            case 2: osc3Duty = value; break;
            case 3: osc4Duty = value; break;
            default: osc1Duty = value; break;
        }
    }

    float getOscRatio(int index) const {
        switch (index) {
            case 0: return osc1Ratio.load();
            case 1: return osc2Ratio.load();
            case 2: return osc3Ratio.load();
            case 3: return osc4Ratio.load();
            default: return osc1Ratio.load();
        }
    }

    void setOscRatio(int index, float value) {
        switch (index) {
            case 0: osc1Ratio = value; break;
            case 1: osc2Ratio = value; break;
            case 2: osc3Ratio = value; break;
            case 3: osc4Ratio = value; break;
            default: osc1Ratio = value; break;
        }
    }

    float getOscOffset(int index) const {
        switch (index) {
            case 0: return osc1Offset.load();
            case 1: return osc2Offset.load();
            case 2: return osc3Offset.load();
            case 3: return osc4Offset.load();
            default: return osc1Offset.load();
        }
    }

    void setOscOffset(int index, float value) {
        switch (index) {
            case 0: osc1Offset = value; break;
            case 1: osc2Offset = value; break;
            case 2: osc3Offset = value; break;
            case 3: osc4Offset = value; break;
            default: osc1Offset = value; break;
        }
    }


    float getOscAmp(int index) const {
        switch (index) {
            case 0: return osc1Amp.load();
            case 1: return osc2Amp.load();
            case 2: return osc3Amp.load();
            case 3: return osc4Amp.load();
            default: return osc1Amp.load();
        }
    }

    void setOscAmp(int index, float value) {
        switch (index) {
            case 0: osc1Amp = value; break;
            case 1: osc2Amp = value; break;
            case 2: osc3Amp = value; break;
            case 3: osc4Amp = value; break;
            default: osc1Amp = value; break;
        }
    }

    float getOscLevel(int index) const {
        switch (index) {
            case 0: return osc1Level.load();
            case 1: return osc2Level.load();
            case 2: return osc3Level.load();
            case 3: return osc4Level.load();
            default: return osc1Level.load();
        }
    }

    void setOscLevel(int index, float value) {
        switch (index) {
            case 0: osc1Level = value; break;
            case 1: osc2Level = value; break;
            case 2: osc3Level = value; break;
            case 3: osc4Level = value; break;
            default: osc1Level = value; break;
        }
    }

    float getLfoPeriod(int index) const {
        switch (index) {
            case 0: return lfo1Period.load();
            case 1: return lfo2Period.load();
            case 2: return lfo3Period.load();
            case 3: return lfo4Period.load();
            default: return lfo1Period.load();
        }
    }

    void setLfoPeriod(int index, float value) {
        switch (index) {
            case 0: lfo1Period = value; break;
            case 1: lfo2Period = value; break;
            case 2: lfo3Period = value; break;
            case 3: lfo4Period = value; break;
            default: lfo1Period = value; break;
        }
    }

    int getLfoSyncMode(int index) const {
        switch (index) {
            case 0: return lfo1SyncMode.load();
            case 1: return lfo2SyncMode.load();
            case 2: return lfo3SyncMode.load();
            case 3: return lfo4SyncMode.load();
            default: return lfo1SyncMode.load();
        }
    }

    void setLfoSyncMode(int index, int value) {
        switch (index) {
            case 0: lfo1SyncMode = value; break;
            case 1: lfo2SyncMode = value; break;
            case 2: lfo3SyncMode = value; break;
            case 3: lfo4SyncMode = value; break;
            default: lfo1SyncMode = value; break;
        }
    }

    float getLfoMorph(int index) const {
        switch (index) {
            case 0: return lfo1Morph.load();
            case 1: return lfo2Morph.load();
            case 2: return lfo3Morph.load();
            case 3: return lfo4Morph.load();
            default: return lfo1Morph.load();
        }
    }

    void setLfoMorph(int index, float value) {
        switch (index) {
            case 0: lfo1Morph = value; break;
            case 1: lfo2Morph = value; break;
            case 2: lfo3Morph = value; break;
            case 3: lfo4Morph = value; break;
            default: lfo1Morph = value; break;
        }
    }

    float getLfoDuty(int index) const {
        switch (index) {
            case 0: return lfo1Duty.load();
            case 1: return lfo2Duty.load();
            case 2: return lfo3Duty.load();
            case 3: return lfo4Duty.load();
            default: return lfo1Duty.load();
        }
    }

    void setLfoDuty(int index, float value) {
        switch (index) {
            case 0: lfo1Duty = value; break;
            case 1: lfo2Duty = value; break;
            case 2: lfo3Duty = value; break;
            case 3: lfo4Duty = value; break;
            default: lfo1Duty = value; break;
        }
    }

    bool getLfoFlip(int index) const {
        switch (index) {
            case 0: return lfo1Flip.load();
            case 1: return lfo2Flip.load();
            case 2: return lfo3Flip.load();
            case 3: return lfo4Flip.load();
            default: return lfo1Flip.load();
        }
    }

    void setLfoFlip(int index, bool value) {
        switch (index) {
            case 0: lfo1Flip = value; break;
            case 1: lfo2Flip = value; break;
            case 2: lfo3Flip = value; break;
            case 3: lfo4Flip = value; break;
            default: lfo1Flip = value; break;
        }
    }

    bool getLfoResetOnNote(int index) const {
        switch (index) {
            case 0: return lfo1ResetOnNote.load();
            case 1: return lfo2ResetOnNote.load();
            case 2: return lfo3ResetOnNote.load();
            case 3: return lfo4ResetOnNote.load();
            default: return lfo1ResetOnNote.load();
        }
    }

    void setLfoResetOnNote(int index, bool value) {
        switch (index) {
            case 0: lfo1ResetOnNote = value; break;
            case 1: lfo2ResetOnNote = value; break;
            case 2: lfo3ResetOnNote = value; break;
            case 3: lfo4ResetOnNote = value; break;
            default: lfo1ResetOnNote = value; break;
        }
    }

    int getLfoShape(int index) const {
        switch (index) {
            case 0: return lfo1Shape.load();
            case 1: return lfo2Shape.load();
            case 2: return lfo3Shape.load();
            case 3: return lfo4Shape.load();
            default: return lfo1Shape.load();
        }
    }

    void setLfoShape(int index, int value) {
        switch (index) {
            case 0: lfo1Shape = value; break;
            case 1: lfo2Shape = value; break;
            case 2: lfo3Shape = value; break;
            case 3: lfo4Shape = value; break;
            default: lfo1Shape = value; break;
        }
    }

    // Envelope getters/setters
    float getEnvAttack(int index) const {
        switch (index) {
            case 0: return env1Attack.load();
            case 1: return env2Attack.load();
            case 2: return env3Attack.load();
            case 3: return env4Attack.load();
            default: return env1Attack.load();
        }
    }

    void setEnvAttack(int index, float value) {
        switch (index) {
            case 0: env1Attack = value; attack = value; break;
            case 1: env2Attack = value; break;
            case 2: env3Attack = value; break;
            case 3: env4Attack = value; break;
            default: env1Attack = value; break;
        }
    }

    float getEnvDecay(int index) const {
        switch (index) {
            case 0: return env1Decay.load();
            case 1: return env2Decay.load();
            case 2: return env3Decay.load();
            case 3: return env4Decay.load();
            default: return env1Decay.load();
        }
    }

    void setEnvDecay(int index, float value) {
        switch (index) {
            case 0: env1Decay = value; decay = value; break;
            case 1: env2Decay = value; break;
            case 2: env3Decay = value; break;
            case 3: env4Decay = value; break;
            default: env1Decay = value; break;
        }
    }

    float getEnvSustain(int index) const {
        switch (index) {
            case 0: return env1Sustain.load();
            case 1: return env2Sustain.load();
            case 2: return env3Sustain.load();
            case 3: return env4Sustain.load();
            default: return env1Sustain.load();
        }
    }

    void setEnvSustain(int index, float value) {
        switch (index) {
            case 0: env1Sustain = value; sustain = value; break;
            case 1: env2Sustain = value; break;
            case 2: env3Sustain = value; break;
            case 3: env4Sustain = value; break;
            default: env1Sustain = value; break;
        }
    }

    float getEnvRelease(int index) const {
        switch (index) {
            case 0: return env1Release.load();
            case 1: return env2Release.load();
            case 2: return env3Release.load();
            case 3: return env4Release.load();
            default: return env1Release.load();
        }
    }

    void setEnvRelease(int index, float value) {
        switch (index) {
            case 0: env1Release = value; release = value; break;
            case 1: env2Release = value; break;
            case 2: env3Release = value; break;
            case 3: env4Release = value; break;
            default: env1Release = value; break;
        }
    }

    float getEnvAttackBend(int index) const {
        switch (index) {
            case 0: return env1AttackBend.load();
            case 1: return env2AttackBend.load();
            case 2: return env3AttackBend.load();
            case 3: return env4AttackBend.load();
            default: return env1AttackBend.load();
        }
    }

    void setEnvAttackBend(int index, float value) {
        switch (index) {
            case 0: env1AttackBend = value; break;
            case 1: env2AttackBend = value; break;
            case 2: env3AttackBend = value; break;
            case 3: env4AttackBend = value; break;
            default: env1AttackBend = value; break;
        }
    }

    float getEnvReleaseBend(int index) const {
        switch (index) {
            case 0: return env1ReleaseBend.load();
            case 1: return env2ReleaseBend.load();
            case 2: return env3ReleaseBend.load();
            case 3: return env4ReleaseBend.load();
            default: return env1ReleaseBend.load();
        }
    }

    void setEnvReleaseBend(int index, float value) {
        switch (index) {
            case 0: env1ReleaseBend = value; break;
            case 1: env2ReleaseBend = value; break;
            case 2: env3ReleaseBend = value; break;
            case 3: env4ReleaseBend = value; break;
            default: env1ReleaseBend = value; break;
        }
    }

    // FM Matrix accessors (8x8: OSC1-4 are 0-3, SAMP1-4 are 4-7)
    float getFMDepth(int target, int source) const {
        if (target < 0 || target >= 8 || source < 0 || source >= 8) return 0.0f;
        return fmMatrix[target][source].load();
    }

    void setFMDepth(int target, int source, float depth) {
        if (target < 0 || target >= 8 || source < 0 || source >= 8) return;
        const float clamped = std::max(-0.99f, std::min(0.99f, depth));
        fmMatrix[target][source] = clamped;
    }
};

enum class UIPage {
    OSCILLATOR,
    SAMPLER,
    MIXER,
    LFO,
    ENV,
    FM,
    MOD,
    REVERB,
    FILTER,
    LOOPER,
    SEQUENCER,
    CLOCK,
    CHAOS,
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
    
    void addConsoleMessage(const std::string&) {}

    // Get parameter name by ID (public for MIDI handler)
    std::string getParameterName(int id);

    // Waveform buffer for oscilloscope
    void writeToWaveformBuffer(float sample);

    // LFO amplitude history for rolling scope view
    void writeToLFOHistory(int lfoIndex, float amplitude);

    // CPU monitor access
    CPUMonitor& getCPUMonitor() { return cpuMonitor; }

    // Preset management
    void loadPreset(const std::string& filename);
    void savePreset(const std::string& filename);
    
    // Device change request (returns true if restart requested)
    bool isDeviceChangeRequested() const { return deviceChangeRequested; }
    int getRequestedAudioDevice() const { return requestedAudioDeviceId; }
    int getRequestedMidiDevice() const { return requestedMidiPortNum; }
    void clearDeviceChangeRequest() { deviceChangeRequested = false; }

    // MOD matrix data (16 modulation slots) - public for Synth access
    ModulationSlot modulationSlots[16];

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
    
    // Parameter navigation state
    int selectedParameterId;
    bool numericInputActive;
    std::string numericInputBuffer;
    bool numericInputIsMod;  // True if numeric input is for MOD matrix Amount
    
    // Preset management
    std::string currentPresetName;
    std::vector<std::string> availablePresets;
    bool textInputActive;
    std::string textInputBuffer;
    
    static const int WAVEFORM_BUFFER_SIZE = 8192;
    std::vector<float> waveformBuffer;
    std::atomic<int> waveformBufferWritePos;

    // LFO amplitude history (no longer used for display, kept for potential future use)
    static const int LFO_HISTORY_SIZE = 2048;
    std::vector<float> lfoHistoryBuffer[4];  // One per LFO
    int lfoHistoryWritePos[4];

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

    // MIDI keyboard mode
    bool midiKeyboardMode;
    std::vector<int> activeKeyboardNotes;  // Track which notes are currently pressed
    int getCurrentOctave() const { return midiKeyboardOctave; }
    void setCurrentOctave(int octave) { midiKeyboardOctave = octave; }

private:
    int midiKeyboardOctave;  // Current octave (0-10, default 4 = middle C)

    // CPU monitor
    CPUMonitor cpuMonitor;
    void drawCPUOverlay();

    // Text input for preset names
    void startTextInput();
    void handleTextInput(int ch);
    void finishTextInput();
    
    // Handle keyboard input
    void handleInput(int ch);
    
    // Draw helper functions
    void drawTabs();
    void drawParametersPage(int startRow = 3, int startCol = 2);  // Generic parameter page drawing
    void drawOscillatorPage();
    void drawSamplerPage();
    void drawMixerPage();
    void drawLFOPage();
    void drawEnvelopePage();
    void drawFMPage();
    void drawModPage();
    void drawReverbPage();
    void drawFilterPage();
    void drawLooperPage();
    void drawClockPage();
    void drawSequencerPage();
    void drawChaosPage();
    void drawConfigPage();
    void drawBar(int y, int x, const char* label, float value, float min, float max, int width);
    void drawHotkeyLine();
    void drawOscillatorWavePreview(int topRow, int leftCol, int plotHeight, int plotWidth);
    void drawLFOWavePreview(int topRow, int leftCol, int plotHeight, int plotWidth, int lfoIndex, float phase);
    void drawEnvelopePreview(int topRow, int leftCol, int plotHeight, int plotWidth);

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

    // MOD matrix menu helpers
    void startModMatrixMenu();
    void handleModMatrixMenuInput(int ch);
    void finishModMatrixMenu(bool applySelection);
    void startModMatrixAmountInput();
    void finishModMatrixAmountInput();

    // Preset management helpers
    void refreshPresetList();
    
    // Parameter management
    std::vector<InlineParameter> parameters;
    void initializeParameters();
    InlineParameter* getParameter(int id);
    std::vector<int> getParameterIdsForPage(UIPage page);
    void adjustParameter(int id, bool increase, bool fine);
    void setParameterValue(int id, float value);
    float getParameterValue(int id);
    std::string getParameterDisplayString(int id);
    void startNumericInput(int id);
    void finishNumericInput();
    void startMidiLearn(int id);
    void finishMidiLearn();
    bool isParameterModulated(int id);  // Check if parameter has active modulation

    // Oscillator/LFO/Envelope UI state
    int currentOscillatorIndex;  // 0-3: which oscillator is selected on OSCILLATOR page
    int currentSamplerIndex;     // 0-3: which sampler is selected on SAMPLER page
    int currentLFOIndex;          // 0-3: which LFO is selected on LFO page
    int currentEnvelopeIndex;     // 0-3: which envelope is selected on ENV page

    // FM Matrix UI state
    int fmMatrixCursorRow;        // 0-3: source oscillator (row)
    int fmMatrixCursorCol;        // 0-3: target oscillator (column)

    // MOD Matrix UI state
    int modMatrixCursorRow;       // 0-15: modulation slot
    int modMatrixCursorCol;       // 0-4: column within slot (Source/Curve/Amount/Destination/Type)

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

    // MOD matrix selection menu
    bool modMatrixMenuActive;
    int modMatrixMenuIndex;
    int modMatrixMenuColumn;  // Which column the menu is for (0=Source, 1=Curve, 3=Dest, 4=Type)
    int modMatrixDestinationModuleIndex;
    int modMatrixDestinationParamIndex;
    int modMatrixDestinationFocusColumn;

    // Sample browser state
    bool sampleBrowserActive;
    std::string sampleBrowserCurrentDir;
    std::vector<std::string> sampleBrowserFiles;
    std::vector<std::string> sampleBrowserDirs;
    int sampleBrowserSelectedIndex;
    int sampleBrowserScrollOffset;

    // Sample browser helpers
    void startSampleBrowser();
    void handleSampleBrowserInput(int ch);
    void finishSampleBrowser(bool applySelection);
    void refreshSampleBrowserFiles();
    void loadSampleForCurrentSampler(const std::string& filepath);
};

#endif // UI_H
