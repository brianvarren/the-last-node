#ifndef UI_H
#define UI_H

#include <ncursesw/curses.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <atomic>
#include <vector>
#include <functional>
#include <chrono>
#include <termios.h>
#include <unistd.h>
#include "fm_constants.h"
#include "oscillator.h"
#include "cpu_monitor.h"
#include "modulation.h"
#include "theme.h"

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
enum class ParamCurve {
    Linear,
    Logarithmic
};

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
    bool randomizable; // Can be affected by global randomize/mutate operations
    ParamCurve curve = ParamCurve::Linear;

    float denormalize(float normalized) const {
        float norm = std::clamp(normalized, 0.0f, 1.0f);
        switch (type) {
            case ParamType::FLOAT: {
                float clampedMin = min_val;
                float clampedMax = max_val;
                if (curve == ParamCurve::Logarithmic) {
                    float safeMin = std::max(clampedMin, std::numeric_limits<float>::min());
                    float safeMax = std::max(clampedMax, safeMin + std::numeric_limits<float>::epsilon());
                    float logMin = std::log(safeMin);
                    float logMax = std::log(safeMax);
                    float value = std::exp(logMin + norm * (logMax - logMin));
                    return std::clamp(value, clampedMin, clampedMax);
                }
                return clampedMin + norm * (clampedMax - clampedMin);
            }
            case ParamType::INT:
            case ParamType::ENUM: {
                int minInt = static_cast<int>(std::round(min_val));
                int maxInt = static_cast<int>(std::round(max_val));
                int span = std::max(0, maxInt - minInt);
                int value = minInt + static_cast<int>(std::round(norm * span));
                value = std::clamp(value, minInt, maxInt);
                return static_cast<float>(value);
            }
            case ParamType::BOOL:
                return norm >= 0.5f ? 1.0f : 0.0f;
        }
        return min_val;
    }

    float normalize(float value) const {
        switch (type) {
            case ParamType::FLOAT: {
                float clampedValue = std::clamp(value, min_val, max_val);
                if (curve == ParamCurve::Logarithmic) {
                    float safeMin = std::max(min_val, std::numeric_limits<float>::min());
                    float safeMax = std::max(max_val, safeMin + std::numeric_limits<float>::epsilon());
                    float logMin = std::log(safeMin);
                    float logMax = std::log(safeMax);
                    if (logMax - logMin <= std::numeric_limits<float>::epsilon()) {
                        return 0.0f;
                    }
                    float logValue = std::log(std::max(clampedValue, safeMin));
                    return std::clamp((logValue - logMin) / (logMax - logMin), 0.0f, 1.0f);
                }
                if (max_val - min_val <= std::numeric_limits<float>::epsilon()) {
                    return 0.0f;
                }
                return std::clamp((clampedValue - min_val) / (max_val - min_val), 0.0f, 1.0f);
            }
            case ParamType::INT:
            case ParamType::ENUM: {
                int minInt = static_cast<int>(std::round(min_val));
                int maxInt = static_cast<int>(std::round(max_val));
                int span = std::max(1, maxInt - minInt);
                int clamped = std::clamp(static_cast<int>(std::round(value)), minInt, maxInt);
                return std::clamp(static_cast<float>(clamped - minInt) / static_cast<float>(span), 0.0f, 1.0f);
            }
            case ParamType::BOOL:
                return value > 0.5f ? 1.0f : 0.0f;
        }
        return 0.0f;
    }
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
    std::atomic<int> filterType{0};  // 0=LP, 1=HP, 2=HighShelf, 3=LowShelf, 4=Ladder, 5=Diode, 6=LadderBP, 7=DualNotch, 8=BP2
    std::atomic<float> filterCutoff{1000.0f};
    std::atomic<float> filterGain{0.0f};  // For shelf filters (dB)
    std::atomic<float> filterResonance{0.4f};
    std::atomic<float> filterDrive{1.0f};
    std::atomic<float> filterFeedbackHP{200.0f};
    std::atomic<float> filterSpread{0.5f};
    std::atomic<float> filterNotchFeedback{0.3f};
    std::atomic<float> filterBandWidth{0.5f};
    std::atomic<float> filterDryWet{1.0f};  // Dry/wet mix for notch filter

    // Compressor/Limiter parameters
    std::atomic<bool> compressorEnabled{false};
    std::atomic<float> compressorThreshold{-20.0f};  // dB (-60 to 0)
    std::atomic<float> compressorRatio{4.0f};         // ratio (1 to 20, or 100 for limiting)
    std::atomic<float> compressorAttack{5.0f};        // ms (0.1 to 100)
    std::atomic<float> compressorRelease{50.0f};      // ms (10 to 1000)
    std::atomic<float> compressorKnee{6.0f};          // dB (0 to 20)
    std::atomic<float> compressorMix{1.0f};           // Dry/wet (0 to 1)
    std::atomic<bool> compressorAutoMakeup{true};     // Automatic makeup gain
    std::atomic<float> compressorManualMakeup{0.0f};  // Manual makeup gain in dB (0 to 24)
    std::atomic<bool> compressorRMS{false};           // Detection mode: false=Peak, true=RMS

    // Generic MIDI CC Learn for new parameter system
    std::atomic<bool> midiLearnActive{false};
    std::atomic<int> midiLearnParameterId{-1};  // Which parameter ID to learn (-1 = none)
    std::atomic<double> midiLearnStartTime{0.0};  // Timestamp when MIDI learn started

    // MIDI CC mappings for all parameters (parameter ID -> CC number, -1 means not mapped)
    static constexpr int kMaxParamMap = 1024;
    std::atomic<int> parameterCCMap[kMaxParamMap];  // Broad range for parameter IDs
    // Context storage for parameters (which osc/LFO/env/sampler/chaos instance)
    std::atomic<int> parameterContextOsc[kMaxParamMap];
    std::atomic<int> parameterContextLFO[kMaxParamMap];
    std::atomic<int> parameterContextEnv[kMaxParamMap];
    std::atomic<int> parameterContextSampler[kMaxParamMap];
    std::atomic<int> parameterContextChaos[kMaxParamMap];

    // Legacy - keep for backwards compatibility
    std::atomic<int> filterCutoffCC{-1};  // Which CC controls filter cutoff (-1 = none)
    
    // Legacy MIDI learn for compatibility
    std::atomic<bool> ccLearnMode{false};
    std::atomic<int> ccLearnTarget{-1};  // Which parameter to learn (-1 = none)
    
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
    std::atomic<float> osc1Level{0.8f};        // Mix level (static, 0.0-1.0)

    // Oscillator 2 (index 1)
    std::atomic<int> osc2Mode{1};
    std::atomic<float> osc2Freq{440.0f};
    std::atomic<float> osc2Morph{0.5f};
    std::atomic<int> osc2Shape{0};
    std::atomic<float> osc2Duty{0.5f};
    std::atomic<float> osc2Ratio{1.0f};
    std::atomic<float> osc2Offset{0.0f};
    std::atomic<float> osc2Amp{1.0f};
    std::atomic<float> osc2Level{0.8f};

    // Oscillator 3 (index 2)
    std::atomic<int> osc3Mode{1};
    std::atomic<float> osc3Freq{440.0f};
    std::atomic<float> osc3Morph{0.5f};
    std::atomic<int> osc3Shape{0};
    std::atomic<float> osc3Duty{0.5f};
    std::atomic<float> osc3Ratio{1.0f};
    std::atomic<float> osc3Offset{0.0f};
    std::atomic<float> osc3Amp{1.0f};
    std::atomic<float> osc3Level{0.8f};

    // Oscillator 4 (index 3)
    std::atomic<int> osc4Mode{1};
    std::atomic<float> osc4Freq{440.0f};
    std::atomic<float> osc4Morph{0.5f};
    std::atomic<int> osc4Shape{0};
    std::atomic<float> osc4Duty{0.5f};
    std::atomic<float> osc4Ratio{1.0f};
    std::atomic<float> osc4Offset{0.0f};
    std::atomic<float> osc4Amp{1.0f};
    std::atomic<float> osc4Level{0.8f};

    // Mixer mute/solo state (4 OSC + 4 SAMP + 4 CHAOS = 12 channels)
    std::atomic<bool> oscMuted[4]{false, true, true, true};     // OSC 1 unmuted, others muted by default
    std::atomic<bool> oscSolo[4]{false, false, false, false};
    std::atomic<bool> samplerMuted[4]{true, true, true, true};  // All samplers muted by default
    std::atomic<bool> samplerSolo[4]{false, false, false, false};
    std::atomic<bool> chaosMuted[4]{true, true, true, true};    // All chaos muted by default
    std::atomic<bool> chaosSolo[4]{false, false, false, false};
    std::atomic<float> chaosLevel[4]{0.8f, 0.8f, 0.8f, 0.8f};   // Chaos mix levels

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

    // Chaos mixer level helpers
    float getChaosLevel(int index) const {
        if (index < 0 || index >= 4) return 0.0f;
        return chaosLevel[index].load();
    }
    void setChaosLevel(int index, float value) {
        if (index < 0 || index >= 4) return;
        chaosLevel[index] = std::clamp(value, 0.0f, 1.0f);
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
    std::atomic<float> chaos1Parameter{0.918f};    // u parameter (0.6-0.99, 0.918 is typical)
    std::atomic<float> chaos1ClockFreq{1.0f};      // Clock frequency in Hz (0.0001-20000)
    std::atomic<int> chaos1InterpMode{0};          // 0=LINEAR, 1=CUBIC, 2=HOLD
    std::atomic<bool> chaos1FastMode{false};       // Fast (per-sample) vs slow (per-block)
    std::atomic<bool> chaos1Running{true};         // Run/stop state
    std::atomic<float> chaos1VisualX{0.0f};        // Current X for visualization
    std::atomic<float> chaos1VisualY{0.0f};        // Current Y for visualization

    std::atomic<float> chaos2Parameter{0.918f};
    std::atomic<float> chaos2ClockFreq{1.0f};
    std::atomic<int> chaos2InterpMode{0};
    std::atomic<bool> chaos2FastMode{false};
    std::atomic<bool> chaos2Running{true};
    std::atomic<float> chaos2VisualX{0.0f};
    std::atomic<float> chaos2VisualY{0.0f};

    std::atomic<float> chaos3Parameter{0.918f};
    std::atomic<float> chaos3ClockFreq{1.0f};
    std::atomic<int> chaos3InterpMode{0};
    std::atomic<bool> chaos3FastMode{false};
    std::atomic<bool> chaos3Running{true};
    std::atomic<float> chaos3VisualX{0.0f};
    std::atomic<float> chaos3VisualY{0.0f};

    std::atomic<float> chaos4Parameter{0.918f};
    std::atomic<float> chaos4ClockFreq{1.0f};
    std::atomic<int> chaos4InterpMode{0};
    std::atomic<bool> chaos4FastMode{false};
    std::atomic<bool> chaos4Running{true};
    std::atomic<float> chaos4VisualX{0.0f};
    std::atomic<float> chaos4VisualY{0.0f};

    // Chaos global diff-mix toggle (mix X/Y deltas for a whiter sound)
    std::atomic<bool> chaosDiff{false};

    // FM Matrix - audio-rate frequency modulation routing
    // fmMatrix[target][source] = depth (-0.99 to +0.99)
    // Example: fmMatrix[2][0] = 0.5 means OSC1 modulates OSC3 at 50% depth
    // Targets: OSC1-4 (0-3), SAMP1-4 (4-7)
    // Sources: OSC1-4 (0-3), SAMP1-4 (4-7)
    std::atomic<float> fmMatrix[kFMTargetCount][kFMSourceCount];
    std::atomic<float> fmGlobalDepth{1.0f};  // Global FM depth scalar (0.0-1.0)

    // Constructor to initialize CC map and FM matrix
    SynthParameters() {
        for (int i = 0; i < kMaxParamMap; ++i) {
            parameterCCMap[i] = -1;  // -1 means no CC assigned
            parameterContextOsc[i] = -1;
            parameterContextLFO[i] = -1;
            parameterContextEnv[i] = -1;
            parameterContextSampler[i] = -1;
            parameterContextChaos[i] = -1;
        }
        // Initialize FM matrix to zero (no FM routing by default)
        for (int target = 0; target < kFMTargetCount; ++target) {
            for (int source = 0; source < kFMSourceCount; ++source) {
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

    void applyParameterValue(int id,
                             float value,
                             Synth* synth,
                             int oscIndex,
                             int lfoIndex,
                             int envIndex,
                             int samplerIndex,
                             int chaosIndex);

    // Chaos getters/setters
    float getChaosParameter(int index) const {
        switch (index) {
            case 0: return chaos1Parameter.load();
            case 1: return chaos2Parameter.load();
            case 2: return chaos3Parameter.load();
            case 3: return chaos4Parameter.load();
            default: return chaos1Parameter.load();
        }
    }

    void setChaosParameter(int index, float value) {
        switch (index) {
            case 0: chaos1Parameter = value; break;
            case 1: chaos2Parameter = value; break;
            case 2: chaos3Parameter = value; break;
            case 3: chaos4Parameter = value; break;
            default: chaos1Parameter = value; break;
        }
    }

    float getChaosClockFreq(int index) const {
        switch (index) {
            case 0: return chaos1ClockFreq.load();
            case 1: return chaos2ClockFreq.load();
            case 2: return chaos3ClockFreq.load();
            case 3: return chaos4ClockFreq.load();
            default: return chaos1ClockFreq.load();
        }
    }

    void setChaosClockFreq(int index, float value) {
        switch (index) {
            case 0: chaos1ClockFreq = value; break;
            case 1: chaos2ClockFreq = value; break;
            case 2: chaos3ClockFreq = value; break;
            case 3: chaos4ClockFreq = value; break;
            default: chaos1ClockFreq = value; break;
        }
    }

    bool getChaosFastMode(int index) const {
        switch (index) {
            case 0: return chaos1FastMode.load();
            case 1: return chaos2FastMode.load();
            case 2: return chaos3FastMode.load();
            case 3: return chaos4FastMode.load();
            default: return chaos1FastMode.load();
        }
    }

    void setChaosFastMode(int index, bool value) {
        switch (index) {
            case 0: chaos1FastMode = value; break;
            case 1: chaos2FastMode = value; break;
            case 2: chaos3FastMode = value; break;
            case 3: chaos4FastMode = value; break;
            default: chaos1FastMode = value; break;
        }
    }

    int getChaosInterpMode(int index) const {
        switch (index) {
            case 0: return chaos1InterpMode.load();
            case 1: return chaos2InterpMode.load();
            case 2: return chaos3InterpMode.load();
            case 3: return chaos4InterpMode.load();
            default: return chaos1InterpMode.load();
        }
    }

    void setChaosInterpMode(int index, int value) {
        switch (index) {
            case 0: chaos1InterpMode = value; break;
            case 1: chaos2InterpMode = value; break;
            case 2: chaos3InterpMode = value; break;
            case 3: chaos4InterpMode = value; break;
            default: chaos1InterpMode = value; break;
        }
    }

    bool getChaosRunning(int index) const {
        switch (index) {
            case 0: return chaos1Running.load();
            case 1: return chaos2Running.load();
            case 2: return chaos3Running.load();
            case 3: return chaos4Running.load();
            default: return chaos1Running.load();
        }
    }

    void setChaosRunning(int index, bool value) {
        switch (index) {
            case 0: chaos1Running = value; break;
            case 1: chaos2Running = value; break;
            case 2: chaos3Running = value; break;
            case 3: chaos4Running = value; break;
            default: chaos1Running = value; break;
        }
    }

    // FM Matrix accessors (8×8: OSC1-4 are 0-3, SAMP1-4 are 4-7)
    float getFMDepth(int target, int source) const {
        if (target < 0 || target >= kFMTargetCount ||
            source < 0 || source >= kFMSourceCount) {
            return 0.0f;
        }
        return fmMatrix[target][source].load();
    }

    void setFMDepth(int target, int source, float depth) {
        if (target < 0 || target >= kFMTargetCount ||
            source < 0 || source >= kFMSourceCount) {
            return;
        }
        const float clamped = std::max(-0.99f, std::min(0.99f, depth));
        fmMatrix[target][source] = clamped;
    }
};

namespace ParameterRegistry {
    void Clear();
    void Register(const InlineParameter& param);
    const InlineParameter* Lookup(int id);
}

enum class UIPage {
    MAIN,
    OSCILLATOR,
    SAMPLER,
    MIXER,
    LFO,
    ENV,
    FM,
    MOD,
    REVERB,
    FILTER,
    COMPRESSOR,
    SEQUENCER,
    CHAOS,
    CONFIG,
    DEBUG
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
        PHASE_SOURCE
    };
    
    
    // Set device info
    void setDeviceInfo(const std::string& audioDevice, int sampleRate, int bufferSize,
                       const std::string& midiDevice, int midiPort);
    
    // Set available devices
    void setAvailableAudioDevices(const std::vector<std::pair<int, std::string>>& devices, int currentDeviceId);
    void setAvailableMidiDevices(const std::vector<std::pair<int, std::string>>& devices, int currentPort);
    void setSampleDirectory(const std::string& path);
    std::string getSampleDirectory() const { return sampleBrowserCurrentDir; }
    void setAudioUnderrunCount(uint64_t count) { audioUnderrunCount.store(count, std::memory_order_relaxed); }
    uint64_t getAudioUnderrunCount() const { return audioUnderrunCount.load(std::memory_order_relaxed); }
    void updateAudioDebugStats(uint32_t durationMicros, float peak, uint32_t activeVoices);
    void resetAudioDebugStats();
    
    void addConsoleMessage(const std::string& message);

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
    void refreshPresetList();
    
    // Device change request (returns true if restart requested)
    bool isDeviceChangeRequested() const { return deviceChangeRequested; }
    int getRequestedAudioDevice() const { return requestedAudioDeviceId; }
    int getRequestedMidiDevice() const { return requestedMidiPortNum; }
    void clearDeviceChangeRequest() { deviceChangeRequested = false; }
    // Audio buffer size request
    void requestAudioBufferSizeChange(int newSize);
    bool isBufferSizeChangeRequested() const { return bufferSizeChangeRequested; }
    int getRequestedBufferSize() const { return requestedBufferSize; }
    void clearBufferSizeChangeRequest() { bufferSizeChangeRequested = false; requestedBufferSize = -1; }
    int getCurrentAudioDeviceId() const { return currentAudioDeviceId; }
    int getCurrentMidiPort() const { return currentMidiPortNum; }
    int getCurrentAudioBufferSize() const { return audioBufferSize; }

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
    std::atomic<uint64_t> audioUnderrunCount{0};
    struct AudioDebugState {
        std::atomic<uint32_t> currentMicros{0};
        std::atomic<uint32_t> minMicros{std::numeric_limits<uint32_t>::max()};
        std::atomic<uint32_t> maxMicros{0};
        std::atomic<float> currentPeak{0.0f};
        std::atomic<float> maxPeak{0.0f};
        std::atomic<uint32_t> currentVoices{0};
        std::atomic<uint32_t> maxVoices{0};
    } audioDebugState;
    
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
    int presetListIndex;
    int presetListScroll;
    
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
    bool bufferSizeChangeRequested;
    int requestedBufferSize;
    std::vector<int> bufferSizeOptions;

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
    void drawParameterList(int startRow, int startCol, const std::vector<int>& paramIds);
    void drawOscillatorPage();
    void drawSamplerPage();
    void drawMixerPage();
    void drawLFOPage();
    void drawEnvelopePage();
    void drawFMPage();
    void drawModPage();
    void drawReverbPage();
    void drawFilterPage();
    void drawCompressorPage();
    void drawMainPage();
    void drawSequencerPage();
    void drawChaosPage();
    void drawConfigPage();
    void drawDebugPage();
    void cycleAudioBufferSize(int direction);
    void drawBar(int y, int x, const char* label, float value, float min, float max, int width);
    void drawHotkeyLine();
    void drawStatusMessage();
    void drawOscillatorWavePreview(int topRow, int leftCol, int plotHeight, int plotWidth);
    void drawLFOWavePreview(int topRow, int leftCol, int plotHeight, int plotWidth, int lfoIndex, float phase);
    void drawEnvelopePreview(int topRow, int leftCol, int plotHeight, int plotWidth);
    void drawChaosVisualization(int topRow, int leftCol, int plotHeight, int plotWidth, int chaosIndex);

    // Sequencer helpers
    bool handleSequencerInput(int ch);
    void adjustSequencerTrackerField(int row, int column, int direction, bool fine);
    void adjustSequencerInfoField(int infoIndex, int direction, bool fine);
    void executeSequencerAction(int actionRow, int actionColumn);
    void startSequencerNumericInput(int row, int column);
    void startSequencerInfoNumericInput(int infoIndex);
    void applySequencerNumericInput(const std::string& text);
    void cancelSequencerNumericInput();
    void startSequencerScaleMenu();
    void handleSequencerScaleMenuInput(int ch);
    void finishSequencerScaleMenu(bool applySelection);
    void ensureSequencerSelectionInRange();
    void undoAction();
    void redoAction();

public:
    // Returns true if a MIDI note was consumed to fill the sequencer note/name popup
    bool handleMidiNoteForSequencerNumeric(int midiNote);

    // Baseline reset to last loaded/saved preset values
    void resetToBaseline();

    // MOD matrix menu helpers
    void startModMatrixMenu();
    void handleModMatrixMenuInput(int ch);
    void finishModMatrixMenu(bool applySelection);
    void startModMatrixAmountInput();
    void finishModMatrixAmountInput();

    // Preset management helpers
    // refreshPresetList is declared public above; no duplicate here
    
    // Parameter management
    std::vector<InlineParameter> parameters;
    void initializeParameters();
    InlineParameter* getParameter(int id);
    std::vector<int> getParameterIdsForPage(UIPage page);
    std::vector<int> getFilterParameterIds() const;
    std::vector<int> getCompressorParameterIds() const;
    std::vector<int> getRandomizableParameterIds();
    void adjustParameter(int id, bool increase, bool fine);
    void setParameterValue(int id, float value);
    float getParameterValue(int id);
    std::string getParameterDisplayString(int id);
    void startNumericInput(int id);
    void finishNumericInput();
    // FM numeric input context
    bool numericInputIsFM = false;
    int fmNumericTarget = -1;
    int fmNumericSource = -1;
    void startMidiLearn(int id);
    void finishMidiLearn();
    bool isParameterModulated(int id);  // Check if parameter has active modulation
    void captureUndoSnapshot(const char* reason = nullptr);
    void resetUndoHistory();
    std::string serializeState();
    void applySerializedState(const std::string& data);

    std::vector<std::string> undoStack;
    std::vector<std::string> redoStack;
    bool undoCaptureSuppressed = false;
    static constexpr size_t kMaxUndoHistory = 64;

    // Global parameter operations (respect randomizable whitelist)
    void randomizeAllParameters(float amount01);
    void mutateAllParameters(float amount01);
    void resetAllParametersToNeutral();

    // Per-page parameter operations
    void randomizePageParameters(UIPage page, float amount01);
    void mutatePageParameters(UIPage page, float amount01);
    void resetPageParameters(UIPage page);
    void randomizeSingleParameter(int id, float amount01);
    void mutateSingleParameter(int id, float amount01);
    void randomizeCurrentEntity(UIPage page, float amount01);
    void mutateCurrentEntity(UIPage page, float amount01);
    void randomizeModSlot(int slotIndex, float amount01, bool capture = true);
    void mutateModSlot(int slotIndex, float amount01, bool capture = true);
    void randomizeAllModSlots(float amount01);
    void mutateAllModSlots(float amount01);

    // Oscillator/LFO/Envelope UI state
    int currentOscillatorIndex;  // 0-3: which oscillator is selected on OSCILLATOR page
    int currentSamplerIndex;     // 0-3: which sampler is selected on SAMPLER page
    int currentLFOIndex;          // 0-3: which LFO is selected on LFO page
    int currentEnvelopeIndex;     // 0-3: which envelope is selected on ENV page
    int currentChaosIndex;        // 0-3: which chaos generator is selected on CHAOS page

    // FM Matrix UI state
    int fmMatrixCursorRow;        // 0-15 sources
    int fmMatrixCursorCol;        // 0-(kFMTargetCount-1): targets
    bool fmMatrixLocked[kFMTargetCount][kFMSourceCount] = {{false}}; // target x source lock flags (set to all locked at runtime)

    // MOD Matrix UI state
    int modMatrixCursorRow;       // 0-15: modulation slot
    int modMatrixCursorCol;       // 0-4: column within slot (Source/Curve/Amount/Destination/Type)
    bool modSlotLocked[16] = {false};

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
    int sequencerScrollOffset = 0;   // Top row index for tracker view (for >16 steps)
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
    struct termios originalTermios{};
    bool hasOriginalTermios;
    std::string statusMessage;
    std::chrono::steady_clock::time_point statusMessageExpiry;

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

    // Main page action buttons state
    int mainPageActionIndex;         // 0=Save, 1=Load, 2=Randomize, 3=Mutate, 4=MutateAmount, 5=Reset, 6=CPU Monitor
    float globalMutatePercentage;    // 0-100%
    float globalRandomizePercentage; // 0-100%
    bool mainPageFocusLeft;          // true=left column (actions), false=right column (mixer)

    // Main page mixer state
    int mainPageMixerChannel;        // 0-11: 0-3=OSC, 4-7=SAMP, 8-11=CHAOS

    // Preset browser state (similar to sample browser)
    bool presetBrowserActive;
    std::vector<std::string> presetBrowserPresets;
    int presetBrowserSelectedIndex;
    int presetBrowserScrollOffset;

    // Preset browser helpers
    void startPresetBrowser();
    void handlePresetBrowserInput(int ch);
    void finishPresetBrowser(bool applySelection);
    void refreshPresetBrowserList();

    // Unified preset helpers
    std::string buildUnifiedPresetContent(const std::string& name);
    void applyUnifiedPresetContent(const std::string& fullContent);

    // Baseline content captured on load/save for global reset
    std::string baselinePresetContent;
    bool baselineAvailable = false;

    // Global Reset popup (choose baseline vs init defaults)
    bool globalResetPopupActive = false;
    int globalResetPopupIndex = 0; // 0=Preset Baseline, 1=Init Defaults
    void startGlobalResetPopup() { globalResetPopupActive = true; globalResetPopupIndex = 0; }
    void finishGlobalResetPopup(bool applySelection);
    void handleGlobalResetPopupInput(int ch);
};

#endif // UI_H
