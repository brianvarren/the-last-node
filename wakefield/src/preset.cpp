#include "preset.h"
#include "ui.h"
#include "oscillator.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <pwd.h>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <filesystem>

std::string PresetManager::getPresetDirectory() {
    const char* homeDir = getenv("HOME");
    if (!homeDir) {
        struct passwd* pw = getpwuid(getuid());
        homeDir = pw->pw_dir;
    }
    
    std::string configDir = std::string(homeDir) + "/.config/wakefield/presets";
    return configDir;
}

bool PresetManager::ensurePresetDirectoryExists() {
    std::string presetDir = getPresetDirectory();
    
    // Create ~/.config if it doesn't exist
    std::string configBase = std::string(getenv("HOME")) + "/.config";
    mkdir(configBase.c_str(), 0755);
    
    // Create ~/.config/wakefield if it doesn't exist
    std::string wakeFieldDir = configBase + "/wakefield";
    mkdir(wakeFieldDir.c_str(), 0755);
    
    // Create presets directory
    mkdir(presetDir.c_str(), 0755);
    
    return true;
}

std::vector<std::string> PresetManager::listPresets() {
    std::vector<std::string> presets;
    std::string presetDir = getPresetDirectory();
    
    DIR* dir = opendir(presetDir.c_str());
    if (!dir) {
        return presets;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        
        // Check if it ends with .txt
        if (filename.size() > 4 && 
            filename.substr(filename.size() - 4) == ".txt") {
            // Remove .txt extension
            std::string name = filename.substr(0, filename.size() - 4);
            presets.push_back(name);
        }
    }
    
    closedir(dir);
    
    // Sort alphabetically
    std::sort(presets.begin(), presets.end());
    
    return presets;
}

std::string PresetManager::getPresetPath(const std::string& name) {
    return getPresetDirectory() + "/" + name + ".txt";
}

std::string PresetManager::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool PresetManager::parseBool(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return (lower == "true" || lower == "1" || lower == "yes");
}

bool PresetManager::parsePresetFile(const std::string& filepath, SynthParameters* params) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    bool result = parsePresetStream(file, params);
    file.close();
    return result;
}

bool PresetManager::parsePresetStream(std::istream& stream, SynthParameters* params) {
    if (!stream.good()) {
        return false;
    }

    std::string line;
    std::string currentSection;

    while (std::getline(stream, line)) {
        line = trim(line);

        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.size() - 2);
            continue;
        }

        size_t equalPos = line.find('=');
        if (equalPos == std::string::npos) {
            continue;
        }

        std::string key = trim(line.substr(0, equalPos));
        std::string value = trim(line.substr(equalPos + 1));

        if (currentSection == "waveform") {
            if (key == "type") {
                if (value == "SINE") params->waveform = static_cast<int>(Waveform::SINE);
                else if (value == "SQUARE") params->waveform = static_cast<int>(Waveform::SQUARE);
                else if (value == "SAWTOOTH") params->waveform = static_cast<int>(Waveform::SAWTOOTH);
                else if (value == "TRIANGLE") params->waveform = static_cast<int>(Waveform::TRIANGLE);
            }
        } else if (currentSection == "envelope") {
            if (key == "attack") {
                float mapped = std::stof(value);
                params->attack = mapped;
                params->setEnvAttack(0, mapped);
            } else if (key == "decay") {
                float mapped = std::stof(value);
                params->decay = mapped;
                params->setEnvDecay(0, mapped);
            } else if (key == "sustain") {
                float mapped = std::stof(value);
                params->sustain = mapped;
                params->setEnvSustain(0, mapped);
            } else if (key == "release") {
                float mapped = std::stof(value);
                params->release = mapped;
                params->setEnvRelease(0, mapped);
            }
        } else if (currentSection == "master") {
            if (key == "volume") params->masterVolume = std::stof(value);
        } else if (currentSection == "filter") {
            if (key == "enabled") params->filterEnabled = parseBool(value);
            else if (key == "type") {
                if (value == "LOWPASS") params->filterType = 0;
                else if (value == "HIGHPASS") params->filterType = 1;
                else if (value == "HIGHSHELF") params->filterType = 2;
                else if (value == "LOWSHELF") params->filterType = 3;
                else if (value == "LADDER") params->filterType = 4;
                else if (value == "DIODE") params->filterType = 5;
                else if (value == "BANDPASS") params->filterType = 6;
                else if (value == "NOTCH") params->filterType = 7;
                else if (value == "BANDPASS2") params->filterType = 8;
            } else if (key == "cutoff") params->filterCutoff = std::stof(value);
            else if (key == "gain") params->filterGain = std::stof(value);
            else if (key == "spread") params->filterSpread = std::stof(value);
            else if (key == "notch_feedback") params->filterNotchFeedback = std::stof(value);
            else if (key == "width") params->filterBandWidth = std::stof(value);
        } else if (currentSection == "reverb") {
            if (key == "enabled") params->reverbEnabled = parseBool(value);
            else if (key == "type") {
                if (value == "GREYHOLE") params->reverbType = static_cast<int>(ReverbType::GREYHOLE);
                else if (value == "PLATE") params->reverbType = static_cast<int>(ReverbType::PLATE);
                else if (value == "ROOM") params->reverbType = static_cast<int>(ReverbType::ROOM);
                else if (value == "HALL") params->reverbType = static_cast<int>(ReverbType::HALL);
                else if (value == "SPRING") params->reverbType = static_cast<int>(ReverbType::SPRING);
            } else if (key == "size") params->reverbSize = std::stof(value);
            else if (key == "damping") params->reverbDamping = std::stof(value);
            else if (key == "mix") params->reverbMix = std::stof(value);
            else if (key == "decay") params->reverbDecay = std::stof(value);
            else if (key == "diffusion") params->reverbDiffusion = std::stof(value);
            else if (key == "modDepth") params->reverbModDepth = std::stof(value);
            else if (key == "modFreq") params->reverbModFreq = std::stof(value);
        }
    }

    return true;
}

