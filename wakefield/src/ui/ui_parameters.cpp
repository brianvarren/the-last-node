#include "../ui.h"
#include "../synth.h"
#include "ui_mod_data.h"
#include "../fm_constants.h"
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {
    std::unordered_map<int, InlineParameter> gParameterRegistry;
    std::string trimWhitespace(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }
}

namespace ParameterRegistry {
void Clear() {
    gParameterRegistry.clear();
}

void Register(const InlineParameter& param) {
    gParameterRegistry[param.id] = param;
}

const InlineParameter* Lookup(int id) {
    auto it = gParameterRegistry.find(id);
    if (it == gParameterRegistry.end()) {
        return nullptr;
    }
    return &it->second;
}
} // namespace ParameterRegistry

/*
 * RANDOMIZATION WHITELIST POLICY
 *
 * The 'randomizable' flag determines which parameters can be affected by
 * global randomize and mutate operations.
 *
 * IMMUNE (randomizable = false):
 * - Master Gain (prevents sudden loud sounds)
 * - Mix levels (OSC/SAMP levels - prevents silencing active sources)
 * - Mute/Solo states (not in parameter list, but immune via separate handling)
 * - CPU Monitor toggle
 * - Special UI controls (sample selector)
 * - Filter/Reverb enabled states (prevents disabling core FX)
 * - Key modes (prevents changing between FREE/KEY unexpectedly)
 * - Chaos Running state (prevents stopping generators mid-performance)
 *
 * RANDOMIZABLE (randomizable = true):
 * - Sound design parameters (morph, duty, ratio, offset, amp)
 * - Effect parameters (reverb settings, filter settings)
 * - Modulation sources (LFO, envelope, chaos parameters)
 * - Sampler controls (loop points, tuning, xfade)
 * - FM depth
 */

