#ifndef PRESET_H
#define PRESET_H

#include <string>
#include <vector>
#include "ui.h"

// Forward declaration
struct SynthParameters;

class PresetManager {
public:
    // Get preset directory path
    static std::string getPresetDirectory();
    
    // Ensure preset directory exists
    static bool ensurePresetDirectoryExists();
    
    // List all available presets
    static std::vector<std::string> listPresets();
    
    // Save current parameters to a preset file
    static bool savePreset(const std::string& name, SynthParameters* params);
    
    // Load preset file into parameters
    static bool loadPreset(const std::string& name, SynthParameters* params);
    
    // Get full path for a preset name
    static std::string getPresetPath(const std::string& name);
    
private:
    // Parse INI-style config file
    static bool parsePresetFile(const std::string& filepath, SynthParameters* params);
    
    // Write INI-style config file
    static bool writePresetFile(const std::string& filepath, SynthParameters* params);
    
    // Helper to trim whitespace
    static std::string trim(const std::string& str);
    
    // Helper to parse boolean
    static bool parseBool(const std::string& str);
};

#endif // PRESET_H