bool PresetManager::writePresetFile(const std::string& filepath, SynthParameters* params) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    writePresetStream(file, params);
    file.close();
    return true;
}

void PresetManager::writePresetStream(std::ostream& file, SynthParameters* params) {
    // Waveform section
    file << "[waveform]\n";
    int waveform = params->waveform.load();
    if (waveform == static_cast<int>(Waveform::SINE)) file << "type=SINE\n";
    else if (waveform == static_cast<int>(Waveform::SQUARE)) file << "type=SQUARE\n";
    else if (waveform == static_cast<int>(Waveform::SAWTOOTH)) file << "type=SAWTOOTH\n";
    else if (waveform == static_cast<int>(Waveform::TRIANGLE)) file << "type=TRIANGLE\n";
    file << "\n";

    // Envelope section
    file << "[envelope]\n";
    file << "attack=" << params->getEnvAttack(0) << "\n";
    file << "decay=" << params->getEnvDecay(0) << "\n";
    file << "sustain=" << params->getEnvSustain(0) << "\n";
    file << "release=" << params->getEnvRelease(0) << "\n";
    file << "\n";

    // Master section
    file << "[master]\n";
    file << "volume=" << params->masterVolume.load() << "\n";
    file << "\n";

    // Filter section
    file << "[filter]\n";
    file << "enabled=" << (params->filterEnabled.load() ? "true" : "false") << "\n";
    int filterType = params->filterType.load();
    if (filterType == 0) file << "type=LOWPASS\n";
    else if (filterType == 1) file << "type=HIGHPASS\n";
    else if (filterType == 2) file << "type=HIGHSHELF\n";
    else if (filterType == 3) file << "type=LOWSHELF\n";
    else if (filterType == 4) file << "type=LADDER\n";
    else if (filterType == 5) file << "type=DIODE\n";
    else if (filterType == 6) file << "type=BANDPASS\n";
    else if (filterType == 7) file << "type=NOTCH\n";
    else if (filterType == 8) file << "type=BANDPASS2\n";
    file << "cutoff=" << params->filterCutoff.load() << "\n";
    file << "gain=" << params->filterGain.load() << "\n";
    file << "spread=" << params->filterSpread.load() << "\n";
    file << "notch_feedback=" << params->filterNotchFeedback.load() << "\n";
    file << "width=" << params->filterBandWidth.load() << "\n";
    file << "\n";

    // Reverb section
    file << "[reverb]\n";
    file << "enabled=" << (params->reverbEnabled.load() ? "true" : "false") << "\n";
    int reverbType = params->reverbType.load();
    if (reverbType == static_cast<int>(ReverbType::GREYHOLE)) file << "type=GREYHOLE\n";
    else if (reverbType == static_cast<int>(ReverbType::PLATE)) file << "type=PLATE\n";
    else if (reverbType == static_cast<int>(ReverbType::ROOM)) file << "type=ROOM\n";
    else if (reverbType == static_cast<int>(ReverbType::HALL)) file << "type=HALL\n";
    else if (reverbType == static_cast<int>(ReverbType::SPRING)) file << "type=SPRING\n";
    file << "size=" << params->reverbSize.load() << "\n";
    file << "damping=" << params->reverbDamping.load() << "\n";
    file << "mix=" << params->reverbMix.load() << "\n";
    file << "decay=" << params->reverbDecay.load() << "\n";
    file << "diffusion=" << params->reverbDiffusion.load() << "\n";
    file << "modDepth=" << params->reverbModDepth.load() << "\n";
    file << "modFreq=" << params->reverbModFreq.load() << "\n";
    file << "\n";

}

std::string PresetManager::serializeToString(SynthParameters* params) {
    std::ostringstream oss;
    writePresetStream(oss, params);
    return oss.str();
}