void UI::initializeParameters() {
    parameters.clear();
    ParameterRegistry::Clear();

    // ============================================================================
    // GLOBAL SYNTH CONTROLS (accessible via MIDI, limited UI exposure)
    // ============================================================================
    parameters.push_back({1, ParamType::ENUM, "Waveform", "", 0, 3,
                          {"Sine", "Square", "Saw", "Triangle"}, true, static_cast<int>(UIPage::MAIN), true});
    parameters.push_back({2, ParamType::FLOAT, "Attack", "s", 0.001f, 30.0f, {},
                          true, static_cast<int>(UIPage::MAIN), true, ParamCurve::Logarithmic});
    parameters.push_back({3, ParamType::FLOAT, "Decay", "s", 0.001f, 30.0f, {},
                          true, static_cast<int>(UIPage::MAIN), true, ParamCurve::Logarithmic});
    parameters.push_back({4, ParamType::FLOAT, "Sustain", "", 0.0f, 1.0f, {},
                          true, static_cast<int>(UIPage::MAIN), true});
    parameters.push_back({5, ParamType::FLOAT, "Release", "s", 0.001f, 30.0f, {},
                          true, static_cast<int>(UIPage::MAIN), true, ParamCurve::Logarithmic});

    // ============================================================================
    // MAIN PAGE - IMMUNE (prevent volume disasters)
    // ============================================================================
    parameters.push_back({6, ParamType::FLOAT, "Master Gain", "", 0.0f, 4.0f, {}, true, static_cast<int>(UIPage::MAIN), false});  // 0.0-4.0 (0dB to +12dB)

    // ============================================================================
    // OSCILLATOR PAGE - MOSTLY RANDOMIZABLE (except mode)
    // ============================================================================
    parameters.push_back({13, ParamType::ENUM, "Note Source", "", 0, 5, {"Free", "Ext MIDI", "Track 1", "Track 2", "Track 3", "Track 4"}, true, static_cast<int>(UIPage::OSCILLATOR), false});  // IMMUNE
    parameters.push_back({19, ParamType::ENUM, "Shape", "", 0, 1, {"Saw", "Pulse"}, true, static_cast<int>(UIPage::OSCILLATOR), true});
    parameters.push_back({11, ParamType::FLOAT, "Frequency", "Hz", 20.0f, 2000.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR), true, ParamCurve::Logarithmic});
    parameters.push_back({16, ParamType::INT, "Octave", "", -5, 5, {}, true, static_cast<int>(UIPage::OSCILLATOR), true});
    parameters.push_back({12, ParamType::FLOAT, "Morph", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR), true});
    parameters.push_back({14, ParamType::FLOAT, "Ratio", "", 0.125f, 16.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR), true, ParamCurve::Logarithmic});
    parameters.push_back({15, ParamType::FLOAT, "Offset", "Hz", -1000.0f, 1000.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR), true});
    parameters.push_back({18, ParamType::FLOAT, "Amp", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR), true});

    // ============================================================================
    // LFO PAGE - ALL RANDOMIZABLE (modulation sources)
    // ============================================================================
    parameters.push_back({200, ParamType::FLOAT, "Period", "s", 0.1f, 1800.0f, {}, true, static_cast<int>(UIPage::LFO), true, ParamCurve::Logarithmic});
    parameters.push_back({201, ParamType::ENUM, "Sync", "", 0, 3, {"Off", "On", "Trip", "Dot"}, true, static_cast<int>(UIPage::LFO), true});
    parameters.push_back({206, ParamType::ENUM, "Shape", "", 0, 1, {"Saw", "Pulse"}, true, static_cast<int>(UIPage::LFO), true});
    parameters.push_back({202, ParamType::FLOAT, "Morph", "", 0.0001f, 0.9999f, {}, true, static_cast<int>(UIPage::LFO), true});
    parameters.push_back({203, ParamType::FLOAT, "Duty", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::LFO), true});
    parameters.push_back({204, ParamType::BOOL, "Flip", "", 0, 1, {}, true, static_cast<int>(UIPage::LFO), true});
    parameters.push_back({205, ParamType::BOOL, "Reset On Note", "", 0, 1, {}, true, static_cast<int>(UIPage::LFO), true});

    // ============================================================================
    // REVERB PAGE - IMMUNE enabled state, randomizable settings
    // ============================================================================
    parameters.push_back({20, ParamType::ENUM, "Reverb Type", "", 0, 4, {"Greyhole", "Plate", "Room", "Hall", "Spring"}, true, static_cast<int>(UIPage::REVERB), true});
    parameters.push_back({21, ParamType::BOOL, "Reverb Enabled", "", 0, 1, {}, true, static_cast<int>(UIPage::REVERB), false});  // IMMUNE
    parameters.push_back({22, ParamType::FLOAT, "Delay Time", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::REVERB), true});
    parameters.push_back({23, ParamType::FLOAT, "Size", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::REVERB), true});
    parameters.push_back({24, ParamType::FLOAT, "Damping", "", 0.0f, 0.99f, {}, true, static_cast<int>(UIPage::REVERB), true});
    parameters.push_back({25, ParamType::FLOAT, "Mix", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::REVERB), true});
    parameters.push_back({26, ParamType::FLOAT, "Decay", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::REVERB), true});
    parameters.push_back({27, ParamType::FLOAT, "Diffusion", "", 0.0f, 0.99f, {}, true, static_cast<int>(UIPage::REVERB), true});
    parameters.push_back({28, ParamType::FLOAT, "Mod Depth", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::REVERB), true});
    parameters.push_back({29, ParamType::FLOAT, "Mod Freq", "", 0.0f, 10.0f, {}, true, static_cast<int>(UIPage::REVERB), true});

    // ============================================================================
    // FILTER PAGE - IMMUNE enabled state, randomizable settings
    // ============================================================================
    parameters.push_back({30, ParamType::ENUM, "Filter Type", "", 0, 9,
                          {"Lowpass", "Highpass", "High Shelf", "Low Shelf",
                           "Ladder LP", "Diode LP", "Ladder BP", "Dual Notch",
                           "Bandpass 2P", "Ladder BP 8P"},
                          true, static_cast<int>(UIPage::FILTER), true});
    parameters.push_back({31, ParamType::BOOL, "Filter Enabled", "", 0, 1, {}, true, static_cast<int>(UIPage::FILTER), false});  // IMMUNE
    parameters.push_back({32, ParamType::FLOAT, "Cutoff", "Hz", 20.0f, 20000.0f, {}, true, static_cast<int>(UIPage::FILTER), true, ParamCurve::Logarithmic});
    parameters.push_back({33, ParamType::FLOAT, "Gain", "dB", -24.0f, 24.0f, {}, true, static_cast<int>(UIPage::FILTER), true});
    parameters.push_back({34, ParamType::FLOAT, "Resonance", "", 0.0f, 1.2f, {}, true, static_cast<int>(UIPage::FILTER), true});
    parameters.push_back({35, ParamType::FLOAT, "Drive", "", 0.1f, 15.0f, {}, true, static_cast<int>(UIPage::FILTER), true});
    parameters.push_back({36, ParamType::FLOAT, "FB HP", "Hz", 10.0f, 6000.0f, {}, true, static_cast<int>(UIPage::FILTER), true, ParamCurve::Logarithmic});
    parameters.push_back({37, ParamType::FLOAT, "Spread", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::FILTER), true});
    parameters.push_back({38, ParamType::FLOAT, "Notch FB", "", 0.0f, 0.95f, {}, true, static_cast<int>(UIPage::FILTER), true});
    parameters.push_back({39, ParamType::FLOAT, "Width", "", 0.05f, 0.95f, {}, true, static_cast<int>(UIPage::FILTER), true});
    parameters.push_back({42, ParamType::FLOAT, "Dry/Wet", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::FILTER), true});

    // ============================================================================
    // MIXER PAGE - ALL IMMUNE (prevents silencing sources)
    // ============================================================================
    parameters.push_back({50, ParamType::FLOAT, "OSC 1 Level", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MIXER), false});  // IMMUNE
    parameters.push_back({51, ParamType::FLOAT, "OSC 2 Level", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MIXER), false});  // IMMUNE
    parameters.push_back({52, ParamType::FLOAT, "OSC 3 Level", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MIXER), false});  // IMMUNE
    parameters.push_back({53, ParamType::FLOAT, "OSC 4 Level", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MIXER), false});  // IMMUNE
    parameters.push_back({54, ParamType::FLOAT, "SAMP 1 Level", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MIXER), false});  // IMMUNE
    parameters.push_back({55, ParamType::FLOAT, "SAMP 2 Level", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MIXER), false});  // IMMUNE
    parameters.push_back({56, ParamType::FLOAT, "SAMP 3 Level", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MIXER), false});  // IMMUNE
    parameters.push_back({57, ParamType::FLOAT, "SAMP 4 Level", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MIXER), false});  // IMMUNE

    // ============================================================================
    // SAMPLER PAGE - MOSTLY RANDOMIZABLE (except key mode and sample selector)
    // ============================================================================
    parameters.push_back({69, ParamType::ENUM, "Sample", "", 0, 0, {}, false, static_cast<int>(UIPage::SAMPLER), false});  // IMMUNE (special)
    parameters.push_back({60, ParamType::ENUM, "Key Mode", "", 0, 1, {"KEY", "FREE"}, true, static_cast<int>(UIPage::SAMPLER), false});  // IMMUNE (deprecated, use Note Source)
    parameters.push_back({80, ParamType::ENUM, "Note Source", "", 0, 5, {"Free", "Ext MIDI", "Track 1", "Track 2", "Track 3", "Track 4"}, true, static_cast<int>(UIPage::SAMPLER), false});  // IMMUNE
    parameters.push_back({68, ParamType::ENUM, "Direction", "", 0, 2, {"Forward", "Reverse", "Ping-Pong"}, true, static_cast<int>(UIPage::SAMPLER), true});
    parameters.push_back({61, ParamType::FLOAT, "Loop Start", "%", 0.0f, 100.0f, {}, true, static_cast<int>(UIPage::SAMPLER), true});
    parameters.push_back({62, ParamType::FLOAT, "Loop Length", "%", 0.0f, 100.0f, {}, true, static_cast<int>(UIPage::SAMPLER), true});
    parameters.push_back({63, ParamType::FLOAT, "Xfade", "%", 0.0f, 100.0f, {}, true, static_cast<int>(UIPage::SAMPLER), true});
    parameters.push_back({64, ParamType::INT, "Octave", "", -5, 5, {}, true, static_cast<int>(UIPage::SAMPLER), true});
    parameters.push_back({65, ParamType::FLOAT, "Tune", "st", -60.0f, 67.0f, {}, true, static_cast<int>(UIPage::SAMPLER), true});
    parameters.push_back({66, ParamType::ENUM, "Sync", "", 0, 3, {"Off", "On", "Trip", "Dot"}, true, static_cast<int>(UIPage::SAMPLER), true});
    parameters.push_back({67, ParamType::BOOL, "Note Reset", "", 0, 1, {}, true, static_cast<int>(UIPage::SAMPLER), true});

    // ============================================================================
    // CONFIG PAGE - IMMUNE (UI settings)
    // ============================================================================
    parameters.push_back({400, ParamType::BOOL, "CPU Monitor", "", 0, 1, {}, false, static_cast<int>(UIPage::CONFIG), false});  // IMMUNE

    // ============================================================================
    // ENVELOPE PAGE - ALL RANDOMIZABLE (modulation sources)
    // ============================================================================
    // Envelope 1: 300-305
    parameters.push_back({300, ParamType::FLOAT, "Attack", "s", 0.001f, 600.0f, {}, true, static_cast<int>(UIPage::ENV), true, ParamCurve::Logarithmic});
    parameters.push_back({301, ParamType::FLOAT, "Decay", "s", 0.001f, 600.0f, {}, true, static_cast<int>(UIPage::ENV), true, ParamCurve::Logarithmic});
    parameters.push_back({302, ParamType::FLOAT, "Sustain", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({303, ParamType::FLOAT, "Release", "s", 0.001f, 600.0f, {}, true, static_cast<int>(UIPage::ENV), true, ParamCurve::Logarithmic});
    parameters.push_back({304, ParamType::FLOAT, "Atk Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({305, ParamType::FLOAT, "Rel Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    // Envelope 2: 306-311
    parameters.push_back({306, ParamType::FLOAT, "Attack", "s", 0.001f, 600.0f, {}, true, static_cast<int>(UIPage::ENV), true, ParamCurve::Logarithmic});
    parameters.push_back({307, ParamType::FLOAT, "Decay", "s", 0.001f, 600.0f, {}, true, static_cast<int>(UIPage::ENV), true, ParamCurve::Logarithmic});
    parameters.push_back({308, ParamType::FLOAT, "Sustain", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({309, ParamType::FLOAT, "Release", "s", 0.001f, 600.0f, {}, true, static_cast<int>(UIPage::ENV), true, ParamCurve::Logarithmic});
    parameters.push_back({310, ParamType::FLOAT, "Atk Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({311, ParamType::FLOAT, "Rel Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    // Envelope 3: 312-317
    parameters.push_back({312, ParamType::FLOAT, "Attack", "s", 0.001f, 600.0f, {}, true, static_cast<int>(UIPage::ENV), true, ParamCurve::Logarithmic});
    parameters.push_back({313, ParamType::FLOAT, "Decay", "s", 0.001f, 600.0f, {}, true, static_cast<int>(UIPage::ENV), true, ParamCurve::Logarithmic});
    parameters.push_back({314, ParamType::FLOAT, "Sustain", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({315, ParamType::FLOAT, "Release", "s", 0.001f, 600.0f, {}, true, static_cast<int>(UIPage::ENV), true, ParamCurve::Logarithmic});
    parameters.push_back({316, ParamType::FLOAT, "Atk Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({317, ParamType::FLOAT, "Rel Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    // Envelope 4: 318-323
    parameters.push_back({318, ParamType::FLOAT, "Attack", "s", 0.001f, 600.0f, {}, true, static_cast<int>(UIPage::ENV), true, ParamCurve::Logarithmic});
    parameters.push_back({319, ParamType::FLOAT, "Decay", "s", 0.001f, 600.0f, {}, true, static_cast<int>(UIPage::ENV), true, ParamCurve::Logarithmic});
    parameters.push_back({320, ParamType::FLOAT, "Sustain", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({321, ParamType::FLOAT, "Release", "s", 0.001f, 600.0f, {}, true, static_cast<int>(UIPage::ENV), true, ParamCurve::Logarithmic});
    parameters.push_back({322, ParamType::FLOAT, "Atk Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({323, ParamType::FLOAT, "Rel Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});

    // ============================================================================
    // CHAOS PAGE - RANDOMIZABLE (except running state)
    // ============================================================================
    parameters.push_back({350, ParamType::FLOAT, "Chaos", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::CHAOS), true});
    parameters.push_back({351, ParamType::FLOAT, "Clock", "Hz", 0.0001f, 20000.0f, {}, true, static_cast<int>(UIPage::CHAOS), true, ParamCurve::Logarithmic});
    parameters.push_back({352, ParamType::ENUM, "Interp", "", 0, 2, {"LINEAR", "CUBIC", "HOLD"}, true, static_cast<int>(UIPage::CHAOS), true});
    parameters.push_back({353, ParamType::BOOL, "Running", "", 0, 1, {}, false, static_cast<int>(UIPage::CHAOS), false});  // IMMUNE
    parameters.push_back({354, ParamType::BOOL, "FAST", "", 0, 1, {}, true, static_cast<int>(UIPage::CHAOS), true});
    parameters.push_back({355, ParamType::BOOL, "DIFF", "", 0, 1, {}, true, static_cast<int>(UIPage::CHAOS), true});

    // ============================================================================
    // FM PAGE - RANDOMIZABLE
    // ============================================================================
    parameters.push_back({360, ParamType::FLOAT, "Global Depth", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::FM), true});

    // ============================================================================
    // MIXER PAGE - IMMUNE (mixer levels shouldn't be randomized)
    // (OSC 1-4 Level: 50-53, SAMP 1-4 Level: 54-57 defined elsewhere)
    parameters.push_back({410, ParamType::FLOAT, "CHAOS 1 Level", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MIXER), false});
    parameters.push_back({411, ParamType::FLOAT, "CHAOS 2 Level", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MIXER), false});
    parameters.push_back({412, ParamType::FLOAT, "CHAOS 3 Level", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MIXER), false});
    parameters.push_back({413, ParamType::FLOAT, "CHAOS 4 Level", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MIXER), false});

    // ============================================================================
    // COMPRESSOR PAGE - RANDOMIZABLE (except enabled)
    // ============================================================================
    parameters.push_back({70, ParamType::BOOL, "Enabled", "", 0, 1, {}, true, static_cast<int>(UIPage::COMPRESSOR), false});
    parameters.push_back({71, ParamType::FLOAT, "Threshold", "dB", -60.0f, 0.0f, {}, true, static_cast<int>(UIPage::COMPRESSOR), true});
    parameters.push_back({72, ParamType::FLOAT, "Ratio", ":1", 1.0f, 20.0f, {}, true, static_cast<int>(UIPage::COMPRESSOR), true});
    parameters.push_back({73, ParamType::FLOAT, "Attack", "ms", 0.1f, 100.0f, {}, true, static_cast<int>(UIPage::COMPRESSOR), true, ParamCurve::Logarithmic});
    parameters.push_back({74, ParamType::FLOAT, "Release", "ms", 10.0f, 1000.0f, {}, true, static_cast<int>(UIPage::COMPRESSOR), true, ParamCurve::Logarithmic});
    parameters.push_back({75, ParamType::FLOAT, "Knee", "dB", 0.0f, 20.0f, {}, true, static_cast<int>(UIPage::COMPRESSOR), true});
    parameters.push_back({76, ParamType::FLOAT, "Mix", "%", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::COMPRESSOR), true});
    parameters.push_back({77, ParamType::BOOL, "Auto Makeup", "", 0, 1, {}, true, static_cast<int>(UIPage::COMPRESSOR), false});
    parameters.push_back({79, ParamType::FLOAT, "Manual Makeup", "dB", 0.0f, 24.0f, {}, true, static_cast<int>(UIPage::COMPRESSOR), true});
    parameters.push_back({78, ParamType::BOOL, "RMS Mode", "", 0, 1, {}, true, static_cast<int>(UIPage::COMPRESSOR), false});

    for (const auto& param : parameters) {
        ParameterRegistry::Register(param);
    }
}


void SynthParameters::applyParameterValue(int id,
                                          float value,
                                          Synth* synth,
                                          int oscIndex,
                                          int lfoIndex,
                                          int envIndex,
                                          int samplerIndex,
                                          int chaosIndex) {
    auto clampIndex = [](int index, int maxCount) {
        if (maxCount <= 0) return 0;
        if (index < 0) return 0;
        if (index >= maxCount) return maxCount - 1;
        return index;
    };

    constexpr int kOscCount = 4;
    constexpr int kLfoCount = 4;
    constexpr int kEnvCount = 4;
    constexpr int kSamplerCount = 4;
    constexpr int kChaosCount = 4;

    const int osc = clampIndex(oscIndex, kOscCount);
    const int lfo = clampIndex(lfoIndex, kLfoCount);
    const int env = clampIndex(envIndex, kEnvCount);
    const int sampler = clampIndex(samplerIndex, kSamplerCount);
    const int chaos = clampIndex(chaosIndex, kChaosCount);

    switch (id) {
        case 1:
            waveform = static_cast<int>(std::round(value));
            break;
        case 2:
            attack = value;
            setEnvAttack(0, value);
            break;
        case 3:
            decay = value;
            setEnvDecay(0, value);
            break;
        case 4:
            sustain = value;
            setEnvSustain(0, value);
            break;
        case 5:
            release = value;
            setEnvRelease(0, value);
            break;
        case 6: masterVolume = value; break;
        case 13: setOscNoteSource(osc, static_cast<int>(value)); break;
        case 19: setOscShape(osc, static_cast<int>(value)); break;
        case 11: setOscFrequency(osc, value); break;
        case 16: if (synth) synth->setOscillatorOctave(osc, static_cast<int>(value)); break;
        case 12: setOscMorph(osc, value); break;
        case 14: setOscRatio(osc, value); break;
        case 15: setOscOffset(osc, value); break;
        case 18: setOscAmp(osc, value); break;
        case 50: setOscLevel(0, value); break;
        case 51: setOscLevel(1, value); break;
        case 52: setOscLevel(2, value); break;
        case 53: setOscLevel(3, value); break;
        case 54: if (synth) synth->setSamplerLevel(0, value); break;
        case 55: if (synth) synth->setSamplerLevel(1, value); break;
        case 56: if (synth) synth->setSamplerLevel(2, value); break;
        case 57: if (synth) synth->setSamplerLevel(3, value); break;
        case 410: setChaosLevel(0, value); break;
        case 411: setChaosLevel(1, value); break;
        case 412: setChaosLevel(2, value); break;
        case 413: setChaosLevel(3, value); break;
        case 69: break;
        case 60: if (synth) synth->setSamplerKeyMode(sampler, value < 0.5f); break;
        case 80: setSamplerNoteSource(sampler, static_cast<int>(value)); break;
        case 68: if (synth) synth->setSamplerPlaybackMode(sampler, static_cast<PlaybackMode>(static_cast<int>(value))); break;
        case 61: if (synth) synth->setSamplerLoopStart(sampler, value / 100.0f); break;
        case 62: if (synth) synth->setSamplerLoopLength(sampler, value / 100.0f); break;
        case 63: if (synth) synth->setSamplerCrossfadeLength(sampler, value / 100.0f); break;
        case 64: if (synth) synth->setSamplerOctave(sampler, static_cast<int>(value)); break;
        case 65: if (synth) synth->setSamplerTune(sampler, value); break;
        case 66: if (synth) synth->setSamplerSyncMode(sampler, static_cast<int>(value)); break;
        case 67: if (synth) synth->setSamplerNoteReset(sampler, value > 0.5f); break;
        case 200: setLfoPeriod(lfo, value); break;
        case 201: setLfoSyncMode(lfo, static_cast<int>(value)); break;
        case 202: setLfoMorph(lfo, value); break;
        case 203: setLfoDuty(lfo, value); break;
        case 204: setLfoFlip(lfo, value > 0.5f); break;
        case 205: setLfoResetOnNote(lfo, value > 0.5f); break;
        case 206: setLfoShape(lfo, static_cast<int>(value)); break;
        case 20: reverbType = static_cast<int>(value); break;
        case 21: reverbEnabled = (value > 0.5f); break;
        case 22: reverbDelayTime = value; break;
        case 23: reverbSize = value; break;
        case 24: reverbDamping = value; break;
        case 25: reverbMix = value; break;
        case 26: reverbDecay = value; break;
        case 27: reverbDiffusion = value; break;
        case 28: reverbModDepth = value; break;
        case 29: reverbModFreq = value; break;
        case 30: filterType = static_cast<int>(value); break;
        case 31: filterEnabled = (value > 0.5f); break;
        case 32: filterCutoff = value; break;
        case 33: filterGain = value; break;
        case 34: filterResonance = value; break;
        case 35: filterDrive = value; break;
        case 36: filterFeedbackHP = value; break;
        case 37: filterSpread = value; break;
        case 38: filterNotchFeedback = value; break;
        case 39: filterBandWidth = value; break;
        case 42: filterDryWet = value; break;

        // Compressor parameters (70-78)
        case 70: compressorEnabled = (value > 0.5f); break;
        case 71: compressorThreshold = value; break;
        case 72: compressorRatio = value; break;
        case 73: compressorAttack = value; break;
        case 74: compressorRelease = value; break;
        case 75: compressorKnee = value; break;
        case 76: compressorMix = value; break;
        case 77: compressorAutoMakeup = (value > 0.5f); break;
        case 79: compressorManualMakeup = value; break;
        case 78: compressorRMS = (value > 0.5f); break;
        case 300: setEnvAttack(0, value); attack = value; break;
        case 301: setEnvDecay(0, value); decay = value; break;
        case 302: setEnvSustain(0, value); sustain = value; break;
        case 303: setEnvRelease(0, value); release = value; break;
        case 304: setEnvAttackBend(0, value); break;
        case 305: setEnvReleaseBend(0, value); break;
        case 306: setEnvAttack(1, value); break;
        case 307: setEnvDecay(1, value); break;
        case 308: setEnvSustain(1, value); break;
        case 309: setEnvRelease(1, value); break;
        case 310: setEnvAttackBend(1, value); break;
        case 311: setEnvReleaseBend(1, value); break;
        case 312: setEnvAttack(2, value); break;
        case 313: setEnvDecay(2, value); break;
        case 314: setEnvSustain(2, value); break;
        case 315: setEnvRelease(2, value); break;
        case 316: setEnvAttackBend(2, value); break;
        case 317: setEnvReleaseBend(2, value); break;
        case 318: setEnvAttack(3, value); break;
        case 319: setEnvDecay(3, value); break;
        case 320: setEnvSustain(3, value); break;
        case 321: setEnvRelease(3, value); break;
        case 322: setEnvAttackBend(3, value); break;
        case 323: setEnvReleaseBend(3, value); break;
        case 350:
            setChaosParameter(chaos, value);
            if (synth) synth->setChaosParameter(chaos, value);
            break;
        case 351: {
            float clamped = std::clamp(value, 0.0001f, 20000.0f);
            setChaosClockFreq(chaos, clamped);
            if (synth) synth->setChaosClockFreq(chaos, clamped);
            break;
        }
        case 352:
            setChaosInterpMode(chaos, static_cast<int>(value));
            if (synth) synth->setChaosInterpMode(chaos, static_cast<int>(value));
            break;
        case 353: setChaosRunning(chaos, value > 0.5f); break;
        case 354:
            setChaosFastMode(chaos, value > 0.5f);
            if (synth) synth->setChaosFastMode(chaos, value > 0.5f);
            break;
        case 355:
            chaosDiff = (value > 0.5f);
            if (synth) synth->setChaosDiffMode(value > 0.5f);
            break;
        case 360: fmGlobalDepth.store(std::clamp(value, 0.0f, 1.0f)); break;
        default:
            break;
    }
}


InlineParameter* UI::getParameter(int id) {
    for (auto& param : parameters) {
        if (param.id == id) {
            return &param;
        }
    }
    return nullptr;
}

std::vector<int> UI::getParameterIdsForPage(UIPage page) {
    std::vector<int> ids;
    int pageInt = static_cast<int>(page);
    for (const auto& param : parameters) {
        if (param.page == pageInt) {
            ids.push_back(param.id);
        }
    }
    return ids;
}

std::vector<int> UI::getFilterParameterIds() const {
    std::vector<int> ids = {31, 30, 32};
    int type = params ? params->filterType.load() : 0;

    if (type == 2 || type == 3) {
        ids.push_back(33);
    }

    if (type >= 4) {
        ids.push_back(34);
    }

    if (type == 4 || type == 5) {
        ids.push_back(35);
        ids.push_back(36);
    } else if (type == 6 || type == 9) {
        ids.push_back(35);
        ids.push_back(39);
    } else if (type == 7) {
        ids.push_back(35);
        ids.push_back(37);
        ids.push_back(38);
        ids.push_back(42);
    } else if (type == 8) {
        ids.push_back(35);
        ids.push_back(39);
    }

    return ids;
}

std::vector<int> UI::getCompressorParameterIds() const {
    // IDs for compressor parameters:
    // 70 = Enabled
    // 71 = Threshold
    // 72 = Ratio
    // 73 = Attack
    // 74 = Release
    // 75 = Knee
    // 76 = Mix
    // 77 = Auto Makeup
    // 79 = Manual Makeup
    // 78 = RMS Mode
    return {70, 71, 72, 73, 74, 75, 76, 77, 79, 78};
}

std::vector<int> UI::getRandomizableParameterIds() {
    std::vector<int> ids;
    for (const auto& param : parameters) {
        if (param.randomizable) {
            ids.push_back(param.id);
        }
    }
    return ids;
}

// Helper to detect which selector index a parameter uses (if any)
static inline bool paramUsesOscIndex(int id) {
    switch (id) {
        case 11: case 12: case 13: case 14: case 15: case 16: case 18: case 19:
            return true;
        default: return false;
    }
}

static inline bool paramUsesLfoIndex(int id) {
    return (id >= 200 && id <= 206);
}

static inline bool paramUsesEnvIndex(int id) {
    return (id >= 300 && id <= 323);
}

static inline bool paramUsesSamplerIndex(int id) {
    // SAMPLER page (60-68, plus 80). 69 is a file picker placeholder; it's never randomizable.
    return ((id >= 60 && id <= 68) || id == 80);
}

static inline bool paramUsesChaosIndex(int id) {
    return (id >= 350 && id <= 353);
}

// Generate a random float in [a,b]
static inline float frand(float a, float b) {
    float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return a + (b - a) * r;
}

// Forward declarations for sequencer access
#include "../sequencer.h"
#include "../track.h"
#include "../constraint.h"
#include "../clock.h"
extern Sequencer* sequencer;

void UI::randomizeAllParameters(float amount01) {
    amount01 = std::clamp(amount01, 0.0f, 1.0f);
    if (amount01 <= 0.0f) return;
    captureUndoSnapshot("randomize_all");
    auto ids = getRandomizableParameterIds();
    for (int id : ids) {
        InlineParameter* p = getParameter(id);
        if (!p) continue;

        auto applyOne = [&](int count, auto setIndex, int saved) {
            for (int i = 0; i < count; ++i) {
                setIndex(i);
                switch (p->type) {
                    case ParamType::FLOAT: {
                        float cur = getParameterValue(id);
                        float rnd = frand(p->min_val, p->max_val);
                        float v = cur + (rnd - cur) * amount01;
                        setParameterValue(id, std::clamp(v, p->min_val, p->max_val));
                        break;
                    }
                    case ParamType::INT:
                    case ParamType::ENUM: {
                        int minI = static_cast<int>(p->min_val);
                        int maxI = static_cast<int>(p->max_val);
                        int span = std::max(0, maxI - minI);
                        float roll = frand(0.0f, 1.0f);
                        if (roll < amount01) {
                            int r = span == 0 ? minI : (minI + (rand() % (span + 1)));
                            setParameterValue(id, static_cast<float>(r));
                        }
                        break;
                    }
                    case ParamType::BOOL: {
                        float roll = frand(0.0f, 1.0f);
                        if (roll < amount01) {
                            bool cur = getParameterValue(id) > 0.5f;
                            setParameterValue(id, cur ? 0.0f : 1.0f);
                        }
                        break;
                    }
                }
            }
            setIndex(saved);
        };

        if (paramUsesOscIndex(id)) {
            int saved = currentOscillatorIndex; auto setIdx = [&](int v){ currentOscillatorIndex = v;};
            applyOne(4, setIdx, saved); continue;
        }
        if (paramUsesLfoIndex(id)) {
            int saved = currentLFOIndex; auto setIdx = [&](int v){ currentLFOIndex = v;};
            applyOne(4, setIdx, saved); continue;
        }
        if (paramUsesEnvIndex(id)) {
            int saved = currentEnvelopeIndex; auto setIdx = [&](int v){ currentEnvelopeIndex = v;};
            applyOne(4, setIdx, saved); continue;
        }
        if (paramUsesSamplerIndex(id)) {
            int saved = currentSamplerIndex; auto setIdx = [&](int v){ currentSamplerIndex = v;};
            applyOne(4, setIdx, saved); continue;
        }
        if (paramUsesChaosIndex(id)) {
            int saved = currentChaosIndex; auto setIdx = [&](int v){ currentChaosIndex = v;};
            applyOne(4, setIdx, saved); continue;
        }

        // Global param (no index)
        switch (p->type) {
            case ParamType::FLOAT: {
                float cur = getParameterValue(id);
                float rnd = frand(p->min_val, p->max_val);
                float v = cur + (rnd - cur) * amount01;
                setParameterValue(id, std::clamp(v, p->min_val, p->max_val));
                break;
            }
            case ParamType::INT:
            case ParamType::ENUM: {
                float roll = frand(0.0f, 1.0f);
                if (roll < amount01) {
                    int minI = static_cast<int>(p->min_val);
                    int maxI = static_cast<int>(p->max_val);
                    int span = std::max(0, maxI - minI);
                    int r = span == 0 ? minI : (minI + (rand() % (span + 1)));
                    setParameterValue(id, static_cast<float>(r));
                }
                break;
            }
            case ParamType::BOOL: {
                float roll = frand(0.0f, 1.0f);
                if (roll < amount01) {
                    bool cur = getParameterValue(id) > 0.5f;
                    setParameterValue(id, cur ? 0.0f : 1.0f);
                }
                break;
            }
        }
    }

    // Also randomize FM matrix depths (respect FM lock flags)
    for (int target = 0; target < kFMTargetCount; ++target) {
        for (int source = 0; source < kFMSourceCount; ++source) {
            if (fmMatrixLocked[target][source]) continue;
            float cur = params->getFMDepth(target, source);
            float rnd = frand(-0.99f, 0.99f);
            float v = cur + (rnd - cur) * amount01;
            params->setFMDepth(target, source, v);
        }
    }

    // Randomize modulation connections/amounts across all slots
    randomizeAllModSlots(amount01);

    // Randomize mixer levels (previously immune); scale towards random with amount
    for (int i = 0; i < 4; ++i) {
        float cur = params->getOscLevel(i);
        float rnd = frand(0.0f, 1.0f);
        params->setOscLevel(i, cur + (rnd - cur) * amount01);
    }
    if (synth) {
        for (int i = 0; i < 4; ++i) {
            float cur = synth->getSamplerLevel(i);
            float rnd = frand(0.0f, 1.0f);
            synth->setSamplerLevel(i, cur + (rnd - cur) * amount01);
        }
    }
    for (int i = 0; i < 4; ++i) {
        float cur = params->getChaosLevel(i);
        float rnd = frand(0.0f, 1.0f);
        params->setChaosLevel(i, cur + (rnd - cur) * amount01);
    }

    // Randomize selected samples on samplers
    if (synth) {
        SampleBank* bank = synth->getSampleBank();
        int count = bank ? bank->getSampleCount() : 0;

        // If none are loaded, try to load from default locations
        if (bank && count == 0) {
            std::vector<std::string> candidates;
            // Project-relative default fallbacks (same as startup resolution)
            candidates.emplace_back("samples");
            candidates.emplace_back("build/samples");
            candidates.emplace_back("../samples");
            candidates.emplace_back("../build/samples");
            candidates.emplace_back("../../samples");

            int totalLoaded = 0;
            std::string firstLoadedDir;
            for (const auto& dir : candidates) {
                int loaded = bank->loadSamplesFromDirectory(dir.c_str());
                if (loaded > 0 && firstLoadedDir.empty()) {
                    firstLoadedDir = dir;
                }
                totalLoaded += std::max(0, loaded);
            }
            if (totalLoaded > 0) {
                if (!firstLoadedDir.empty()) setSampleDirectory(firstLoadedDir);
                count = bank->getSampleCount();
            }
        }

        if (count > 0) {
            for (int i = 0; i < 4; ++i) {
                // With probability proportional to amount, pick a new sample
                if (frand(0.0f, 1.0f) < amount01) {
                    int idx = rand() % count;
                    synth->setSamplerSample(i, idx);
                }
            }
        }
    }

    // Randomize note sources for oscillators and samplers
    if (params && synth) {
        // Randomize oscillator note sources
        for (int i = 0; i < 4; ++i) {
            // Random note source: 0=Free, 1=Ext MIDI, 2-5=Track 1-4
            int noteSource = rand() % 6;
            params->setOscNoteSource(i, noteSource);
        }
        // Randomize sampler note sources
        for (int i = 0; i < 4; ++i) {
            // Random note source: 0=Free, 1=Ext MIDI, 2-5=Track 1-4
            int noteSource = rand() % 6;
            params->setSamplerNoteSource(i, noteSource);
        }
    }

    // Randomize sequencer parameters and regenerate patterns
    if (sequencer) {
        // Randomize tempo slightly
        double curTempo = sequencer->getTempo();
        double rndTempo = std::clamp(curTempo + (frand(-30.0f, 30.0f) * amount01), 40.0, 180.0);
        sequencer->setTempo(rndTempo);

        for (int t = 0; t < sequencer->getTrackCount(); ++t) {
            Track& track = sequencer->getTrack(t);
            Pattern& pattern = track.getPattern();
            MusicalConstraints& c = track.getConstraints();
            EuclideanPattern& eu = track.getEuclideanPattern();

            // Length: choose from {8, 16, 32}
            const int lengths[3] = {8, 16, 32};
            int newLen = lengths[rand() % 3];
            pattern.setLength(newLen);

            // Subdivision: pick among typical values
            const Subdivision subdivs[4] = {Subdivisions::EIGHTH, Subdivisions::SIXTEENTH, Subdivisions::THIRTYSECOND, Subdivisions::QUARTER};
            track.setSubdivision(subdivs[rand() % 4]);

            // Euclidean
            int steps = std::max(1, newLen);
            int hits = std::clamp(static_cast<int>(std::round(frand(0.0f, 1.0f) * steps)), 0, steps);
            int rot = (steps > 0) ? (rand() % steps) : 0;
            eu.setSteps(steps);
            eu.setHits(hits);
            eu.setRotation(rot);

            // Constraints
            c.setRootNote(rand() % 12);
            c.setScale(static_cast<Scale>(rand() % 10)); // Scale enum up to CUSTOM
            int octMin = rand() % 5; // 0..4
            int octMax = octMin + 2 + (rand() % 3); // ensure range
            c.setOctaveRange(octMin, std::min(octMax, 10));
            c.setDensity(std::clamp(frand(0.2f, 1.0f), 0.0f, 1.0f));
            c.setMaxInterval(1 + (rand() % 12));
            c.setContour(static_cast<Contour>(rand() % 5));
            c.setGravityNote((octMin + 1) * 12 + c.getRootNote());

            // Generate new pattern for this track
            track.generatePattern();
        }
    }
}

void UI::randomizeSingleParameter(int id, float amount01) {
    amount01 = std::clamp(amount01, 0.0f, 1.0f);
    InlineParameter* p = getParameter(id);
    if (!p || !p->randomizable) return;

    switch (p->type) {
        case ParamType::FLOAT: {
            float cur = getParameterValue(id);
            float rnd;

            // Special handling for oscillator frequency - use musical values
            if (id == 11) {
                // Convert to MIDI note for musical randomization
                // A4 (440Hz) = MIDI note 69
                // Random MIDI note from C2 (36) to C7 (96)
                int midiNote = 36 + (rand() % 61);  // 61 notes range (5 octaves)
                // Convert MIDI note to frequency: f = 440 * 2^((note-69)/12)
                rnd = 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
                rnd = std::clamp(rnd, p->min_val, p->max_val);
            } else {
                rnd = frand(p->min_val, p->max_val);
            }

            float v = cur + (rnd - cur) * amount01;
            setParameterValue(id, std::clamp(v, p->min_val, p->max_val));
            break;
        }
        case ParamType::INT:
        case ParamType::ENUM: {
            if (frand(0.0f, 1.0f) < amount01) {
                int minI = static_cast<int>(p->min_val);
                int maxI = static_cast<int>(p->max_val);
                int span = std::max(0, maxI - minI);
                int value = span == 0 ? minI : (minI + (rand() % (span + 1)));
                setParameterValue(id, static_cast<float>(value));
            }
            break;
        }
        case ParamType::BOOL: {
            if (frand(0.0f, 1.0f) < amount01 * 0.5f) {
                bool cur = getParameterValue(id) > 0.5f;
                setParameterValue(id, cur ? 0.0f : 1.0f);
            }
            break;
        }
    }
}

void UI::mutateSingleParameter(int id, float amount01) {
    amount01 = std::clamp(amount01, 0.0f, 1.0f);
    InlineParameter* p = getParameter(id);
    if (!p || !p->randomizable) return;

    switch (p->type) {
        case ParamType::FLOAT: {
            float cur = getParameterValue(id);
            float range = (p->max_val - p->min_val);
            float maxDelta = range * (0.3f * amount01);
            float v = std::clamp(cur + frand(-maxDelta, maxDelta), p->min_val, p->max_val);
            setParameterValue(id, v);
            break;
        }
        case ParamType::INT:
        case ParamType::ENUM: {
            if (frand(0.0f, 1.0f) < amount01) {
                int cur = static_cast<int>(std::round(getParameterValue(id)));
                int dir = (rand() % 3) - 1;
                int minI = static_cast<int>(p->min_val);
                int maxI = static_cast<int>(p->max_val);
                cur = std::clamp(cur + dir, minI, maxI);
                setParameterValue(id, static_cast<float>(cur));
            }
            break;
        }
        case ParamType::BOOL: {
            if (frand(0.0f, 1.0f) < amount01 * 0.25f) {
                bool cur = getParameterValue(id) > 0.5f;
                setParameterValue(id, cur ? 0.0f : 1.0f);
            }
            break;
        }
    }
}

void UI::randomizeModSlot(int slotIndex, float amount01, bool capture) {
    amount01 = std::clamp(amount01, 0.0f, 1.0f);
    if (amount01 <= 0.0f) return;
    if (slotIndex < 0 || slotIndex >= kModulationSlotCount) return;
    if (modSlotLocked[slotIndex]) return;
    if (capture) captureUndoSnapshot("randomize_mod_slot");

    ModulationSlot& slot = modulationSlots[slotIndex];
    const auto& sources = getModSourceOptions();
    const auto& curves = getModCurveOptions();
    const auto& types = getModTypeOptions();
    const auto& destinations = getModDestinationOptions();

    auto pick = [](int count) -> int {
        return count > 0 ? rand() % count : -1;
    };

    slot.source = pick(static_cast<int>(sources.size()));
    slot.curve = pick(static_cast<int>(curves.size()));
    slot.type = pick(static_cast<int>(types.size()));
    slot.destination = pick(static_cast<int>(destinations.size()));

    float range = std::max(5.0f, 99.0f * amount01);
    int newAmount = static_cast<int>(std::round(frand(-range, range)));
    if (newAmount == 0) {
        newAmount = (rand() & 1) ? 50 : -50;
    }
    slot.amount = static_cast<int8_t>(std::clamp(newAmount, -99, 99));
}

void UI::mutateModSlot(int slotIndex, float amount01, bool capture) {
    amount01 = std::clamp(amount01, 0.0f, 1.0f);
    if (amount01 <= 0.0f) return;
    if (slotIndex < 0 || slotIndex >= kModulationSlotCount) return;
    if (modSlotLocked[slotIndex]) return;
    if (capture) captureUndoSnapshot("mutate_mod_slot");

    ModulationSlot& slot = modulationSlots[slotIndex];
    if (!slot.isComplete()) {
        randomizeModSlot(slotIndex, amount01, capture);
        return;
    }

    const auto& sources = getModSourceOptions();
    const auto& curves = getModCurveOptions();
    const auto& types = getModTypeOptions();
    const auto& destinations = getModDestinationOptions();

    auto maybeReassign = [&](auto& field, int count) {
        if (count <= 0) return;
        if (frand(0.0f, 1.0f) < amount01 * 0.35f) {
            field = rand() % count;
        }
    };

    maybeReassign(slot.source, static_cast<int>(sources.size()));
    maybeReassign(slot.curve, static_cast<int>(curves.size()));
    maybeReassign(slot.type, static_cast<int>(types.size()));
    maybeReassign(slot.destination, static_cast<int>(destinations.size()));

    float delta = std::max(5.0f, 30.0f * amount01);
    int mutated = static_cast<int>(std::round(slot.amount + frand(-delta, delta)));
    slot.amount = static_cast<int8_t>(std::clamp(mutated, -99, 99));
}

void UI::randomizeAllModSlots(float amount01) {
    amount01 = std::clamp(amount01, 0.0f, 1.0f);
    if (amount01 <= 0.0f) return;
    captureUndoSnapshot("randomize_all_mod_slots");
    for (int i = 0; i < kModulationSlotCount; ++i) {
        randomizeModSlot(i, amount01, false);
    }
}

void UI::mutateAllModSlots(float amount01) {
    amount01 = std::clamp(amount01, 0.0f, 1.0f);
    if (amount01 <= 0.0f) return;
    captureUndoSnapshot("mutate_all_mod_slots");
    for (int i = 0; i < kModulationSlotCount; ++i) {
        mutateModSlot(i, amount01, false);
    }
}

void UI::randomizeCurrentEntity(UIPage page, float amount01) {
    amount01 = std::clamp(amount01, 0.0f, 1.0f);
    if (amount01 <= 0.0f) return;
    captureUndoSnapshot("randomize_entity");
    auto applyList = [&](const std::vector<int>& ids) {
        for (int id : ids) {
            randomizeSingleParameter(id, amount01);
        }
    };

    switch (page) {
        case UIPage::MAIN: {
            if (!mainPageFocusLeft) {
                int pid = -1;
                if (mainPageMixerChannel >= 0 && mainPageMixerChannel < 4) {
                    pid = 50 + mainPageMixerChannel;
                } else if (mainPageMixerChannel >= 4 && mainPageMixerChannel < 8) {
                    pid = 54 + (mainPageMixerChannel - 4);
                } else if (mainPageMixerChannel >= 8 && mainPageMixerChannel < 12) {
                    pid = 410 + (mainPageMixerChannel - 8);
                }
                if (pid >= 0) {
                    randomizeSingleParameter(pid, amount01);
                    break;
                }
            }
            randomizeSingleParameter(selectedParameterId, amount01);
            break;
        }
        case UIPage::OSCILLATOR: {
            auto ids = getParameterIdsForPage(UIPage::OSCILLATOR);
            if (ids.empty()) return;
            int saved = currentOscillatorIndex;
            currentOscillatorIndex = std::clamp(currentOscillatorIndex, 0, 3);
            applyList(ids);
            currentOscillatorIndex = saved;
            break;
        }
        case UIPage::SAMPLER: {
            auto ids = getParameterIdsForPage(UIPage::SAMPLER);
            if (ids.empty()) return;
            int saved = currentSamplerIndex;
            currentSamplerIndex = std::clamp(currentSamplerIndex, 0, 3);
            applyList(ids);
            currentSamplerIndex = saved;
            break;
        }
        case UIPage::LFO: {
            auto ids = getParameterIdsForPage(UIPage::LFO);
            if (ids.empty()) return;
            int saved = currentLFOIndex;
            currentLFOIndex = std::clamp(currentLFOIndex, 0, 3);
            applyList(ids);
            currentLFOIndex = saved;
            break;
        }
        case UIPage::CHAOS: {
            auto ids = getParameterIdsForPage(UIPage::CHAOS);
            if (ids.empty()) return;
            int saved = currentChaosIndex;
            currentChaosIndex = std::clamp(currentChaosIndex, 0, 3);
            applyList(ids);
            currentChaosIndex = saved;
            break;
        }
        case UIPage::ENV: {
            auto ids = getParameterIdsForPage(UIPage::ENV);
            if (ids.empty()) return;
            int saved = currentEnvelopeIndex;
            currentEnvelopeIndex = std::clamp(currentEnvelopeIndex, 0, 3);
            applyList(ids);
            currentEnvelopeIndex = saved;
            break;
        }
        case UIPage::MOD:
            randomizeModSlot(modMatrixCursorRow, amount01, false);
            break;
        default:
            randomizeSingleParameter(selectedParameterId, amount01);
            break;
    }
}

void UI::mutateCurrentEntity(UIPage page, float amount01) {
    amount01 = std::clamp(amount01, 0.0f, 1.0f);
    if (amount01 <= 0.0f) return;
    captureUndoSnapshot("mutate_entity");
    auto applyList = [&](const std::vector<int>& ids) {
        for (int id : ids) {
            mutateSingleParameter(id, amount01);
        }
    };

    switch (page) {
        case UIPage::MAIN: {
            if (!mainPageFocusLeft) {
                int pid = -1;
                if (mainPageMixerChannel >= 0 && mainPageMixerChannel < 4) {
                    pid = 50 + mainPageMixerChannel;
                } else if (mainPageMixerChannel >= 4 && mainPageMixerChannel < 8) {
                    pid = 54 + (mainPageMixerChannel - 4);
                } else if (mainPageMixerChannel >= 8 && mainPageMixerChannel < 12) {
                    pid = 410 + (mainPageMixerChannel - 8);
                }
                if (pid >= 0) {
                    mutateSingleParameter(pid, amount01);
                    break;
                }
            }
            mutateSingleParameter(selectedParameterId, amount01);
            break;
        }
        case UIPage::OSCILLATOR: {
            auto ids = getParameterIdsForPage(UIPage::OSCILLATOR);
            if (ids.empty()) return;
            int saved = currentOscillatorIndex;
            currentOscillatorIndex = std::clamp(currentOscillatorIndex, 0, 3);
            applyList(ids);
            currentOscillatorIndex = saved;
            break;
        }
        case UIPage::SAMPLER: {
            auto ids = getParameterIdsForPage(UIPage::SAMPLER);
            if (ids.empty()) return;
            int saved = currentSamplerIndex;
            currentSamplerIndex = std::clamp(currentSamplerIndex, 0, 3);
            applyList(ids);
            currentSamplerIndex = saved;
            break;
        }
        case UIPage::LFO: {
            auto ids = getParameterIdsForPage(UIPage::LFO);
            if (ids.empty()) return;
            int saved = currentLFOIndex;
            currentLFOIndex = std::clamp(currentLFOIndex, 0, 3);
            applyList(ids);
            currentLFOIndex = saved;
            break;
        }
        case UIPage::CHAOS: {
            auto ids = getParameterIdsForPage(UIPage::CHAOS);
            if (ids.empty()) return;
            int saved = currentChaosIndex;
            currentChaosIndex = std::clamp(currentChaosIndex, 0, 3);
            applyList(ids);
            currentChaosIndex = saved;
            break;
        }
        case UIPage::ENV: {
            auto ids = getParameterIdsForPage(UIPage::ENV);
            if (ids.empty()) return;
            int saved = currentEnvelopeIndex;
            currentEnvelopeIndex = std::clamp(currentEnvelopeIndex, 0, 3);
            applyList(ids);
            currentEnvelopeIndex = saved;
            break;
        }
        case UIPage::MOD:
            mutateModSlot(modMatrixCursorRow, amount01, false);
            break;
        default:
            mutateSingleParameter(selectedParameterId, amount01);
            break;
    }
}

void UI::captureUndoSnapshot(const char* /*reason*/) {
    if (undoCaptureSuppressed || !params) return;
    std::string snapshot = serializeState();
    if (!undoStack.empty() && snapshot == undoStack.back()) {
        return;
    }
    if (undoStack.size() >= kMaxUndoHistory) {
        undoStack.erase(undoStack.begin());
    }
    undoStack.push_back(std::move(snapshot));
    redoStack.clear();
}

void UI::resetUndoHistory() {
    undoStack.clear();
    redoStack.clear();
    if (params) {
        undoStack.push_back(serializeState());
    }
}

std::string UI::serializeState() {
    if (!params) return {};

    std::ostringstream out;
    out.setf(std::ios::fixed);
    out << std::setprecision(9);

    int savedOsc = currentOscillatorIndex;
    int savedSampler = currentSamplerIndex;
    int savedLFO = currentLFOIndex;
    int savedEnv = currentEnvelopeIndex;
    int savedChaos = currentChaosIndex;

    out << "#PARAM_BEGIN\n";
    for (const auto& paramDef : parameters) {
        int id = paramDef.id;
        InlineParameter* param = getParameter(id);
        if (!param) continue;

        auto emitValue = [&](const char* tag, int idx) {
            float value = getParameterValue(id);
            out << "P " << id << ' ' << tag << ' ' << idx << ' ' << value << '\n';
        };

        if (paramUsesOscIndex(id)) {
            for (int i = 0; i < 4; ++i) {
                currentOscillatorIndex = i;
                emitValue("osc", i);
            }
        } else if (paramUsesSamplerIndex(id)) {
            for (int i = 0; i < 4; ++i) {
                currentSamplerIndex = i;
                emitValue("samp", i);
            }
        } else if (paramUsesLfoIndex(id)) {
            for (int i = 0; i < 4; ++i) {
                currentLFOIndex = i;
                emitValue("lfo", i);
            }
        } else if (paramUsesEnvIndex(id)) {
            for (int i = 0; i < 4; ++i) {
                currentEnvelopeIndex = i;
                emitValue("env", i);
            }
        } else if (paramUsesChaosIndex(id)) {
            for (int i = 0; i < 4; ++i) {
                currentChaosIndex = i;
                emitValue("chaos", i);
            }
        } else {
            emitValue("global", -1);
        }
    }
    out << "#PARAM_END\n";

    currentOscillatorIndex = savedOsc;
    currentSamplerIndex = savedSampler;
    currentLFOIndex = savedLFO;
    currentEnvelopeIndex = savedEnv;
    currentChaosIndex = savedChaos;

    out << "#FM_BEGIN\n";
    for (int target = 0; target < kFMTargetCount; ++target) {
        for (int source = 0; source < kFMSourceCount; ++source) {
            out << "F " << target << ' ' << source << ' ' << params->getFMDepth(target, source) << '\n';
        }
    }
    out << "#FM_END\n";

    out << "#FM_LOCK_BEGIN\n";
    for (int target = 0; target < kFMTargetCount; ++target) {
        for (int source = 0; source < kFMSourceCount; ++source) {
            out << "FL " << target << ' ' << source << ' ' << (fmMatrixLocked[target][source] ? 1 : 0) << '\n';
        }
    }
    out << "#FM_LOCK_END\n";

    out << "#MOD_BEGIN\n";
    for (int i = 0; i < kModulationSlotCount; ++i) {
        const ModulationSlot& slot = modulationSlots[i];
        out << "M " << i << ' '
            << static_cast<int>(slot.source) << ' '
            << static_cast<int>(slot.curve) << ' '
            << static_cast<int>(slot.amount) << ' '
            << static_cast<int>(slot.destination) << ' '
            << static_cast<int>(slot.type) << ' '
            << (modSlotLocked[i] ? 1 : 0) << '\n';
    }
    out << "#MOD_END\n";

    out << "#UI_BEGIN\n";
    out << "selected " << selectedParameterId << '\n';
    out << "currentOsc " << savedOsc << '\n';
    out << "currentSampler " << savedSampler << '\n';
    out << "currentLFO " << savedLFO << '\n';
    out << "currentEnv " << savedEnv << '\n';
    out << "currentChaos " << savedChaos << '\n';
    out << "mainFocusLeft " << (mainPageFocusLeft ? 1 : 0) << '\n';
    out << "mainMixerChannel " << mainPageMixerChannel << '\n';
    out << "mainActionIndex " << mainPageActionIndex << '\n';
    out << "globalRandom " << globalRandomizePercentage << '\n';
    out << "globalMutate " << globalMutatePercentage << '\n';
    out << "modCursorRow " << modMatrixCursorRow << '\n';
    out << "modCursorCol " << modMatrixCursorCol << '\n';
    out << "#UI_END\n";

    // MIDI CC mapping section (param ID -> CC with context indices)
    out << "#MIDI_BEGIN\n";
    if (params) {
        // Legacy single mapping for cutoff (kept for compatibility)
        int legacyCutoff = params->filterCutoffCC.load();
        out << "LEGACY_CUTOFF_CC " << legacyCutoff << "\n";

        for (int pid = 0; pid < SynthParameters::kMaxParamMap; ++pid) {
            int cc = params->parameterCCMap[pid].load();
            if (cc >= 0) {
                int osc = params->parameterContextOsc[pid].load();
                int lfo = params->parameterContextLFO[pid].load();
                int env = params->parameterContextEnv[pid].load();
                int samp = params->parameterContextSampler[pid].load();
                int chaos = params->parameterContextChaos[pid].load();
                out << "CC " << pid << ' ' << cc << ' ' << osc << ' ' << lfo << ' '
                    << env << ' ' << samp << ' ' << chaos << "\n";
            }
        }
    }
    out << "#MIDI_END\n";

    // Instance counts section
    out << "#INSTANCE_COUNTS_BEGIN\n";
    if (params) {
        out << "activeOscCount " << params->activeOscCount.load() << '\n';
        out << "activeSamplerCount " << params->activeSamplerCount.load() << '\n';
        out << "activeLfoCount " << params->activeLfoCount.load() << '\n';
        out << "activeEnvCount " << params->activeEnvCount.load() << '\n';
        out << "activeChaosCount " << params->activeChaosCount.load() << '\n';
    }
    out << "#INSTANCE_COUNTS_END\n";

    // Mixer mute/solo state section
    out << "#MIXER_STATE_BEGIN\n";
    if (params) {
        // OSC mute/solo
        for (int i = 0; i < 4; ++i) {
            out << "oscMuted " << i << ' ' << (params->oscMuted[i].load() ? 1 : 0) << '\n';
        }
        for (int i = 0; i < 4; ++i) {
            out << "oscSolo " << i << ' ' << (params->oscSolo[i].load() ? 1 : 0) << '\n';
        }
        // Sampler mute/solo
        for (int i = 0; i < 4; ++i) {
            out << "samplerMuted " << i << ' ' << (params->samplerMuted[i].load() ? 1 : 0) << '\n';
        }
        for (int i = 0; i < 4; ++i) {
            out << "samplerSolo " << i << ' ' << (params->samplerSolo[i].load() ? 1 : 0) << '\n';
        }
        // Chaos mute/solo
        for (int i = 0; i < 4; ++i) {
            out << "chaosMuted " << i << ' ' << (params->chaosMuted[i].load() ? 1 : 0) << '\n';
        }
        for (int i = 0; i < 4; ++i) {
            out << "chaosSolo " << i << ' ' << (params->chaosSolo[i].load() ? 1 : 0) << '\n';
        }
    }
    out << "#MIXER_STATE_END\n";

    return out.str();
}

void UI::applySerializedState(const std::string& data) {
    if (!params) return;
    bool previous = undoCaptureSuppressed;
    undoCaptureSuppressed = true;

    std::istringstream stream(data);
    std::string line;
    enum class Section { None, Param, FM, FMLock, Mod, UI, MIDI, InstanceCounts, MixerState };
    Section section = Section::None;

    int savedOsc = currentOscillatorIndex;
    int savedSampler = currentSamplerIndex;
    int savedLFO = currentLFOIndex;
    int savedEnv = currentEnvelopeIndex;
    int savedChaos = currentChaosIndex;

    while (std::getline(stream, line)) {
        line = trimWhitespace(line);
        if (line.empty()) continue;
        if (line[0] == '#') {
            if (line == "#PARAM_BEGIN") section = Section::Param;
            else if (line == "#PARAM_END") section = Section::None;
            else if (line == "#FM_BEGIN") section = Section::FM;
            else if (line == "#FM_END") section = Section::None;
            else if (line == "#FM_LOCK_BEGIN") section = Section::FMLock;
            else if (line == "#FM_LOCK_END") section = Section::None;
            else if (line == "#MOD_BEGIN") section = Section::Mod;
            else if (line == "#MOD_END") section = Section::None;
            else if (line == "#UI_BEGIN") section = Section::UI;
            else if (line == "#UI_END") section = Section::None;
            else if (line == "#MIDI_BEGIN") section = Section::MIDI;
            else if (line == "#MIDI_END") section = Section::None;
            else if (line == "#INSTANCE_COUNTS_BEGIN") section = Section::InstanceCounts;
            else if (line == "#INSTANCE_COUNTS_END") section = Section::None;
            else if (line == "#MIXER_STATE_BEGIN") section = Section::MixerState;
            else if (line == "#MIXER_STATE_END") section = Section::None;
            continue;
        }

        std::istringstream ls(line);
        switch (section) {
            case Section::Param: {
                char prefix;
                ls >> prefix;
                if (prefix != 'P') break;
                int id;
                std::string tag;
                int idx;
                float value;
                if (!(ls >> id >> tag >> idx >> value)) break;

                auto applyValue = [&](auto setIndex, int savedIndex) {
                    setIndex(idx);
                    setParameterValue(id, value);
                    setIndex(savedIndex);
                };

                if (tag == "osc") {
                    if (idx < 0 || idx >= 4) break;
                    applyValue([&](int v){ currentOscillatorIndex = v; }, savedOsc);
                } else if (tag == "samp") {
                    if (idx < 0 || idx >= 4) break;
                    applyValue([&](int v){ currentSamplerIndex = v; }, savedSampler);
                } else if (tag == "lfo") {
                    if (idx < 0 || idx >= 4) break;
                    applyValue([&](int v){ currentLFOIndex = v; }, savedLFO);
                } else if (tag == "env") {
                    if (idx < 0 || idx >= 4) break;
                    applyValue([&](int v){ currentEnvelopeIndex = v; }, savedEnv);
                } else if (tag == "chaos") {
                    if (idx < 0 || idx >= 4) break;
                    applyValue([&](int v){ currentChaosIndex = v; }, savedChaos);
                } else {
                    setParameterValue(id, value);
                }
                break;
            }
            case Section::FM: {
                char prefix;
                int target, source;
                float value;
                if (!(ls >> prefix >> target >> source >> value)) break;
                if (prefix != 'F') break;
                if (target >= 0 && target < kFMTargetCount && source >= 0 && source < kFMSourceCount) {
                    params->setFMDepth(target, source, value);
                }
                break;
            }
            case Section::FMLock: {
                char prefix;
                int target, source, locked;
                if (!(ls >> prefix >> target >> source >> locked)) break;
                if (prefix != 'F') break;
                if (target >= 0 && target < kFMTargetCount && source >= 0 && source < kFMSourceCount) {
                    fmMatrixLocked[target][source] = (locked != 0);
                }
                break;
            }
            case Section::Mod: {
                char prefix;
                int idx;
                int source, curve, amount, destination, type, locked;
                if (!(ls >> prefix >> idx >> source >> curve >> amount >> destination >> type >> locked)) break;
                if (prefix != 'M') break;
                if (idx < 0 || idx >= kModulationSlotCount) break;
                modulationSlots[idx].source = static_cast<int8_t>(source);
                modulationSlots[idx].curve = static_cast<int8_t>(curve);
                modulationSlots[idx].amount = static_cast<int8_t>(std::clamp(amount, -99, 99));
                modulationSlots[idx].destination = static_cast<int16_t>(destination);
                modulationSlots[idx].type = static_cast<int8_t>(type);
                modSlotLocked[idx] = (locked != 0);
                break;
            }
            case Section::UI: {
                std::string key;
                ls >> key;
                if (key == "selected") {
                    ls >> selectedParameterId;
                } else if (key == "currentOsc") {
                    ls >> currentOscillatorIndex;
                } else if (key == "currentSampler") {
                    ls >> currentSamplerIndex;
                } else if (key == "currentLFO") {
                    ls >> currentLFOIndex;
                } else if (key == "currentEnv") {
                    ls >> currentEnvelopeIndex;
                } else if (key == "currentChaos") {
                    ls >> currentChaosIndex;
                } else if (key == "mainFocusLeft") {
                    int flag; ls >> flag; mainPageFocusLeft = (flag != 0);
                } else if (key == "mainMixerChannel") {
                    ls >> mainPageMixerChannel;
                } else if (key == "mainActionIndex") {
                    ls >> mainPageActionIndex;
                } else if (key == "globalRandom") {
                    ls >> globalRandomizePercentage;
                } else if (key == "globalMutate") {
                    ls >> globalMutatePercentage;
                } else if (key == "modCursorRow") {
                    ls >> modMatrixCursorRow;
                } else if (key == "modCursorCol") {
                    ls >> modMatrixCursorCol;
                }
                break;
            }
            case Section::MIDI: {
                // Format:
                //   LEGACY_CUTOFF_CC <cc>
                //   CC <paramId> <cc> <osc> <lfo> <env> <samp> <chaos>
                std::string token;
                ls >> token;
                if (token == "LEGACY_CUTOFF_CC") {
                    int v; if (ls >> v) { params->filterCutoffCC = v; }
                } else if (token == "CC") {
                    int pid, cc, osc, lfo, env, samp, chaos;
                    if (ls >> pid >> cc >> osc >> lfo >> env >> samp >> chaos) {
                        if (pid >= 0 && pid < SynthParameters::kMaxParamMap) {
                            params->parameterCCMap[pid] = cc;
                            params->parameterContextOsc[pid] = osc;
                            params->parameterContextLFO[pid] = lfo;
                            params->parameterContextEnv[pid] = env;
                            params->parameterContextSampler[pid] = samp;
                            params->parameterContextChaos[pid] = chaos;
                        }
                    }
                }
                break;
            }
            case Section::InstanceCounts: {
                // Format: activeOscCount <count>
                std::string key;
                ls >> key;
                if (key == "activeOscCount") {
                    int v; if (ls >> v) { params->activeOscCount = std::clamp(v, 1, 4); }
                } else if (key == "activeSamplerCount") {
                    int v; if (ls >> v) { params->activeSamplerCount = std::clamp(v, 1, 4); }
                } else if (key == "activeLfoCount") {
                    int v; if (ls >> v) { params->activeLfoCount = std::clamp(v, 1, 4); }
                } else if (key == "activeEnvCount") {
                    int v; if (ls >> v) { params->activeEnvCount = std::clamp(v, 1, 4); }
                } else if (key == "activeChaosCount") {
                    int v; if (ls >> v) { params->activeChaosCount = std::clamp(v, 1, 4); }
                }
                break;
            }
            case Section::MixerState: {
                // Format: oscMuted <index> <0|1>
                std::string key;
                int index, value;
                ls >> key >> index >> value;
                if (index < 0 || index >= 4) break;
                if (key == "oscMuted") {
                    params->oscMuted[index] = (value != 0);
                } else if (key == "oscSolo") {
                    params->oscSolo[index] = (value != 0);
                } else if (key == "samplerMuted") {
                    params->samplerMuted[index] = (value != 0);
                } else if (key == "samplerSolo") {
                    params->samplerSolo[index] = (value != 0);
                } else if (key == "chaosMuted") {
                    params->chaosMuted[index] = (value != 0);
                } else if (key == "chaosSolo") {
                    params->chaosSolo[index] = (value != 0);
                }
                break;
            }
            default:
                break;
        }
    }

    currentOscillatorIndex = savedOsc;
    currentSamplerIndex = savedSampler;
    currentLFOIndex = savedLFO;
    currentEnvelopeIndex = savedEnv;
    currentChaosIndex = savedChaos;

    currentOscillatorIndex = std::clamp(currentOscillatorIndex, 0, 3);
    currentSamplerIndex = std::clamp(currentSamplerIndex, 0, 3);
    currentLFOIndex = std::clamp(currentLFOIndex, 0, 3);
    currentEnvelopeIndex = std::clamp(currentEnvelopeIndex, 0, 3);
    currentChaosIndex = std::clamp(currentChaosIndex, 0, 3);
    mainPageMixerChannel = std::clamp(mainPageMixerChannel, 0, 11);
    mainPageActionIndex = std::clamp(mainPageActionIndex, 0, kMainPageActionCount - 1);
    modMatrixCursorRow = std::clamp(modMatrixCursorRow, 0, kModulationSlotCount - 1);
    modMatrixCursorCol = std::clamp(modMatrixCursorCol, 0, kFMTargetCount - 1);

    if (synth) {
        synth->refreshSamplerPhaseDrivers();
    }

    undoCaptureSuppressed = previous;
}

void UI::undoAction() {
    if (undoStack.empty()) {
        return;
    }
    if (!params) return;
    if (redoStack.size() >= kMaxUndoHistory) {
        redoStack.erase(redoStack.begin());
    }
    redoStack.push_back(serializeState());
    std::string snapshot = undoStack.back();
    undoStack.pop_back();
    applySerializedState(snapshot);
    addConsoleMessage("Undo");
}

void UI::redoAction() {
    if (redoStack.empty() || !params) {
        return;
    }
    if (undoStack.size() >= kMaxUndoHistory) {
        undoStack.erase(undoStack.begin());
    }
    undoStack.push_back(serializeState());
    std::string snapshot = redoStack.back();
    redoStack.pop_back();
    applySerializedState(snapshot);
    addConsoleMessage("Redo");
}

void UI::mutateAllParameters(float amount01) {
    amount01 = std::max(0.0f, std::min(1.0f, amount01));
    if (amount01 <= 0.0f) return;
    captureUndoSnapshot("mutate_all");
    auto ids = getRandomizableParameterIds();
    for (int id : ids) {
        InlineParameter* p = getParameter(id);
        if (!p) continue;

        auto applyOne = [&](int count, auto setIndex, int saved) {
            for (int i = 0; i < count; ++i) {
                setIndex(i);
                switch (p->type) {
                    case ParamType::FLOAT: {
                        float cur = getParameterValue(id);
                        float range = (p->max_val - p->min_val);
                        float maxDelta = range * (0.3f * amount01);
                        float delta = frand(-maxDelta, maxDelta);
                        float v = std::max(p->min_val, std::min(p->max_val, cur + delta));
                        setParameterValue(id, v);
                        break;
                    }
                    case ParamType::INT:
                    case ParamType::ENUM: {
                        float roll = frand(0.0f, 1.0f);
                        int cur = static_cast<int>(getParameterValue(id));
                        if (roll < amount01) {
                            int dir = (rand() % 3) - 1; // -1,0,1
                            int v = std::max(static_cast<int>(p->min_val), std::min(static_cast<int>(p->max_val), cur + dir));
                            setParameterValue(id, static_cast<float>(v));
                        }
                        break;
                    }
                    case ParamType::BOOL: {
                        float roll = frand(0.0f, 1.0f);
                        if (roll < amount01 * 0.25f) {
                            bool cur = getParameterValue(id) > 0.5f;
                            setParameterValue(id, cur ? 0.0f : 1.0f);
                        }
                        break;
                    }
                }
            }
            setIndex(saved);
        };

        if (paramUsesOscIndex(id)) {
            int saved = currentOscillatorIndex; auto setIdx = [&](int v){ currentOscillatorIndex = v;};
            applyOne(4, setIdx, saved); continue;
        }
        if (paramUsesLfoIndex(id)) {
            int saved = currentLFOIndex; auto setIdx = [&](int v){ currentLFOIndex = v;};
            applyOne(4, setIdx, saved); continue;
        }
        if (paramUsesEnvIndex(id)) {
            int saved = currentEnvelopeIndex; auto setIdx = [&](int v){ currentEnvelopeIndex = v;};
            applyOne(4, setIdx, saved); continue;
        }
        if (paramUsesSamplerIndex(id)) {
            int saved = currentSamplerIndex; auto setIdx = [&](int v){ currentSamplerIndex = v;};
            applyOne(4, setIdx, saved); continue;
        }
        if (paramUsesChaosIndex(id)) {
            int saved = currentChaosIndex; auto setIdx = [&](int v){ currentChaosIndex = v;};
            applyOne(4, setIdx, saved); continue;
        }

        // Global (no index)
        switch (p->type) {
            case ParamType::FLOAT: {
                float cur = getParameterValue(id);
                float range = (p->max_val - p->min_val);
                float maxDelta = range * (0.3f * amount01);
                float delta = frand(-maxDelta, maxDelta);
                float v = std::max(p->min_val, std::min(p->max_val, cur + delta));
                setParameterValue(id, v);
                break;
            }
            case ParamType::INT:
            case ParamType::ENUM: {
                float roll = frand(0.0f, 1.0f);
                int cur = static_cast<int>(getParameterValue(id));
                if (roll < amount01) {
                    int dir = (rand() % 3) - 1;
                    int v = std::max(static_cast<int>(p->min_val), std::min(static_cast<int>(p->max_val), cur + dir));
                    setParameterValue(id, static_cast<float>(v));
                }
                break;
            }
            case ParamType::BOOL: {
                float roll = frand(0.0f, 1.0f);
                if (roll < amount01 * 0.25f) {
                    bool cur = getParameterValue(id) > 0.5f;
                    setParameterValue(id, cur ? 0.0f : 1.0f);
                }
                break;
            }
        }
    }
}

void UI::resetAllParametersToNeutral() {
    captureUndoSnapshot("reset_all");
    auto ids = getRandomizableParameterIds();
    for (int id : ids) {
        InlineParameter* p = getParameter(id);
        if (!p) continue;

        auto applyOne = [&](int count, auto setIndex, int saved) {
            for (int i = 0; i < count; ++i) {
                setIndex(i);
                switch (p->type) {
                    case ParamType::FLOAT: {
                        float v = (p->min_val + p->max_val) * 0.5f;
                        setParameterValue(id, v);
                        break;
                    }
                    case ParamType::INT:
                    case ParamType::ENUM: {
                        setParameterValue(id, p->min_val);
                        break;
                    }
                    case ParamType::BOOL: {
                        setParameterValue(id, 0.0f);
                        break;
                    }
                }
            }
            setIndex(saved);
        };

        if (paramUsesOscIndex(id)) {
            int saved = currentOscillatorIndex; auto setIdx = [&](int v){ currentOscillatorIndex = v;};
            applyOne(4, setIdx, saved); continue;
        }
        if (paramUsesLfoIndex(id)) {
            int saved = currentLFOIndex; auto setIdx = [&](int v){ currentLFOIndex = v;};
            applyOne(4, setIdx, saved); continue;
        }
        if (paramUsesEnvIndex(id)) {
            int saved = currentEnvelopeIndex; auto setIdx = [&](int v){ currentEnvelopeIndex = v;};
            applyOne(4, setIdx, saved); continue;
        }
        if (paramUsesSamplerIndex(id)) {
            int saved = currentSamplerIndex; auto setIdx = [&](int v){ currentSamplerIndex = v;};
            applyOne(4, setIdx, saved); continue;
        }
        if (paramUsesChaosIndex(id)) {
            int saved = currentChaosIndex; auto setIdx = [&](int v){ currentChaosIndex = v;};
            applyOne(4, setIdx, saved); continue;
        }

        // Global (no index)
        switch (p->type) {
            case ParamType::FLOAT: setParameterValue(id, (p->min_val + p->max_val) * 0.5f); break;
            case ParamType::INT:
            case ParamType::ENUM: setParameterValue(id, p->min_val); break;
            case ParamType::BOOL: setParameterValue(id, 0.0f); break;
        }
    }
}

// Per-page helpers reuse the same logic as global ops, but filter to IDs on a page
void UI::randomizePageParameters(UIPage page, float amount01) {
    amount01 = std::clamp(amount01, 0.0f, 1.0f);
    if (amount01 <= 0.0f) return;
    captureUndoSnapshot("randomize_page");
    auto ids = getParameterIdsForPage(page);
    for (int id : ids) {
        InlineParameter* p = getParameter(id);
        if (!p || !p->randomizable) continue;

        // Reuse the same per-id logic as randomizeAllParameters
        // Indexed params are handled by the same branches
        if (paramUsesOscIndex(id)) {
            int saved = currentOscillatorIndex; for (int i=0;i<4;++i){ currentOscillatorIndex=i; }
            // Apply across all indices
            for (int i=0;i<4;++i){ currentOscillatorIndex=i;
                if (p->type==ParamType::FLOAT) { float cur=getParameterValue(id); float rnd=frand(p->min_val,p->max_val); float v=cur+(rnd-cur)*amount01; setParameterValue(id,std::clamp(v,p->min_val,p->max_val)); }
                else if (p->type==ParamType::INT || p->type==ParamType::ENUM){ if (frand(0.0f,1.0f)<amount01){int minI=(int)p->min_val,maxI=(int)p->max_val;int span=std::max(0,maxI-minI);int r= span==0?minI:(minI + (rand()%(span+1))); setParameterValue(id,(float)r);} } 
                else { if (frand(0.0f,1.0f)<amount01){ bool cur=getParameterValue(id)>0.5f; setParameterValue(id, cur?0.0f:1.0f);} }
            }
            currentOscillatorIndex = saved; continue;
        }
        if (paramUsesLfoIndex(id)) {
            int saved = currentLFOIndex; for (int i=0;i<4;++i){ currentLFOIndex=i;
                if (p->type==ParamType::FLOAT) setParameterValue(id, frand(p->min_val,p->max_val));
                else if (p->type==ParamType::INT || p->type==ParamType::ENUM){int minI=(int)p->min_val,maxI=(int)p->max_val;int span=std::max(0,maxI-minI);int r= span==0?minI:(minI + (rand()%(span+1))); setParameterValue(id,(float)r);} 
                else setParameterValue(id,(rand()&1)?1.0f:0.0f);
            }
            currentLFOIndex = saved; continue;
        }
        if (paramUsesEnvIndex(id)) {
            int saved = currentEnvelopeIndex; for (int i=0;i<4;++i){ currentEnvelopeIndex=i;
                if (p->type==ParamType::FLOAT) setParameterValue(id, frand(p->min_val,p->max_val));
                else if (p->type==ParamType::INT || p->type==ParamType::ENUM){int minI=(int)p->min_val,maxI=(int)p->max_val;int span=std::max(0,maxI-minI);int r= span==0?minI:(minI + (rand()%(span+1))); setParameterValue(id,(float)r);} 
                else setParameterValue(id,(rand()&1)?1.0f:0.0f);
            }
            currentEnvelopeIndex = saved; continue;
        }
        if (paramUsesSamplerIndex(id)) {
            int saved = currentSamplerIndex; for (int i=0;i<4;++i){ currentSamplerIndex=i;
                if (p->type==ParamType::FLOAT) setParameterValue(id, frand(p->min_val,p->max_val));
                else if (p->type==ParamType::INT || p->type==ParamType::ENUM){int minI=(int)p->min_val,maxI=(int)p->max_val;int span=std::max(0,maxI-minI);int r= span==0?minI:(minI + (rand()%(span+1))); setParameterValue(id,(float)r);} 
                else setParameterValue(id,(rand()&1)?1.0f:0.0f);
            }
            currentSamplerIndex = saved; continue;
        }
        if (paramUsesChaosIndex(id)) {
            int saved = currentChaosIndex; for (int i=0;i<4;++i){ currentChaosIndex=i;
                if (p->type==ParamType::FLOAT) setParameterValue(id, frand(p->min_val,p->max_val));
                else if (p->type==ParamType::INT || p->type==ParamType::ENUM){int minI=(int)p->min_val,maxI=(int)p->max_val;int span=std::max(0,maxI-minI);int r= span==0?minI:(minI + (rand()%(span+1))); setParameterValue(id,(float)r);} 
                else setParameterValue(id,(rand()&1)?1.0f:0.0f);
            }
            currentChaosIndex = saved; continue;
        }

        // Global param
        if (p->type==ParamType::FLOAT) { float cur=getParameterValue(id); float rnd=frand(p->min_val,p->max_val); float v=cur+(rnd-cur)*amount01; setParameterValue(id,std::clamp(v,p->min_val,p->max_val)); }
        else if (p->type==ParamType::INT || p->type==ParamType::ENUM){ if (frand(0.0f,1.0f)<amount01){int minI=(int)p->min_val,maxI=(int)p->max_val;int span=std::max(0,maxI-minI);int r= span==0?minI:(minI + (rand()%(span+1))); setParameterValue(id,(float)r);} }
        else { if (frand(0.0f,1.0f)<amount01){ bool cur=getParameterValue(id)>0.5f; setParameterValue(id, cur?0.0f:1.0f);} }
    }
}

void UI::mutatePageParameters(UIPage page, float amount01) {
    amount01 = std::max(0.0f, std::min(1.0f, amount01));
    if (amount01 <= 0.0f) return;
    captureUndoSnapshot("mutate_page");
    auto ids = getParameterIdsForPage(page);
    for (int id : ids) {
        InlineParameter* p = getParameter(id);
        if (!p || !p->randomizable) continue;

        auto mutateOne = [&](int /*i*/){
            if (p->type==ParamType::FLOAT){
                float cur = getParameterValue(id);
                float range = (p->max_val - p->min_val);
                float maxDelta = range * (0.3f * amount01);
                float v = std::max(p->min_val, std::min(p->max_val, cur + frand(-maxDelta, maxDelta)));
                setParameterValue(id, v);
            } else if (p->type==ParamType::INT || p->type==ParamType::ENUM){
                if (frand(0.0f,1.0f) < amount01){
                    int cur = (int)getParameterValue(id);
                    int dir = (rand()%3)-1;
                    int v = std::max((int)p->min_val, std::min((int)p->max_val, cur+dir));
                    setParameterValue(id,(float)v);
                }
            } else {
                if (frand(0.0f,1.0f) < amount01*0.25f){
                    bool cur = getParameterValue(id) > 0.5f;
                    setParameterValue(id, cur?0.0f:1.0f);
                }
            }
        };

        if (paramUsesOscIndex(id)) { int s=currentOscillatorIndex; for(int i=0;i<4;++i){ currentOscillatorIndex=i; mutateOne(i);} currentOscillatorIndex=s; continue; }
        if (paramUsesLfoIndex(id)) { int s=currentLFOIndex; for(int i=0;i<4;++i){ currentLFOIndex=i; mutateOne(i);} currentLFOIndex=s; continue; }
        if (paramUsesEnvIndex(id)) { int s=currentEnvelopeIndex; for(int i=0;i<4;++i){ currentEnvelopeIndex=i; mutateOne(i);} currentEnvelopeIndex=s; continue; }
        if (paramUsesSamplerIndex(id)) { int s=currentSamplerIndex; for(int i=0;i<4;++i){ currentSamplerIndex=i; mutateOne(i);} currentSamplerIndex=s; continue; }
        if (paramUsesChaosIndex(id)) { int s=currentChaosIndex; for(int i=0;i<4;++i){ currentChaosIndex=i; mutateOne(i);} currentChaosIndex=s; continue; }
        mutateOne(0);
    }
}

void UI::resetPageParameters(UIPage page) {
    captureUndoSnapshot("reset_page");

    // Special-case: Filter page should reset all filter-type parameters to sane defaults,
    // not just the currently visible subset, and without changing Enabled/Type.
    if (page == UIPage::FILTER) {
        // Do not alter filterEnabled (31) nor filterType (30).
        // Reset all numeric params used across filter implementations to their defaults.
        setParameterValue(32, 1000.0f);  // Cutoff Hz
        setParameterValue(33,   0.0f);   // Gain dB (shelves)
        setParameterValue(34,   0.4f);   // Resonance
        setParameterValue(35,   1.0f);   // Drive
        setParameterValue(36, 200.0f);   // Feedback HP Hz
        setParameterValue(37,   0.5f);   // Spread
        setParameterValue(38,   0.3f);   // Notch feedback
        setParameterValue(39,   0.5f);   // Band width (normalized)
        setParameterValue(42,   1.0f);   // Dry/Wet for notch
        return;
    }

    auto ids = getParameterIdsForPage(page);
    for (int id : ids) {
        InlineParameter* p = getParameter(id);
        if (!p || !p->randomizable) continue;

        auto resetOne = [&](){
            if (p->type==ParamType::FLOAT) setParameterValue(id, (p->min_val+p->max_val)*0.5f);
            else if (p->type==ParamType::INT || p->type==ParamType::ENUM) setParameterValue(id, p->min_val);
            else setParameterValue(id, 0.0f);
        };

        if (paramUsesOscIndex(id)) { int s=currentOscillatorIndex; for(int i=0;i<4;++i){ currentOscillatorIndex=i; resetOne(); } currentOscillatorIndex=s; continue; }
        if (paramUsesLfoIndex(id)) { int s=currentLFOIndex; for(int i=0;i<4;++i){ currentLFOIndex=i; resetOne(); } currentLFOIndex=s; continue; }
        if (paramUsesEnvIndex(id)) { int s=currentEnvelopeIndex; for(int i=0;i<4;++i){ currentEnvelopeIndex=i; resetOne(); } currentEnvelopeIndex=s; continue; }
        if (paramUsesSamplerIndex(id)) { int s=currentSamplerIndex; for(int i=0;i<4;++i){ currentSamplerIndex=i; resetOne(); } currentSamplerIndex=s; continue; }
        if (paramUsesChaosIndex(id)) { int s=currentChaosIndex; for(int i=0;i<4;++i){ currentChaosIndex=i; resetOne(); } currentChaosIndex=s; continue; }
        resetOne();
    }
}

float UI::getParameterValue(int id) {
    const int oscIndex = currentOscillatorIndex;
    const int lfoIndex = currentLFOIndex;
    const int envIndex = currentEnvelopeIndex;
    const int samplerIndex = currentSamplerIndex;
    const int chaosIndex = currentChaosIndex;

    switch (id) {
        case 6: return params->masterVolume.load();
        case 13: return static_cast<float>(params->getOscNoteSource(oscIndex));
        case 19: return static_cast<float>(params->getOscShape(oscIndex));
        case 11: return params->getOscFrequency(oscIndex);
        case 16: return static_cast<float>(synth->getOscillatorOctave(oscIndex));
        case 12: return params->getOscMorph(oscIndex);
        case 14: return params->getOscRatio(oscIndex);
        case 15: return params->getOscOffset(oscIndex);
        case 18: return params->getOscAmp(oscIndex);  // Changed from Level to Amp
        case 50: return params->getOscLevel(0);  // OSC 1 Level (mixer)
        case 51: return params->getOscLevel(1);  // OSC 2 Level (mixer)
        case 52: return params->getOscLevel(2);  // OSC 3 Level (mixer)
        case 53: return params->getOscLevel(3);  // OSC 4 Level (mixer)
        case 54: return synth->getSamplerLevel(0);  // SAMP 1 Level (mixer)
        case 55: return synth->getSamplerLevel(1);  // SAMP 2 Level (mixer)
        case 56: return synth->getSamplerLevel(2);  // SAMP 3 Level (mixer)
        case 57: return synth->getSamplerLevel(3);  // SAMP 4 Level (mixer)
        case 410: return params->getChaosLevel(0);   // CHAOS 1 Level (mixer)
        case 411: return params->getChaosLevel(1);   // CHAOS 2 Level (mixer)
        case 412: return params->getChaosLevel(2);   // CHAOS 3 Level (mixer)
        case 413: return params->getChaosLevel(3);   // CHAOS 4 Level (mixer)
        // SAMPLER page parameters (60-68, 80)
        case 69: return 0.0f;  // Sample name selector (special: no value)
        case 60: return synth->getSamplerKeyMode(samplerIndex) ? 0.0f : 1.0f;
        case 80: return static_cast<float>(params->getSamplerNoteSource(samplerIndex));
        case 68: return static_cast<float>(synth->getSamplerPlaybackMode(samplerIndex));
        case 61: return synth->getSamplerLoopStart(samplerIndex) * 100.0f;
        case 62: return synth->getSamplerLoopLength(samplerIndex) * 100.0f;
        case 63: return synth->getSamplerCrossfadeLength(samplerIndex) * 100.0f;
        case 64: return static_cast<float>(synth->getSamplerOctave(samplerIndex));
        case 65: return synth->getSamplerTune(samplerIndex);
        case 66: return static_cast<float>(synth->getSamplerSyncMode(samplerIndex));
        case 67: return synth->getSamplerNoteReset(samplerIndex) ? 1.0f : 0.0f;
        case 200: return params->getLfoPeriod(lfoIndex);
        case 201: return static_cast<float>(params->getLfoSyncMode(lfoIndex));
        case 202: return params->getLfoMorph(lfoIndex);
        case 203: return params->getLfoDuty(lfoIndex);
        case 204: return params->getLfoFlip(lfoIndex) ? 1.0f : 0.0f;
        case 205: return params->getLfoResetOnNote(lfoIndex) ? 1.0f : 0.0f;
        case 206: return static_cast<float>(params->getLfoShape(lfoIndex));
        case 20: return static_cast<float>(params->reverbType.load());
        case 21: return params->reverbEnabled.load() ? 1.0f : 0.0f;
        case 22: return params->reverbDelayTime.load();
        case 23: return params->reverbSize.load();
        case 24: return params->reverbDamping.load();
        case 25: return params->reverbMix.load();
        case 26: return params->reverbDecay.load();
        case 27: return params->reverbDiffusion.load();
        case 28: return params->reverbModDepth.load();
        case 29: return params->reverbModFreq.load();
        case 30: return static_cast<float>(params->filterType.load());
        case 31: return params->filterEnabled.load() ? 1.0f : 0.0f;
        case 32: return params->filterCutoff.load();
        case 33: return params->filterGain.load();
        case 34: return params->filterResonance.load();
        case 35: return params->filterDrive.load();
        case 36: return params->filterFeedbackHP.load();
        case 37: return params->filterSpread.load();
        case 38: return params->filterNotchFeedback.load();
        case 39: return params->filterBandWidth.load();
        case 42: return params->filterDryWet.load();

        // Compressor parameters (70-78)
        case 70: return params->compressorEnabled.load() ? 1.0f : 0.0f;
        case 71: return params->compressorThreshold.load();
        case 72: return params->compressorRatio.load();
        case 73: return params->compressorAttack.load();
        case 74: return params->compressorRelease.load();
        case 75: return params->compressorKnee.load();
        case 76: return params->compressorMix.load();
        case 77: return params->compressorAutoMakeup.load() ? 1.0f : 0.0f;
        case 79: return params->compressorManualMakeup.load();
        case 78: return params->compressorRMS.load() ? 1.0f : 0.0f;
        // CONFIG page parameters
        case 400: return cpuMonitor.isEnabled() ? 1.0f : 0.0f;
        // ENV page parameters (300-323)
        case 300: return params->getEnvAttack(0);
        case 301: return params->getEnvDecay(0);
        case 302: return params->getEnvSustain(0);
        case 303: return params->getEnvRelease(0);
        case 304: return params->getEnvAttackBend(0);
        case 305: return params->getEnvReleaseBend(0);
        case 306: return params->getEnvAttack(1);
        case 307: return params->getEnvDecay(1);
        case 308: return params->getEnvSustain(1);
        case 309: return params->getEnvRelease(1);
        case 310: return params->getEnvAttackBend(1);
        case 311: return params->getEnvReleaseBend(1);
        case 312: return params->getEnvAttack(2);
        case 313: return params->getEnvDecay(2);
        case 314: return params->getEnvSustain(2);
        case 315: return params->getEnvRelease(2);
        case 316: return params->getEnvAttackBend(2);
        case 317: return params->getEnvReleaseBend(2);
        case 318: return params->getEnvAttack(3);
        case 319: return params->getEnvDecay(3);
        case 320: return params->getEnvSustain(3);
        case 321: return params->getEnvRelease(3);
        case 322: return params->getEnvAttackBend(3);
        case 323: return params->getEnvReleaseBend(3);
        // CHAOS page parameters (350-353) - use currentChaosIndex
        case 350: return params->getChaosParameter(chaosIndex);
        case 351: return params->getChaosClockFreq(chaosIndex);
        case 352: return static_cast<float>(params->getChaosInterpMode(chaosIndex));
        case 353: return params->getChaosRunning(chaosIndex) ? 1.0f : 0.0f;
        case 354: return params->getChaosFastMode(chaosIndex) ? 1.0f : 0.0f;
        case 355: return params->chaosDiff.load() ? 1.0f : 0.0f;
        // FM page parameters (360-360)
        case 360: return params->fmGlobalDepth.load();
        default: return 0.0f;
    }
}

void UI::setParameterValue(int id, float value) {
    const int oscIndex = currentOscillatorIndex;
    const int lfoIndex = currentLFOIndex;
    const int envIndex = currentEnvelopeIndex;
    const int samplerIndex = currentSamplerIndex;
    const int chaosIndex = currentChaosIndex;

    switch (id) {
        case 6: params->masterVolume = value; break;
        case 13: params->setOscNoteSource(oscIndex, static_cast<int>(value)); break;
        case 19: params->setOscShape(oscIndex, static_cast<int>(value)); break;
        case 11: params->setOscFrequency(oscIndex, value); break;
        case 16: synth->setOscillatorOctave(oscIndex, static_cast<int>(value)); break;
        case 12: params->setOscMorph(oscIndex, value); break;
        case 14: params->setOscRatio(oscIndex, value); break;
        case 15: params->setOscOffset(oscIndex, value); break;
        case 18: params->setOscAmp(oscIndex, value); break;  // Changed from Level to Amp
        case 50: params->setOscLevel(0, value); break;  // OSC 1 Level (mixer)
        case 51: params->setOscLevel(1, value); break;  // OSC 2 Level (mixer)
        case 52: params->setOscLevel(2, value); break;  // OSC 3 Level (mixer)
        case 53: params->setOscLevel(3, value); break;  // OSC 4 Level (mixer)
        case 54: synth->setSamplerLevel(0, value); break;  // SAMP 1 Level (mixer)
        case 55: synth->setSamplerLevel(1, value); break;  // SAMP 2 Level (mixer)
        case 56: synth->setSamplerLevel(2, value); break;  // SAMP 3 Level (mixer)
        case 57: synth->setSamplerLevel(3, value); break;  // SAMP 4 Level (mixer)
        case 410: params->setChaosLevel(0, value); break;   // CHAOS 1 Level (mixer)
        case 411: params->setChaosLevel(1, value); break;   // CHAOS 2 Level (mixer)
        case 412: params->setChaosLevel(2, value); break;   // CHAOS 3 Level (mixer)
        case 413: params->setChaosLevel(3, value); break;   // CHAOS 4 Level (mixer)
        // SAMPLER page parameters (60-69, plus 80 for Note Source)
        case 69: break;  // Sample name selector (special: no-op, handled by Enter key)
        case 80: params->setSamplerNoteSource(samplerIndex, static_cast<int>(value)); break;
        case 60: synth->setSamplerKeyMode(samplerIndex, value < 0.5f); break;
        case 68: synth->setSamplerPlaybackMode(samplerIndex, static_cast<PlaybackMode>(static_cast<int>(value))); break;
        case 61: synth->setSamplerLoopStart(samplerIndex, value / 100.0f); break;
        case 62: synth->setSamplerLoopLength(samplerIndex, value / 100.0f); break;
        case 63: synth->setSamplerCrossfadeLength(samplerIndex, value / 100.0f); break;
        case 64: synth->setSamplerOctave(samplerIndex, static_cast<int>(value)); break;
        case 65: synth->setSamplerTune(samplerIndex, value); break;
        case 66: synth->setSamplerSyncMode(samplerIndex, static_cast<int>(value)); break;
        case 67: synth->setSamplerNoteReset(samplerIndex, value > 0.5f); break;
        case 200: params->setLfoPeriod(lfoIndex, value); break;
        case 201: params->setLfoSyncMode(lfoIndex, static_cast<int>(value)); break;
        case 202: params->setLfoMorph(lfoIndex, value); break;
        case 203: params->setLfoDuty(lfoIndex, value); break;
        case 204: params->setLfoFlip(lfoIndex, value > 0.5f); break;
        case 205: params->setLfoResetOnNote(lfoIndex, value > 0.5f); break;
        case 206: params->setLfoShape(lfoIndex, static_cast<int>(value)); break;
        case 20: params->reverbType = static_cast<int>(value); break;
        case 21: params->reverbEnabled = (value > 0.5f); break;
        case 22: params->reverbDelayTime = value; break;
        case 23: params->reverbSize = value; break;
        case 24: params->reverbDamping = value; break;
        case 25: params->reverbMix = value; break;
        case 26: params->reverbDecay = value; break;
        case 27: params->reverbDiffusion = value; break;
        case 28: params->reverbModDepth = value; break;
        case 29: params->reverbModFreq = value; break;
        case 30: params->filterType = static_cast<int>(value); break;
        case 31: params->filterEnabled = (value > 0.5f); break;
        case 32: params->filterCutoff = value; break;
        case 33: params->filterGain = value; break;
        case 34: params->filterResonance = value; break;
        case 35: params->filterDrive = value; break;
        case 36: params->filterFeedbackHP = value; break;
        case 37: params->filterSpread = value; break;
        case 38: params->filterNotchFeedback = value; break;
        case 39: params->filterBandWidth = value; break;
        case 42: params->filterDryWet = value; break;

        // Compressor parameters (70-78)
        case 70:
            params->compressorEnabled = (value > 0.5f);
            if (synth) synth->setCompressorEnabled(value > 0.5f);
            break;
        case 71:
            params->compressorThreshold = value;
            if (synth) synth->updateCompressorParameters(
                params->compressorThreshold.load(), params->compressorRatio.load(),
                params->compressorAttack.load(), params->compressorRelease.load(),
                params->compressorKnee.load(), params->compressorMix.load(),
                params->compressorAutoMakeup.load(), params->compressorManualMakeup.load(),
                params->compressorRMS.load());
            break;
        case 72:
            params->compressorRatio = value;
            if (synth) synth->updateCompressorParameters(
                params->compressorThreshold.load(), params->compressorRatio.load(),
                params->compressorAttack.load(), params->compressorRelease.load(),
                params->compressorKnee.load(), params->compressorMix.load(),
                params->compressorAutoMakeup.load(), params->compressorManualMakeup.load(),
                params->compressorRMS.load());
            break;
        case 73:
            params->compressorAttack = value;
            if (synth) synth->updateCompressorParameters(
                params->compressorThreshold.load(), params->compressorRatio.load(),
                params->compressorAttack.load(), params->compressorRelease.load(),
                params->compressorKnee.load(), params->compressorMix.load(),
                params->compressorAutoMakeup.load(), params->compressorManualMakeup.load(),
                params->compressorRMS.load());
            break;
        case 74:
            params->compressorRelease = value;
            if (synth) synth->updateCompressorParameters(
                params->compressorThreshold.load(), params->compressorRatio.load(),
                params->compressorAttack.load(), params->compressorRelease.load(),
                params->compressorKnee.load(), params->compressorMix.load(),
                params->compressorAutoMakeup.load(), params->compressorManualMakeup.load(),
                params->compressorRMS.load());
            break;
        case 75:
            params->compressorKnee = value;
            if (synth) synth->updateCompressorParameters(
                params->compressorThreshold.load(), params->compressorRatio.load(),
                params->compressorAttack.load(), params->compressorRelease.load(),
                params->compressorKnee.load(), params->compressorMix.load(),
                params->compressorAutoMakeup.load(), params->compressorManualMakeup.load(),
                params->compressorRMS.load());
            break;
        case 76:
            params->compressorMix = value;
            if (synth) synth->updateCompressorParameters(
                params->compressorThreshold.load(), params->compressorRatio.load(),
                params->compressorAttack.load(), params->compressorRelease.load(),
                params->compressorKnee.load(), params->compressorMix.load(),
                params->compressorAutoMakeup.load(), params->compressorManualMakeup.load(),
                params->compressorRMS.load());
            break;
        case 77:
            params->compressorAutoMakeup = (value > 0.5f);
            if (synth) synth->updateCompressorParameters(
                params->compressorThreshold.load(), params->compressorRatio.load(),
                params->compressorAttack.load(), params->compressorRelease.load(),
                params->compressorKnee.load(), params->compressorMix.load(),
                params->compressorAutoMakeup.load(), params->compressorManualMakeup.load(),
                params->compressorRMS.load());
            break;
        case 79:
            params->compressorManualMakeup = value;
            if (synth) synth->updateCompressorParameters(
                params->compressorThreshold.load(), params->compressorRatio.load(),
                params->compressorAttack.load(), params->compressorRelease.load(),
                params->compressorKnee.load(), params->compressorMix.load(),
                params->compressorAutoMakeup.load(), params->compressorManualMakeup.load(),
                params->compressorRMS.load());
            break;
        case 78:
            params->compressorRMS = (value > 0.5f);
            if (synth) synth->updateCompressorParameters(
                params->compressorThreshold.load(), params->compressorRatio.load(),
                params->compressorAttack.load(), params->compressorRelease.load(),
                params->compressorKnee.load(), params->compressorMix.load(),
                params->compressorAutoMakeup.load(), params->compressorManualMakeup.load(),
                params->compressorRMS.load());
            break;
        // CONFIG page parameters
        case 400: cpuMonitor.setEnabled(value > 0.5f); break;
        // ENV page parameters (300-323)
        case 300: params->setEnvAttack(0, value); params->attack = value; break;
        case 301: params->setEnvDecay(0, value); params->decay = value; break;
        case 302: params->setEnvSustain(0, value); params->sustain = value; break;
        case 303: params->setEnvRelease(0, value); params->release = value; break;
        case 304: params->setEnvAttackBend(0, value); break;
        case 305: params->setEnvReleaseBend(0, value); break;
        case 306: params->setEnvAttack(1, value); break;
        case 307: params->setEnvDecay(1, value); break;
        case 308: params->setEnvSustain(1, value); break;
        case 309: params->setEnvRelease(1, value); break;
        case 310: params->setEnvAttackBend(1, value); break;
        case 311: params->setEnvReleaseBend(1, value); break;
        case 312: params->setEnvAttack(2, value); break;
        case 313: params->setEnvDecay(2, value); break;
        case 314: params->setEnvSustain(2, value); break;
        case 315: params->setEnvRelease(2, value); break;
        case 316: params->setEnvAttackBend(2, value); break;
        case 317: params->setEnvReleaseBend(2, value); break;
        case 318: params->setEnvAttack(3, value); break;
        case 319: params->setEnvDecay(3, value); break;
        case 320: params->setEnvSustain(3, value); break;
        case 321: params->setEnvRelease(3, value); break;
        case 322: params->setEnvAttackBend(3, value); break;
        case 323: params->setEnvReleaseBend(3, value); break;
        // CHAOS page parameters (350-353) - use currentChaosIndex
        case 350: params->setChaosParameter(chaosIndex, value); synth->setChaosParameter(chaosIndex, value); break;
        case 351: {
            // Full range from 0.0001Hz to 20kHz regardless of fast/slow mode
            // Fast mode: per-sample iteration, Slow mode: per-block iteration
            float clamped = std::clamp(value, 0.0001f, 20000.0f);
            params->setChaosClockFreq(chaosIndex, clamped);
            synth->setChaosClockFreq(chaosIndex, clamped);
            break;
        }
        case 352: params->setChaosInterpMode(chaosIndex, static_cast<int>(value)); synth->setChaosInterpMode(chaosIndex, static_cast<int>(value)); break;
        case 353: params->setChaosRunning(chaosIndex, value > 0.5f); break;
        case 354: params->setChaosFastMode(chaosIndex, value > 0.5f); synth->setChaosFastMode(chaosIndex, value > 0.5f); break;
        case 355: params->chaosDiff = (value > 0.5f); if (synth) synth->setChaosDiffMode(value > 0.5f); break;
        // FM page parameters (360-360)
        case 360: params->fmGlobalDepth.store(std::clamp(value, 0.0f, 1.0f)); break;
    }

    // Reset MIDI pickup state when parameter is changed via UI
    // This ensures MIDI controller must pick up the new value before controlling the parameter
    if (id >= 0 && id < SynthParameters::kMaxParamMap) {
        params->parameterCCPickedUp[id] = false;
    }

    notifyAutosaveNeeded("param_change");
}

void UI::adjustParameter(int id, bool increase, bool fine) {
    InlineParameter* param = getParameter(id);
    if (!param) return;
    captureUndoSnapshot("adjust_parameter");

    float currentValue = getParameterValue(id);
    float newValue = currentValue;

    switch (param->type) {
        case ParamType::FLOAT: {
            float range = param->max_val - param->min_val;
            float coarseStep = 0.05f * range;
            float linearStep = fine ? (coarseStep * 0.1f) : coarseStep;
            if (linearStep <= 0.0f) {
                linearStep = fine ? 0.001f : 0.01f; // fallback
            }

            auto clampValue = [&](float value) {
                return std::clamp(value, param->min_val, param->max_val);
            };

            auto adjustLinear = [&]() {
                if (increase) {
                    newValue = clampValue(currentValue + linearStep);
                } else {
                    newValue = clampValue(currentValue - linearStep);
                }
            };

            auto adjustMultiplicative = [&](float coarseFactor) {
                float factor = fine ? (1.0f + (coarseFactor - 1.0f) * 0.1f) : coarseFactor;
                factor = std::max(factor, 1.0f);
                float baseline = std::max(currentValue, param->min_val);
                if (increase) {
                    newValue = clampValue(baseline * factor);
                } else {
                    newValue = clampValue(baseline / factor);
                }
            };

            bool isEnvelopeTime =
                (id >= 300 && id <= 323) &&
                ((id % 6 == 0) || (id % 6 == 1) || (id % 6 == 3));

            if (id == 351) { // Chaos Clock
                // Full range from 0.0001Hz to 20kHz regardless of fast/slow mode
                // Use multiplicative adjustment for better control across the wide range
                float factorCoarse = 1.3f;
                float factor = fine ? (1.0f + (factorCoarse - 1.0f) * 0.1f) : factorCoarse;
                float base = std::max(currentValue, 0.0001f);
                newValue = increase ? base * factor : base / factor;
                newValue = std::clamp(newValue, 0.0001f, 20000.0f);
            } else if (isEnvelopeTime) { // Envelope attack/decay/release
                // Exponential acceleration: as value grows toward max, increase factor
                float minv = std::max(param->min_val, 0.000001f);
                float maxv = std::max(param->max_val, minv * 2.0f);
                float base = std::max(currentValue, minv);
                float pos = 0.0f;
                float span = std::log(maxv / minv);
                if (span > 0.0f) {
                    pos = std::clamp(std::log(base / minv) / span, 0.0f, 1.0f);
                }
                // Factor ranges roughly [1.15 .. 1.50]
                float factorCoarse = 1.15f + 0.35f * pos;
                float factor = fine ? (1.0f + (factorCoarse - 1.0f) * 0.1f) : factorCoarse;
                newValue = increase ? base * factor : base / factor;
                newValue = std::clamp(newValue, param->min_val, param->max_val);
            } else if (id == 11) { // Oscillator frequency - semitone steps
                adjustMultiplicative(1.122462f);
            } else if (id == 14) { // Oscillator ratio
                adjustMultiplicative(1.25f);
            } else if (id == 19) { // Shape - discrete selection
                int current = static_cast<int>(std::round(currentValue));
                current += increase ? 1 : -1;
                current = std::clamp(current, static_cast<int>(param->min_val), static_cast<int>(param->max_val));
                newValue = static_cast<float>(current);
            } else if (id == 32) { // Filter cutoff
                adjustMultiplicative(1.3f);
            } else if (id == 35) { // Filter drive - fine increments (0.2 coarse, 0.05 fine)
                float step = fine ? 0.02f : 0.2f;
                if (increase) {
                    newValue = clampValue(currentValue + step);
                } else {
                    newValue = clampValue(currentValue - step);
                }
            } else if (id == 200) { // LFO period
                adjustMultiplicative(1.3f);
            } else {
                adjustLinear();
            }
            break;
        }
        case ParamType::INT: {
            int rangeInt = static_cast<int>(std::round(param->max_val - param->min_val));
            int coarseStep = std::max(1, rangeInt / 20);
            int step = fine ? 1 : coarseStep;
            int intValue = static_cast<int>(currentValue);
            if (increase) {
                intValue = std::min(static_cast<int>(param->max_val), intValue + step);
            } else {
                intValue = std::max(static_cast<int>(param->min_val), intValue - step);
            }
            newValue = static_cast<float>(intValue);
            break;
        }
        case ParamType::ENUM: {
            int enumValue = static_cast<int>(currentValue);
            int minEnum = static_cast<int>(param->min_val);
            int maxEnum = static_cast<int>(param->max_val);
            bool clampBounds = (id == 13 || id == 80);

            if (increase) {
                if (enumValue < maxEnum) {
                    enumValue++;
                } else if (!clampBounds) {
                    enumValue = minEnum;  // Wrap to beginning
                }
            } else {
                if (enumValue > minEnum) {
                    enumValue--;
                } else if (!clampBounds) {
                    enumValue = maxEnum;  // Wrap to end
                }
            }
            newValue = static_cast<float>(enumValue);
            break;
        }
        case ParamType::BOOL: {
            newValue = (currentValue > 0.5f) ? 0.0f : 1.0f;
            break;
        }
    }

    setParameterValue(id, newValue);
}

std::string UI::getParameterDisplayString(int id) {
    InlineParameter* param = getParameter(id);
    if (!param) return "";

    float value = getParameterValue(id);

    switch (param->type) {
        case ParamType::FLOAT: {
            if (param->unit.empty()) {
                return std::to_string(value);
            } else {
                return std::to_string(value) + " " + param->unit;
            }
        }
        case ParamType::INT: {
            return std::to_string(static_cast<int>(value));
        }
        case ParamType::ENUM: {
            int enumIndex = static_cast<int>(value);
            if (enumIndex >= 0 && enumIndex < static_cast<int>(param->enum_values.size())) {
                return param->enum_values[enumIndex];
            }
            return "Unknown";
        }
        case ParamType::BOOL: {
            return (value > 0.5f) ? "ON" : "OFF";
        }
    }
    return "";
}

void UI::startNumericInput(int id) {
    numericInputActive = true;
    selectedParameterId = id;
    numericInputBuffer.clear();
    numericInputIsSequencer = false;
    sequencerNumericContext = SequencerNumericContext();
}

void UI::finishNumericInput() {
    if (!numericInputActive) return;
    if (numericInputIsFM) {
        if (!numericInputBuffer.empty() && fmNumericTarget >= 0 && fmNumericSource >= 0) {
            // Accept values like -99, 50, 25%, etc.
            std::string txt = numericInputBuffer;
            // Strip trailing % and spaces
            while (!txt.empty() && (txt.back() == '%' || std::isspace(static_cast<unsigned char>(txt.back())))) {
                txt.pop_back();
            }
            try {
                float pct = std::stof(txt);
                pct = std::clamp(pct, -99.0f, 99.0f);
                float depth = pct / 100.0f;
                captureUndoSnapshot("fm_numeric_input");
                params->setFMDepth(fmNumericTarget, fmNumericSource, depth);
            } catch (...) {
                // ignore invalid
            }
        }
        numericInputIsFM = false;
        fmNumericTarget = fmNumericSource = -1;
        numericInputActive = false;
        numericInputBuffer.clear();
        return;
    }
    if (numericInputIsSequencer) {
        if (!numericInputBuffer.empty()) {
            applySequencerNumericInput(numericInputBuffer);
        }
        numericInputIsSequencer = false;
        sequencerNumericContext = SequencerNumericContext();
        numericInputActive = false;
        numericInputBuffer.clear();
        return;
    }

    InlineParameter* param = getParameter(selectedParameterId);
    if (param && !numericInputBuffer.empty()) {
        try {
            float value = std::stof(numericInputBuffer);
            value = std::max(param->min_val, std::min(param->max_val, value));
            captureUndoSnapshot("numeric_input");
            setParameterValue(selectedParameterId, value);
        } catch (...) {
            // Invalid input, ignore
        }
    }

    numericInputActive = false;
    numericInputBuffer.clear();
}

bool UI::handleMidiNoteForFrequencyInput(int midiNote) {
    // Only consume if we're in numeric input mode editing the frequency parameter (ID 11)
    // and the current oscillator is configured for free running (Note Source = None)
    if (!numericInputActive || selectedParameterId != 11) {
        return false;
    }

    // Check if current oscillator uses Note Source = Free (free running mode)
    if (!params || params->getOscNoteSource(currentOscillatorIndex) != static_cast<int>(NoteSource::FREE)) {
        return false;
    }

    // Clamp to valid MIDI range
    int clamped = std::max(0, std::min(127, midiNote));

    // Convert MIDI note to frequency
    if (synth) {
        float frequency = 440.0f * std::pow(2.0f, (clamped - 69) / 12.0f);

        // Format frequency with appropriate precision
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << frequency;
        numericInputBuffer = oss.str();

        // Immediately apply the value
        finishNumericInput();
        return true;
    }

    return false;
}

bool UI::handleMidiNoteForTuneInput(int midiNote) {
    // Only consume if we're in numeric input mode editing the sampler tune parameter (ID 65)
    if (!numericInputActive || selectedParameterId != 65) {
        return false;
    }

    // Clamp to valid MIDI range
    int clamped = std::max(0, std::min(127, midiNote));

    // Convert MIDI note to tune value in semitones relative to C4 (60)
    // Tune range is -60.0 to +67.0 semitones (full MIDI keyboard)
    // C4 (60) = 0.0 st, C0 (0) = -60.0 st, G10 (127) = +67.0 st
    float tuneValue = static_cast<float>(clamped - 60);

    // Format tune value with appropriate precision
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << tuneValue;
    numericInputBuffer = oss.str();

    // Immediately apply the value
    finishNumericInput();
    return true;
}

void UI::startMidiLearn(int id) {
    InlineParameter* param = getParameter(id);
    if (param && param->supports_midi_learn) {
        params->midiLearnActive = true;
        params->midiLearnParameterId = id;

        // Save current context indices for this parameter
        params->parameterContextOsc[id] = currentOscillatorIndex;
        params->parameterContextLFO[id] = currentLFOIndex;
        params->parameterContextEnv[id] = currentEnvelopeIndex;
        params->parameterContextSampler[id] = currentSamplerIndex;
        params->parameterContextChaos[id] = currentChaosIndex;

        // Record start time for timeout
        auto now = std::chrono::steady_clock::now();
        double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();
        params->midiLearnStartTime = currentTime;

        addConsoleMessage("MIDI Learn: " + param->name + " (10s timeout) - move a controller");
    } else if (param && !param->supports_midi_learn) {
        addConsoleMessage("MIDI Learn not available for " + param->name);
    }
}

void UI::finishMidiLearn() {
    params->midiLearnActive = false;
    params->midiLearnParameterId = -1;
}

std::string UI::getParameterName(int id) {
    InlineParameter* param = getParameter(id);
    return param ? param->name : "Unknown";
}

bool UI::isParameterModulated(int id) {
    // Map parameter ID to modulation destination index
    // Modulation destinations are grouped by module. Key ranges:
    // 0-19: Oscillator parameters (Pitch, Morph, Ratio, Offset, Amp)
    // 20-26: Filter Cutoff/Resonance/Drive/Width/Notch FB/Spread/DryWet
    // 27-28: Reverb Mix/Size
    // 29-48: Sampler parameters (Pitch, Loop Start/Length, Crossfade, Level)
    // 49-60: LFO parameters (Rate, Morph, Duty)
    // 61-69: Mixer parameters (Master volume and source levels)

    int modDestination = -1;

    // Determine modulation destination based on parameter ID and current oscillator index
    switch (id) {
        // Oscillator parameters (ID 10-18) - need to map to correct osc based on currentOscillatorIndex
        case 19: // Shape - not modulatable
        case 16: // Octave - not modulatable
            return false;
        case 11: // Frequency/Pitch
            modDestination = currentOscillatorIndex * 5 + 0;
            break;
        case 12: // Morph
            modDestination = currentOscillatorIndex * 5 + 1;
            break;
        case 14: // Ratio
            modDestination = currentOscillatorIndex * 5 + 2;
            break;
        case 15: // Offset
            modDestination = currentOscillatorIndex * 5 + 3;
            break;
        case 18: // Amp (changed from Level)
            modDestination = currentOscillatorIndex * 5 + 4;
            break;
        case 32: // Filter Cutoff
            modDestination = 20;
            break;
        case 25: // Reverb Mix
            modDestination = 27;
            break;
        case 23: // Reverb Size
            modDestination = 28;
            break;
        case 6: // Master Volume
            modDestination = 61;
            break;
        case 50: // Mixer OSC 1 Level
        case 51: // Mixer OSC 2 Level
        case 52: // Mixer OSC 3 Level
        case 53: // Mixer OSC 4 Level
            modDestination = 62 + (id - 50);
            break;
        case 54: // Mixer SAMP 1 Level
        case 55: // Mixer SAMP 2 Level
        case 56: // Mixer SAMP 3 Level
        case 57: // Mixer SAMP 4 Level
            modDestination = 66 + (id - 54);
            break;
        case 200: { // LFO Rate
            int lfoIndex = currentLFOIndex;
            modDestination = 49 + lfoIndex * 3;
            break;
        }
        case 202: { // LFO Morph
            int lfoIndex = currentLFOIndex;
            modDestination = 50 + lfoIndex * 3;
            break;
        }
        case 203: { // LFO Duty
            int lfoIndex = currentLFOIndex;
            modDestination = 51 + lfoIndex * 3;
            break;
        }
        default:
            return false;
    }

    // Check all 16 modulation slots for this destination
    for (int i = 0; i < 16; ++i) {
        const ModulationSlot& slot = modulationSlots[i];
        if (slot.isComplete() && slot.destination == modDestination && slot.amount != 0) {
            return true;
        }
    }

    return false;
}
