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
    
    std::string line;
    std::string currentSection;
    
    while (std::getline(file, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Check for section header
        if (line[0] == '[' && line[line.size()-1] == ']') {
            currentSection = line.substr(1, line.size() - 2);
            continue;
        }
        
        // Parse key=value pairs
        size_t equalPos = line.find('=');
        if (equalPos == std::string::npos) {
            continue;
        }
        
        std::string key = trim(line.substr(0, equalPos));
        std::string value = trim(line.substr(equalPos + 1));
        
        // Apply values based on section
        if (currentSection == "waveform") {
            if (key == "type") {
                if (value == "SINE") params->waveform = static_cast<int>(Waveform::SINE);
                else if (value == "SQUARE") params->waveform = static_cast<int>(Waveform::SQUARE);
                else if (value == "SAWTOOTH") params->waveform = static_cast<int>(Waveform::SAWTOOTH);
                else if (value == "TRIANGLE") params->waveform = static_cast<int>(Waveform::TRIANGLE);
            }
        } else if (currentSection == "envelope") {
            if (key == "attack") params->attack = std::stof(value);
            else if (key == "decay") params->decay = std::stof(value);
            else if (key == "sustain") params->sustain = std::stof(value);
            else if (key == "release") params->release = std::stof(value);
        } else if (currentSection == "master") {
            if (key == "volume") params->masterVolume = std::stof(value);
        } else if (currentSection == "filter") {
            if (key == "enabled") params->filterEnabled = parseBool(value);
            else if (key == "type") {
                if (value == "LOWPASS") params->filterType = 0;
                else if (value == "HIGHPASS") params->filterType = 1;
                else if (value == "HIGHSHELF") params->filterType = 2;
                else if (value == "LOWSHELF") params->filterType = 3;
            }
            else if (key == "cutoff") params->filterCutoff = std::stof(value);
            else if (key == "gain") params->filterGain = std::stof(value);
        } else if (currentSection == "reverb") {
            if (key == "enabled") params->reverbEnabled = parseBool(value);
            else if (key == "type") {
                if (value == "GREYHOLE") params->reverbType = static_cast<int>(ReverbType::GREYHOLE);
                else if (value == "PLATE") params->reverbType = static_cast<int>(ReverbType::PLATE);
                else if (value == "ROOM") params->reverbType = static_cast<int>(ReverbType::ROOM);
                else if (value == "HALL") params->reverbType = static_cast<int>(ReverbType::HALL);
                else if (value == "SPRING") params->reverbType = static_cast<int>(ReverbType::SPRING);
            }
            else if (key == "size") params->reverbSize = std::stof(value);
            else if (key == "damping") params->reverbDamping = std::stof(value);
            else if (key == "mix") params->reverbMix = std::stof(value);
            else if (key == "decay") params->reverbDecay = std::stof(value);
            else if (key == "diffusion") params->reverbDiffusion = std::stof(value);
            else if (key == "modDepth") params->reverbModDepth = std::stof(value);
            else if (key == "modFreq") params->reverbModFreq = std::stof(value);
        } else if (currentSection == "looper") {
            if (key == "current_loop") params->currentLoop = std::stoi(value);
            else if (key == "overdub_mix") params->overdubMix = std::stof(value);
            else if (key == "rec_play_cc") params->loopRecPlayCC = std::stoi(value);
            else if (key == "overdub_cc") params->loopOverdubCC = std::stoi(value);
            else if (key == "stop_cc") params->loopStopCC = std::stoi(value);
            else if (key == "clear_cc") params->loopClearCC = std::stoi(value);
        }
    }
    
    file.close();
    return true;
}

bool PresetManager::writePresetFile(const std::string& filepath, SynthParameters* params) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }
    
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
    file << "attack=" << params->attack.load() << "\n";
    file << "decay=" << params->decay.load() << "\n";
    file << "sustain=" << params->sustain.load() << "\n";
    file << "release=" << params->release.load() << "\n";
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
    file << "cutoff=" << params->filterCutoff.load() << "\n";
    file << "gain=" << params->filterGain.load() << "\n";
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
    
    // Looper section
    file << "[looper]\n";
    file << "current_loop=" << params->currentLoop.load() << "\n";
    file << "overdub_mix=" << params->overdubMix.load() << "\n";
    file << "rec_play_cc=" << params->loopRecPlayCC.load() << "\n";
    file << "overdub_cc=" << params->loopOverdubCC.load() << "\n";
    file << "stop_cc=" << params->loopStopCC.load() << "\n";
    file << "clear_cc=" << params->loopClearCC.load() << "\n";
    
    file.close();
    return true;
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

