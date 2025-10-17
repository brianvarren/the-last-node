#include "../ui.h"
#include <algorithm>
#include <chrono>
#include <string>

void UI::initializeParameters() {
    parameters.clear();

    // MAIN page parameters - ALL support MIDI learn
    parameters.push_back({1, ParamType::ENUM, "Waveform", "", 0, 3, {"Sine", "Square", "Sawtooth", "Triangle"}, true, static_cast<int>(UIPage::MAIN)});
    parameters.push_back({2, ParamType::FLOAT, "Attack", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::MAIN)});
    parameters.push_back({3, ParamType::FLOAT, "Decay", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::MAIN)});
    parameters.push_back({4, ParamType::FLOAT, "Sustain", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MAIN)});
    parameters.push_back({5, ParamType::FLOAT, "Release", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::MAIN)});
    parameters.push_back({6, ParamType::FLOAT, "Master Volume", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MAIN)});

    // BRAINWAVE page parameters - ALL support MIDI learn
    parameters.push_back({10, ParamType::ENUM, "Mode", "", 0, 1, {"FREE", "KEY"}, true, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({11, ParamType::FLOAT, "Frequency", "Hz", 20.0f, 2000.0f, {}, true, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({12, ParamType::FLOAT, "Morph", "", 0.0001f, 0.9999f, {}, true, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({13, ParamType::FLOAT, "Duty", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({14, ParamType::INT, "Octave", "", -3, 3, {}, true, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({15, ParamType::BOOL, "LFO Enabled", "", 0, 1, {}, true, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({16, ParamType::INT, "LFO Speed", "", 0, 9, {}, true, static_cast<int>(UIPage::BRAINWAVE)});

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
    switch (id) {
        case 1: return static_cast<float>(params->waveform.load());
        case 2: return params->attack.load();
        case 3: return params->decay.load();
        case 4: return params->sustain.load();
        case 5: return params->release.load();
        case 6: return params->masterVolume.load();
        case 10: return static_cast<float>(params->brainwaveMode.load());
        case 11: return params->brainwaveFreq.load();
        case 12: return params->brainwaveMorph.load();
        case 13: return params->brainwaveDuty.load();
        case 14: return static_cast<float>(params->brainwaveOctave.load());
        case 15: return params->brainwaveLFOEnabled.load() ? 1.0f : 0.0f;
        case 16: return static_cast<float>(params->brainwaveLFOSpeed.load());
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
        default: return 0.0f;
    }
}

void UI::setParameterValue(int id, float value) {
    switch (id) {
        case 1: params->waveform = static_cast<int>(value); break;
        case 2: params->attack = value; break;
        case 3: params->decay = value; break;
        case 4: params->sustain = value; break;
        case 5: params->release = value; break;
        case 6: params->masterVolume = value; break;
        case 10: params->brainwaveMode = static_cast<int>(value); break;
        case 11: params->brainwaveFreq = value; break;
        case 12: params->brainwaveMorph = value; break;
        case 13: params->brainwaveDuty = value; break;
        case 14: params->brainwaveOctave = static_cast<int>(value); break;
        case 15: params->brainwaveLFOEnabled = (value > 0.5f); break;
        case 16: params->brainwaveLFOSpeed = static_cast<int>(value); break;
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
            if (id == 2 || id == 3 || id == 5) { // Attack, Decay, Release - exponential scaling
                if (increase) {
                    newValue = std::min(param->max_val, currentValue * 1.1f);
                } else {
                    newValue = std::max(param->min_val, currentValue / 1.1f);
                }
            } else if (id == 11) { // Brainwave frequency - musical scaling
                if (increase) {
                    newValue = std::min(param->max_val, currentValue * 1.059463f); // semitone ratio
                } else {
                    newValue = std::max(param->min_val, currentValue / 1.059463f);
                }
            } else if (id == 32) { // Filter cutoff - logarithmic scaling
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
