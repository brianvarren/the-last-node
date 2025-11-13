#ifndef PRESET_H
#define PRESET_H

#include <string>
#include <vector>
#include <ctime>
#include <istream>
#include <ostream>
#include "ui.h"

// Forward declaration
struct SynthParameters;

struct AutosaveInfo {
    std::string label;     // Human-friendly label for UI
    std::string path;      // Absolute path to the autosave file
    std::time_t timestamp; // Last write time
};

class PresetManager {
public:
    // Get preset directory path
    static std::string getPresetDirectory();
    static std::string getAutosaveDirectory();
    
    // Ensure preset directory exists
    static bool ensurePresetDirectoryExists();
    static bool ensureAutosaveDirectoryExists();
    
    // List all available presets
    static std::vector<std::string> listPresets();
    static std::vector<AutosaveInfo> listAutosaves();
    
    // Save current parameters to a preset file
    static bool savePreset(const std::string& name, SynthParameters* params);
    
    // Load preset file into parameters
    static bool loadPreset(const std::string& name, SynthParameters* params);
    static bool loadPresetFromPath(const std::string& filepath, SynthParameters* params);

    // Serialize parameters to an in-memory string
    static std::string serializeToString(SynthParameters* params);

    // Load parameters from an in-memory string
    static bool deserializeFromString(const std::string& data, SynthParameters* params);
    
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

    static bool parsePresetStream(std::istream& stream, SynthParameters* params);
    static void writePresetStream(std::ostream& stream, SynthParameters* params);

public:
    // Bundle a preset and its sidecars into a single shareable file
    // Writes to getPresetDirectory()/name + ".wkbundle"
    static bool exportPresetBundle(const std::string& name);
    // Import a bundle file (absolute or in preset directory) and write its sidecars
    // If outName is provided, returns the inferred preset name
    static bool importPresetBundle(const std::string& bundlePath, std::string* outName = nullptr);
};

#endif // PRESET_H
