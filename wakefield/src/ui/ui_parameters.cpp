#include "../ui.h"
#include <algorithm>
#include <chrono>
#include <string>

void UI::initializeParameters() {
    parameters.clear();

    // Global parameters
    parameters.push_back({6, ParamType::FLOAT, "Master Volume", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::CONFIG)});

    // OSCILLATOR page parameters - control the currently selected oscillator
    parameters.push_back({10, ParamType::ENUM, "Mode", "", 0, 1, {"FREE", "KEY"}, true, static_cast<int>(UIPage::OSCILLATOR)});
    parameters.push_back({19, ParamType::ENUM, "Shape", "", 0, 1, {"Saw", "Pulse"}, true, static_cast<int>(UIPage::OSCILLATOR)});
    parameters.push_back({11, ParamType::FLOAT, "Frequency", "Hz", 20.0f, 2000.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR)});
    parameters.push_back({12, ParamType::FLOAT, "Morph", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR)});
    parameters.push_back({13, ParamType::FLOAT, "Duty", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR)});
    parameters.push_back({14, ParamType::FLOAT, "Ratio", "", 0.125f, 16.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR)});
    parameters.push_back({15, ParamType::FLOAT, "Offset", "Hz", -1000.0f, 1000.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR)});
    parameters.push_back({18, ParamType::FLOAT, "Level", "", -1.0f, 1.0f, {}, true, static_cast<int>(UIPage::OSCILLATOR)});

    // LFO page parameters - control the currently selected LFO
    parameters.push_back({200, ParamType::FLOAT, "Period", "s", 0.1f, 1800.0f, {}, true, static_cast<int>(UIPage::LFO)});
    parameters.push_back({201, ParamType::ENUM, "Sync", "", 0, 3, {"Off", "On", "Trip", "Dot"}, true, static_cast<int>(UIPage::LFO)});
    parameters.push_back({206, ParamType::ENUM, "Shape", "", 0, 1, {"Saw", "Pulse"}, true, static_cast<int>(UIPage::LFO)});
    parameters.push_back({202, ParamType::FLOAT, "Morph", "", 0.0001f, 0.9999f, {}, true, static_cast<int>(UIPage::LFO)});
    parameters.push_back({203, ParamType::FLOAT, "Duty", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::LFO)});
    parameters.push_back({204, ParamType::BOOL, "Flip", "", 0, 1, {}, true, static_cast<int>(UIPage::LFO)});
    parameters.push_back({205, ParamType::BOOL, "Reset On Note", "", 0, 1, {}, true, static_cast<int>(UIPage::LFO)});

    // REVERB page parameters - ALL support MIDI learn
    parameters.push_back({20, ParamType::ENUM, "Reverb Type", "", 0, 4, {"Greyhole", "Plate", "Room", "Hall", "Spring"}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({21, ParamType::BOOL, "Reverb Enabled", "", 0, 1, {}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({22, ParamType::FLOAT, "Delay Time", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({23, ParamType::FLOAT, "Size", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({24, ParamType::FLOAT, "Damping", "", 0.0f, 0.99f, {}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({25, ParamType::FLOAT, "Mix", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({26, ParamType::FLOAT, "Decay", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({27, ParamType::FLOAT, "Diffusion", "", 0.0f, 0.99f, {}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({28, ParamType::FLOAT, "Mod Depth", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({29, ParamType::FLOAT, "Mod Freq", "", 0.0f, 10.0f, {}, true, static_cast<int>(UIPage::REVERB)});

    // FILTER page parameters - ALL support MIDI learn
    parameters.push_back({30, ParamType::ENUM, "Filter Type", "", 0, 3, {"Lowpass", "Highpass", "High Shelf", "Low Shelf"}, true, static_cast<int>(UIPage::FILTER)});
    parameters.push_back({31, ParamType::BOOL, "Filter Enabled", "", 0, 1, {}, true, static_cast<int>(UIPage::FILTER)});
    parameters.push_back({32, ParamType::FLOAT, "Cutoff", "Hz", 20.0f, 20000.0f, {}, true, static_cast<int>(UIPage::FILTER)});
    parameters.push_back({33, ParamType::FLOAT, "Gain", "dB", -24.0f, 24.0f, {}, true, static_cast<int>(UIPage::FILTER)});

    // LOOPER page parameters - ALL support MIDI learn
    parameters.push_back({40, ParamType::INT, "Current Loop", "", 0, 3, {}, true, static_cast<int>(UIPage::LOOPER)});
    parameters.push_back({41, ParamType::FLOAT, "Overdub Mix", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::LOOPER)});

    // CONFIG page parameters
    parameters.push_back({400, ParamType::BOOL, "CPU Monitor", "", 0, 1, {}, false, static_cast<int>(UIPage::CONFIG)});

    // ENV page parameters - control the currently selected envelope (300-323)
    // Envelope 1: 300-305
    parameters.push_back({300, ParamType::FLOAT, "Attack", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({301, ParamType::FLOAT, "Decay", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({302, ParamType::FLOAT, "Sustain", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({303, ParamType::FLOAT, "Release", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({304, ParamType::FLOAT, "Atk Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({305, ParamType::FLOAT, "Rel Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV)});
    // Envelope 2: 306-311
    parameters.push_back({306, ParamType::FLOAT, "Attack", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({307, ParamType::FLOAT, "Decay", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({308, ParamType::FLOAT, "Sustain", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({309, ParamType::FLOAT, "Release", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({310, ParamType::FLOAT, "Atk Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({311, ParamType::FLOAT, "Rel Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV)});
    // Envelope 3: 312-317
    parameters.push_back({312, ParamType::FLOAT, "Attack", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({313, ParamType::FLOAT, "Decay", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({314, ParamType::FLOAT, "Sustain", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({315, ParamType::FLOAT, "Release", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({316, ParamType::FLOAT, "Atk Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({317, ParamType::FLOAT, "Rel Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV)});
    // Envelope 4: 318-323
    parameters.push_back({318, ParamType::FLOAT, "Attack", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({319, ParamType::FLOAT, "Decay", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({320, ParamType::FLOAT, "Sustain", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({321, ParamType::FLOAT, "Release", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({322, ParamType::FLOAT, "Atk Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV)});
    parameters.push_back({323, ParamType::FLOAT, "Rel Bend", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::ENV)});
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

float UI::getParameterValue(int id) {
    const int oscIndex = currentOscillatorIndex;
    const int lfoIndex = currentLFOIndex;
    const int envIndex = currentEnvelopeIndex;

    switch (id) {
        case 6: return params->masterVolume.load();
        case 10: return static_cast<float>(params->getOscMode(oscIndex));
        case 19: return static_cast<float>(params->getOscShape(oscIndex));
        case 11: return params->getOscFrequency(oscIndex);
        case 12: return params->getOscMorph(oscIndex);
        case 13: return params->getOscDuty(oscIndex);
        case 14: return params->getOscRatio(oscIndex);
        case 15: return params->getOscOffset(oscIndex);
        case 18: return params->getOscLevel(oscIndex);
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
        default: return 0.0f;
    }
}

void UI::setParameterValue(int id, float value) {
    const int oscIndex = currentOscillatorIndex;
    const int lfoIndex = currentLFOIndex;
    const int envIndex = currentEnvelopeIndex;

    switch (id) {
        case 6: params->masterVolume = value; break;
        case 10: params->setOscMode(oscIndex, static_cast<int>(value)); break;
        case 19: params->setOscShape(oscIndex, static_cast<int>(value)); break;
        case 11: params->setOscFrequency(oscIndex, value); break;
        case 12: params->setOscMorph(oscIndex, value); break;
        case 13: params->setOscDuty(oscIndex, value); break;
        case 14: params->setOscRatio(oscIndex, value); break;
        case 15: params->setOscOffset(oscIndex, value); break;
        case 18: params->setOscLevel(oscIndex, value); break;
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
        case 40: params->currentLoop = static_cast<int>(value); break;
        case 41: params->overdubMix = value; break;
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
    }
}

void UI::adjustParameter(int id, bool increase) {
    InlineParameter* param = getParameter(id);
    if (!param) return;

    float currentValue = getParameterValue(id);
    float newValue = currentValue;

    switch (param->type) {
        case ParamType::FLOAT: {
            float step = (param->max_val - param->min_val) * 0.01f; // 1% steps

            // Special exponential handling for certain parameters
            if (id >= 300 && id <= 323 && (id % 6 == 0 || id % 6 == 1 || id % 6 == 3)) { // ENV Attack, Decay, Release - exponential scaling
                if (increase) {
                    newValue = std::min(param->max_val, currentValue * 1.1f);
                } else {
                    newValue = std::max(param->min_val, currentValue / 1.1f);
                }
            } else if (id == 11) { // Oscillator frequency - musical scaling
                if (increase) {
                    newValue = std::min(param->max_val, currentValue * 1.059463f); // semitone ratio
                } else {
                    newValue = std::max(param->min_val, currentValue / 1.059463f);
                }
            } else if (id == 14) { // Oscillator ratio - logarithmic scaling
                if (increase) {
                    newValue = std::min(param->max_val, currentValue * 1.1f);
                } else {
                    newValue = std::max(param->min_val, currentValue / 1.1f);
                }
            } else if (id == 32) { // Filter cutoff - logarithmic scaling
                if (increase) {
                    newValue = std::min(param->max_val, currentValue * 1.1f);
                } else {
                    newValue = std::max(param->min_val, currentValue / 1.1f);
                }
            } else if (id == 200) { // LFO period - logarithmic scaling
                if (increase) {
                    newValue = std::min(param->max_val, currentValue * 1.1f);
                } else {
                    newValue = std::max(param->min_val, currentValue / 1.1f);
                }
            } else {
                // Linear scaling for other parameters
                if (increase) {
                    newValue = std::min(param->max_val, currentValue + step);
                } else {
                    newValue = std::max(param->min_val, currentValue - step);
                }
            }
            break;
        }
        case ParamType::INT: {
            int intValue = static_cast<int>(currentValue);
            if (increase) {
                intValue = std::min(static_cast<int>(param->max_val), intValue + 1);
            } else {
                intValue = std::max(static_cast<int>(param->min_val), intValue - 1);
            }
            newValue = static_cast<float>(intValue);
            break;
        }
        case ParamType::ENUM: {
            int enumValue = static_cast<int>(currentValue);
            if (increase) {
                enumValue = std::min(static_cast<int>(param->max_val), enumValue + 1);
            } else {
                enumValue = std::max(static_cast<int>(param->min_val), enumValue - 1);
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
    // Modulation destinations are:
    // 0-5:   OSC 1 (Pitch, Morph, Duty, Ratio, Offset, Level)
    // 6-11:  OSC 2 (Pitch, Morph, Duty, Ratio, Offset, Level)
    // 12-17: OSC 3 (Pitch, Morph, Duty, Ratio, Offset, Level)
    // 18-23: OSC 4 (Pitch, Morph, Duty, Ratio, Offset, Level)
    // 24: Filter Cutoff
    // 25: Filter Resonance (not implemented yet)
    // 26: Reverb Mix
    // 27: Reverb Size

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
        case 18: // Level
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
