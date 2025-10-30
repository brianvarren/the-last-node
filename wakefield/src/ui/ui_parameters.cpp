#include "../ui.h"
#include "../synth.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <string>

/*
 * RANDOMIZATION WHITELIST POLICY
 *
 * The 'randomizable' flag determines which parameters can be affected by
 * global randomize and mutate operations.
 *
 * IMMUNE (randomizable = false):
 * - Master Volume (prevents sudden loud sounds)
 * - Mix levels (OSC/SAMP levels - prevents silencing active sources)
 * - Mute/Solo states (not in parameter list, but immune via separate handling)
 * - CPU Monitor toggle
 * - Special UI controls (sample selector, looper controls)
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

    // ============================================================================
    // MAIN PAGE - IMMUNE (prevent volume disasters)
    // ============================================================================
    parameters.push_back({6, ParamType::FLOAT, "Master Volume", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MAIN), false});

    // ============================================================================
    // OSCILLATOR PAGE - MOSTLY RANDOMIZABLE (except mode)
    // ============================================================================
    parameters.push_back({10, ParamType::ENUM, "Mode", "", 0, 1, {"FREE", "KEY"}, true, static_cast<int>(UIPage::OSCILLATOR), false});  // IMMUNE
    parameters.push_back({19, ParamType::ENUM, "Shape", "", 0, 1, {"Saw", "Pulse"}, true, static_cast<int>(UIPage::OSCILLATOR), true});
    parameters.push_back({11, ParamType::FLOAT, "Frequency", "Hz", 20.0f, 2000.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR), true});
    parameters.push_back({12, ParamType::FLOAT, "Morph", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR), true});
    parameters.push_back({13, ParamType::FLOAT, "Duty", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR), true});
    parameters.push_back({14, ParamType::FLOAT, "Ratio", "", 0.125f, 16.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR), true});
    parameters.push_back({15, ParamType::FLOAT, "Offset", "Hz", -1000.0f, 1000.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR), true});
    parameters.push_back({18, ParamType::FLOAT, "Amp", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR), true});

    // ============================================================================
    // LFO PAGE - ALL RANDOMIZABLE (modulation sources)
    // ============================================================================
    parameters.push_back({200, ParamType::FLOAT, "Period", "s", 0.1f, 1800.0f, {}, true, static_cast<int>(UIPage::LFO), true});
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
    parameters.push_back({32, ParamType::FLOAT, "Cutoff", "Hz", 20.0f, 20000.0f, {}, true, static_cast<int>(UIPage::FILTER), true});
    parameters.push_back({33, ParamType::FLOAT, "Gain", "dB", -24.0f, 24.0f, {}, true, static_cast<int>(UIPage::FILTER), true});
    parameters.push_back({34, ParamType::FLOAT, "Resonance", "", 0.0f, 1.2f, {}, true, static_cast<int>(UIPage::FILTER), true});
    parameters.push_back({35, ParamType::FLOAT, "Drive", "", 0.1f, 15.0f, {}, true, static_cast<int>(UIPage::FILTER), true});
    parameters.push_back({36, ParamType::FLOAT, "FB HP", "Hz", 10.0f, 6000.0f, {}, true, static_cast<int>(UIPage::FILTER), true});
    parameters.push_back({37, ParamType::FLOAT, "Spread", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::FILTER), true});
    parameters.push_back({38, ParamType::FLOAT, "Notch FB", "", 0.0f, 0.95f, {}, true, static_cast<int>(UIPage::FILTER), true});
    parameters.push_back({39, ParamType::FLOAT, "Width", "", 0.05f, 0.95f, {}, true, static_cast<int>(UIPage::FILTER), true});
    parameters.push_back({42, ParamType::FLOAT, "Dry/Wet", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::FILTER), true});

    // ============================================================================
    // LOOPER PAGE - IMMUNE (special UI controls, performance-critical)
    // ============================================================================
    parameters.push_back({40, ParamType::INT, "Current Loop", "", 0, 3, {}, true, static_cast<int>(UIPage::LOOPER), false});  // IMMUNE
    parameters.push_back({41, ParamType::FLOAT, "Overdub Mix", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::LOOPER), false});  // IMMUNE

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
    parameters.push_back({60, ParamType::ENUM, "Key Mode", "", 0, 1, {"KEY", "FREE"}, true, static_cast<int>(UIPage::SAMPLER), false});  // IMMUNE
    parameters.push_back({68, ParamType::ENUM, "Direction", "", 0, 2, {"Forward", "Reverse", "Ping-Pong"}, true, static_cast<int>(UIPage::SAMPLER), true});
    parameters.push_back({61, ParamType::FLOAT, "Loop Start", "%", 0.0f, 100.0f, {}, true, static_cast<int>(UIPage::SAMPLER), true});
    parameters.push_back({62, ParamType::FLOAT, "Loop Length", "%", 0.0f, 100.0f, {}, true, static_cast<int>(UIPage::SAMPLER), true});
    parameters.push_back({63, ParamType::FLOAT, "Xfade", "%", 0.0f, 100.0f, {}, true, static_cast<int>(UIPage::SAMPLER), true});
    parameters.push_back({64, ParamType::INT, "Octave", "", -5, 5, {}, true, static_cast<int>(UIPage::SAMPLER), true});
    parameters.push_back({65, ParamType::FLOAT, "Tune", "", -1.0f, 1.0f, {}, true, static_cast<int>(UIPage::SAMPLER), true});
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
    parameters.push_back({300, ParamType::FLOAT, "Attack", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({301, ParamType::FLOAT, "Decay", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({302, ParamType::FLOAT, "Sustain", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({303, ParamType::FLOAT, "Release", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({304, ParamType::FLOAT, "Atk Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({305, ParamType::FLOAT, "Rel Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    // Envelope 2: 306-311
    parameters.push_back({306, ParamType::FLOAT, "Attack", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({307, ParamType::FLOAT, "Decay", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({308, ParamType::FLOAT, "Sustain", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({309, ParamType::FLOAT, "Release", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({310, ParamType::FLOAT, "Atk Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({311, ParamType::FLOAT, "Rel Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    // Envelope 3: 312-317
    parameters.push_back({312, ParamType::FLOAT, "Attack", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({313, ParamType::FLOAT, "Decay", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({314, ParamType::FLOAT, "Sustain", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({315, ParamType::FLOAT, "Release", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({316, ParamType::FLOAT, "Atk Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({317, ParamType::FLOAT, "Rel Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    // Envelope 4: 318-323
    parameters.push_back({318, ParamType::FLOAT, "Attack", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({319, ParamType::FLOAT, "Decay", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({320, ParamType::FLOAT, "Sustain", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({321, ParamType::FLOAT, "Release", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({322, ParamType::FLOAT, "Atk Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});
    parameters.push_back({323, ParamType::FLOAT, "Rel Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV), true});

    // ============================================================================
    // CHAOS PAGE - RANDOMIZABLE (except running state)
    // ============================================================================
    parameters.push_back({350, ParamType::FLOAT, "Chaos", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::CHAOS), true});
    parameters.push_back({351, ParamType::FLOAT, "Clock", "Hz", 0.0001f, 20000.0f, {}, true, static_cast<int>(UIPage::CHAOS), true});
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
        case 10: case 11: case 12: case 13: case 14: case 15: case 18: case 19:
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
    // SAMPLER page (60-69). 69 is a file picker placeholder; it's never randomizable.
    return (id >= 60 && id <= 68);
}

static inline bool paramUsesChaosIndex(int id) {
    return (id >= 350 && id <= 353);
}

// Generate a random float in [a,b]
static inline float frand(float a, float b) {
    float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return a + (b - a) * r;
}

void UI::randomizeAllParameters(float amount01) {
    amount01 = std::clamp(amount01, 0.0f, 1.0f);
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
}

void UI::mutateAllParameters(float amount01) {
    amount01 = std::max(0.0f, std::min(1.0f, amount01));
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
        case 10: return static_cast<float>(params->getOscMode(oscIndex));
        case 19: return static_cast<float>(params->getOscShape(oscIndex));
        case 11: return params->getOscFrequency(oscIndex);
        case 12: return params->getOscMorph(oscIndex);
        case 13: return params->getOscDuty(oscIndex);
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
        // SAMPLER page parameters (60-69)
        case 69: return 0.0f;  // Sample name selector (special: no value)
        case 60: return synth->getSamplerKeyMode(samplerIndex) ? 0.0f : 1.0f;
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
        case 40: return static_cast<float>(params->currentLoop.load());
        case 41: return params->overdubMix.load();
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
        case 10: setOscMode(osc, static_cast<int>(value)); break;
        case 19: setOscShape(osc, static_cast<int>(value)); break;
        case 11: setOscFrequency(osc, value); break;
        case 12: setOscMorph(osc, value); break;
        case 13: setOscDuty(osc, value); break;
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
        case 40: currentLoop = static_cast<int>(value); break;
        case 41: overdubMix = value; break;
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

void UI::setParameterValue(int id, float value) {
    if (!params) return;

    if (id == 400) {
        cpuMonitor.setEnabled(value > 0.5f);
        return;
    }

    params->applyParameterValue(id,
                                value,
                                synth,
                                currentOscillatorIndex,
                                currentLFOIndex,
                                currentEnvelopeIndex,
                                currentSamplerIndex,
                                currentChaosIndex);
}

void UI::adjustParameter(int id, bool increase, bool fine) {
    InlineParameter* param = getParameter(id);
    if (!param) return;

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
                adjustMultiplicative(1.3f);
            } else if (id == 11) { // Oscillator frequency - semitone steps
                adjustMultiplicative(1.122462f);
            } else if (id == 14) { // Oscillator ratio
                adjustMultiplicative(1.25f);
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
            int range = maxEnum - minEnum + 1;

            if (increase) {
                enumValue++;
                if (enumValue > maxEnum) {
                    enumValue = minEnum;  // Wrap to beginning
                }
            } else {
                enumValue--;
                if (enumValue < minEnum) {
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
            setParameterValue(selectedParameterId, value);
        } catch (...) {
            // Invalid input, ignore
        }
    }

    numericInputActive = false;
    numericInputBuffer.clear();
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
    // 0-23: Oscillator parameters (Pitch, Morph, Duty, Ratio, Offset, Amp)
    // 24-25: Filter Cutoff/Resonance
    // 26-27: Reverb Mix/Size
    // 28-47: Sampler parameters (Pitch, Loop Start/Length, Crossfade, Level)
    // 48-59: LFO parameters (Rate, Morph, Duty)
    // 60-68: Mixer parameters (Master volume and source levels)

    int modDestination = -1;

    // Determine modulation destination based on parameter ID and current oscillator index
    switch (id) {
        // Oscillator parameters (ID 10-18) - need to map to correct osc based on currentOscillatorIndex
        case 10: // Mode - not modulatable
        case 19: // Shape - not modulatable
            return false;
        case 11: // Frequency/Pitch
            modDestination = currentOscillatorIndex * 6 + 0;
            break;
        case 12: // Morph
            modDestination = currentOscillatorIndex * 6 + 1;
            break;
        case 13: // Duty
            modDestination = currentOscillatorIndex * 6 + 2;
            break;
        case 14: // Ratio
            modDestination = currentOscillatorIndex * 6 + 3;
            break;
        case 15: // Offset
            modDestination = currentOscillatorIndex * 6 + 4;
            break;
        case 18: // Amp (changed from Level)
            modDestination = currentOscillatorIndex * 6 + 5;
            break;
        case 32: // Filter Cutoff
            modDestination = 24;
            break;
        case 25: // Reverb Mix
            modDestination = 26;
            break;
        case 23: // Reverb Size
            modDestination = 27;
            break;
        case 6: // Master Volume
            modDestination = 60;
            break;
        case 50: // Mixer OSC 1 Level
        case 51: // Mixer OSC 2 Level
        case 52: // Mixer OSC 3 Level
        case 53: // Mixer OSC 4 Level
            modDestination = 61 + (id - 50);
            break;
        case 54: // Mixer SAMP 1 Level
        case 55: // Mixer SAMP 2 Level
        case 56: // Mixer SAMP 3 Level
        case 57: // Mixer SAMP 4 Level
            modDestination = 65 + (id - 54);
            break;
        case 200: { // LFO Rate
            int lfoIndex = currentLFOIndex;
            modDestination = 48 + lfoIndex * 3;
            break;
        }
        case 202: { // LFO Morph
            int lfoIndex = currentLFOIndex;
            modDestination = 49 + lfoIndex * 3;
            break;
        }
        case 203: { // LFO Duty
            int lfoIndex = currentLFOIndex;
            modDestination = 50 + lfoIndex * 3;
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
