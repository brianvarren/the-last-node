#include "ui_mod_data.h"

#include <algorithm>

namespace {

struct ModuleData {
    const char* name;
    std::vector<ModOption> options;
};

const std::vector<ModOption>& buildSources() {
    static const std::vector<ModOption> sources = {
        {"LFO 1", "LFO1"}, {"LFO 2", "LFO2"}, {"LFO 3", "LFO3"}, {"LFO 4", "LFO4"},
        {"ENV 1", "ENV1"}, {"ENV 2", "ENV2"}, {"ENV 3", "ENV3"}, {"ENV 4", "ENV4"},
        {"Velocity", "Vel"}, {"Aftertouch", "AT"}, {"Mod Wheel", "MW"}, {"Pitch Bend", "PB"},
        {"Clock", "Clk"}
    };
    return sources;
}

const std::vector<ModOption>& buildCurves() {
    static const std::vector<ModOption> curves = {
        {"Linear", "Lin"}, {"Exponential", "Exp"}, {"Logarithmic", "Log"}, {"S-Curve", "S"}
    };
    return curves;
}

const std::vector<ModOption>& buildTypes() {
    static const std::vector<ModOption> types = {
        {"Unidirectional", "-->"}, {"Bidirectional", "<->"}
    };
    return types;
}

const std::vector<ModuleData>& buildDestinationModules() {
    static const std::vector<ModuleData> modules = {
        {"Oscillator 1", {
            {"Pitch", "O1:Pitch"}, {"Morph", "O1:Morph"}, {"Duty", "O1:Duty"},
            {"Ratio", "O1:Ratio"}, {"Offset", "O1:Offset"}, {"Amp", "O1:Amp"}
        }},
        {"Oscillator 2", {
            {"Pitch", "O2:Pitch"}, {"Morph", "O2:Morph"}, {"Duty", "O2:Duty"},
            {"Ratio", "O2:Ratio"}, {"Offset", "O2:Offset"}, {"Amp", "O2:Amp"}
        }},
        {"Oscillator 3", {
            {"Pitch", "O3:Pitch"}, {"Morph", "O3:Morph"}, {"Duty", "O3:Duty"},
            {"Ratio", "O3:Ratio"}, {"Offset", "O3:Offset"}, {"Amp", "O3:Amp"}
        }},
        {"Oscillator 4", {
            {"Pitch", "O4:Pitch"}, {"Morph", "O4:Morph"}, {"Duty", "O4:Duty"},
            {"Ratio", "O4:Ratio"}, {"Offset", "O4:Offset"}, {"Amp", "O4:Amp"}
        }},
        {"Filter", {
            {"Cutoff", "Flt:Cut"}, {"Resonance", "Flt:Res"}
        }},
        {"Reverb", {
            {"Mix", "Rvb:Mix"}, {"Size", "Rvb:Size"}
        }},
        {"Sampler 1", {
            {"Pitch", "S1:Pitch"}, {"Loop Start", "S1:LpSt"}, {"Loop Length", "S1:LpLen"},
            {"Crossfade", "S1:XFd"}, {"Level", "S1:Level"}
        }},
        {"Sampler 2", {
            {"Pitch", "S2:Pitch"}, {"Loop Start", "S2:LpSt"}, {"Loop Length", "S2:LpLen"},
            {"Crossfade", "S2:XFd"}, {"Level", "S2:Level"}
        }},
        {"Sampler 3", {
            {"Pitch", "S3:Pitch"}, {"Loop Start", "S3:LpSt"}, {"Loop Length", "S3:LpLen"},
            {"Crossfade", "S3:XFd"}, {"Level", "S3:Level"}
        }},
        {"Sampler 4", {
            {"Pitch", "S4:Pitch"}, {"Loop Start", "S4:LpSt"}, {"Loop Length", "S4:LpLen"},
            {"Crossfade", "S4:XFd"}, {"Level", "S4:Level"}
        }},
        {"LFO 1", {
            {"Rate", "L1:Rate"}, {"Morph", "L1:Morph"}, {"Duty", "L1:Duty"}
        }},
        {"LFO 2", {
            {"Rate", "L2:Rate"}, {"Morph", "L2:Morph"}, {"Duty", "L2:Duty"}
        }},
        {"LFO 3", {
            {"Rate", "L3:Rate"}, {"Morph", "L3:Morph"}, {"Duty", "L3:Duty"}
        }},
        {"LFO 4", {
            {"Rate", "L4:Rate"}, {"Morph", "L4:Morph"}, {"Duty", "L4:Duty"}
        }},
        {"Mixer", {
            {"Master Volume", "Mix:Mst"}, {"OSC 1 Level", "Mix:O1"},
            {"OSC 2 Level", "Mix:O2"}, {"OSC 3 Level", "Mix:O3"},
            {"OSC 4 Level", "Mix:O4"}, {"SAMP 1 Level", "Mix:S1"},
            {"SAMP 2 Level", "Mix:S2"}, {"SAMP 3 Level", "Mix:S3"},
            {"SAMP 4 Level", "Mix:S4"}
        }},
        {"Clock Targets", {
            {"Seq Track 1 Phase", "Clk:T1"}, {"Seq Track 2 Phase", "Clk:T2"},
            {"Seq Track 3 Phase", "Clk:T3"}, {"Seq Track 4 Phase", "Clk:T4"},
            {"Sampler 1 Phase", "Clk:S1"}, {"Sampler 2 Phase", "Clk:S2"},
            {"Sampler 3 Phase", "Clk:S3"}, {"Sampler 4 Phase", "Clk:S4"}
        }}
    };
    return modules;
}

const std::vector<ModOption>& buildDestinationsFlat() {
    static const std::vector<ModOption> flat = [] {
        std::vector<ModOption> result;
        const auto& modules = buildDestinationModules();
        for (const auto& module : modules) {
            result.insert(result.end(), module.options.begin(), module.options.end());
        }
        return result;
    }();
    return flat;
}

} // namespace