bool PresetManager::deserializeFromString(const std::string& data, SynthParameters* params) {
    std::istringstream iss(data);
    return parsePresetStream(iss, params);
}

bool PresetManager::savePreset(const std::string& name, SynthParameters* params) {
    ensurePresetDirectoryExists();
    std::string filepath = getPresetPath(name);
    return writePresetFile(filepath, params);
}

bool PresetManager::loadPreset(const std::string& name, SynthParameters* params) {
    std::string filepath = getPresetPath(name);
    return parsePresetFile(filepath, params);
}

static std::string readFileIfExists(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

bool PresetManager::exportPresetBundle(const std::string& name) {
    try {
        const std::string dir = getPresetDirectory();
        const std::string bundlePath = dir + "/" + name + ".wkbundle";
        const std::string presetTxt = getPresetPath(name);
        const std::string statePath = dir + "/" + name + ".state";
        const std::string seqPath = dir + "/" + name + ".seq.txt";
        const std::string sampPath = dir + "/" + name + ".samplers.txt";
        const std::string t1 = dir + "/" + name + "_track1.csv";
        const std::string t2 = dir + "/" + name + "_track2.csv";
        const std::string t3 = dir + "/" + name + "_track3.csv";
        const std::string t4 = dir + "/" + name + "_track4.csv";

        std::ofstream out(bundlePath);
        if (!out.is_open()) return false;
        auto writeSection = [&](const char* key, const std::string& content) {
            if (content.empty()) return;  // skip missing
            out << "--FILE " << key << "\n" << content << "\n--END\n";
        };
        out << "#BUNDLE v1\n";
        writeSection("preset", readFileIfExists(presetTxt));
        writeSection("state", readFileIfExists(statePath));
        writeSection("seq", readFileIfExists(seqPath));
        writeSection("samplers", readFileIfExists(sampPath));
        writeSection("track1", readFileIfExists(t1));
        writeSection("track2", readFileIfExists(t2));
        writeSection("track3", readFileIfExists(t3));
        writeSection("track4", readFileIfExists(t4));
        return true;
    } catch (...) {
        return false;
    }
}

bool PresetManager::importPresetBundle(const std::string& bundlePathIn, std::string* outName) {
    try {
        std::string bundlePath = bundlePathIn;
        if (!std::filesystem::exists(bundlePath)) {
            // Try relative to preset directory
            std::string candidate = getPresetDirectory() + "/" + bundlePathIn;
            if (std::filesystem::exists(candidate)) bundlePath = candidate;
        }
        std::ifstream in(bundlePath);
        if (!in.is_open()) return false;

        // Infer name from filename (strip dir and extension)
        std::string filename = bundlePath;
        size_t slash = filename.find_last_of("/\\");
        if (slash != std::string::npos) filename = filename.substr(slash + 1);
        size_t dot = filename.find_last_of('.');
        if (dot != std::string::npos) filename = filename.substr(0, dot);
        if (outName) *outName = filename;

        std::string line;
        std::string currentKey;
        std::stringstream current;
        auto flush = [&](const std::string& key, const std::string& content) {
            if (key.empty() || content.empty()) return;
            const std::string dir = getPresetDirectory();
            if (key == "preset") {
                std::ofstream f(getPresetPath(filename)); if (f.is_open()) f << content;
            } else if (key == "state") {
                std::ofstream f(dir + "/" + filename + ".state"); if (f.is_open()) f << content;
            } else if (key == "seq") {
                std::ofstream f(dir + "/" + filename + ".seq.txt"); if (f.is_open()) f << content;
            } else if (key == "samplers") {
                std::ofstream f(dir + "/" + filename + ".samplers.txt"); if (f.is_open()) f << content;
            } else if (key == "track1") {
                std::ofstream f(dir + "/" + filename + "_track1.csv"); if (f.is_open()) f << content;
            } else if (key == "track2") {
                std::ofstream f(dir + "/" + filename + "_track2.csv"); if (f.is_open()) f << content;
            } else if (key == "track3") {
                std::ofstream f(dir + "/" + filename + "_track3.csv"); if (f.is_open()) f << content;
            } else if (key == "track4") {
                std::ofstream f(dir + "/" + filename + "_track4.csv"); if (f.is_open()) f << content;
            }
        };

        while (std::getline(in, line)) {
            if (line.rfind("--FILE ", 0) == 0) {
                // new section
                flush(currentKey, current.str());
                currentKey = line.substr(7);
                current.str("");
                current.clear();
            } else if (line == "--END") {
                flush(currentKey, current.str());
                currentKey.clear();
                current.str("");
                current.clear();
            } else {
                current << line << '\n';
            }
        }
        // Flush any trailing content
        flush(currentKey, current.str());
        return true;
    } catch (...) {
        return false;
    }
}