const std::vector<ModOption>& getModSourceOptions() {
    return buildSources();
}

const std::vector<ModOption>& getModCurveOptions() {
    return buildCurves();
}

const std::vector<ModOption>& getModTypeOptions() {
    return buildTypes();
}

const std::vector<ModOption>& getModDestinationOptions() {
    return buildDestinationsFlat();
}

const std::vector<ModDestinationModule>& getModDestinationModules() {
    static const std::vector<ModDestinationModule> exposedModules = [] {
        const auto& internalModules = buildDestinationModules();
        std::vector<ModDestinationModule> modules;
        modules.reserve(internalModules.size());
        for (const auto& module : internalModules) {
            modules.push_back(ModDestinationModule{module.name, module.options});
        }
        return modules;
    }();
    return exposedModules;
}

int getDestinationIndexFromModule(int moduleIndex, int paramIndex) {
    const auto& modules = getModDestinationModules();
    if (moduleIndex < 0 || moduleIndex >= static_cast<int>(modules.size())) {
        return -1;
    }
    const auto& options = modules[moduleIndex].options;
    if (paramIndex < 0 || paramIndex >= static_cast<int>(options.size())) {
        return -1;
    }

    int index = 0;
    for (int i = 0; i < moduleIndex; ++i) {
        index += static_cast<int>(modules[i].options.size());
    }
    return index + paramIndex;
}

bool getModuleParamFromDestinationIndex(int destinationIndex, int& outModuleIndex, int& outParamIndex) {
    const auto& modules = getModDestinationModules();
    if (destinationIndex < 0) {
        return false;
    }

    int index = destinationIndex;
    for (int module = 0; module < static_cast<int>(modules.size()); ++module) {
        int count = static_cast<int>(modules[module].options.size());
        if (index < count) {
            outModuleIndex = module;
            outParamIndex = index;
            return true;
        }
        index -= count;
    }

    return false;
}
