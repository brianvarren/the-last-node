#pragma once

#include <string>
#include <map>
#include <ncurses.h>

// Semantic color identifiers (used throughout the UI code)
enum class ThemeColor {
    // Typography
    PageTitle,          // Main page titles (e.g., "FM MATRIX", "SAMPLER 1")
    SectionHeader,      // Section headings within a page
    Label,              // Parameter labels, field names
    Value,              // Parameter values, data display
    HintText,           // Help text, instructions at bottom

    // Selection and focus
    SelectionFg,        // Selected item foreground
    SelectionBg,        // Selected item background
    Cursor,             // Selection cursor ">" character

    // Navigation
    TabActiveFg,        // Active tab text color
    TabActiveBg,        // Active tab background
    TabInactiveFg,      // Inactive tab text
    TabInactiveBg,      // Inactive tab background

    // Parameter states
    ParamNormal,        // Normal parameter display
    ParamLocked,        // Locked parameter (immune to randomization)
    ParamModulated,     // Parameter with active modulation
    ParamMidiMapped,    // Parameter mapped to MIDI CC

    // Status indicators
    StatusActive,       // Active voice, playing note
    StatusInactive,     // Inactive voice, no sound
    StatusMuted,        // Muted channel
    StatusSolo,         // Solo channel
    StatusError,        // Error state
    StatusWarning,      // Warning state

    // UI Chrome
    Border,             // Box borders, separators
    GridLine,           // Grid lines in matrices/tables
    Background,         // Default background
    PopupBg,            // Popup dialog background
    PopupBorder,        // Popup dialog border

    // Waveform display
    WaveformActive,     // Active waveform region (in loop)
    WaveformInactive,   // Inactive waveform region (outside loop)

    // Meter/bars
    MeterLow,           // Meter/bar at low levels
    MeterMid,           // Meter/bar at medium levels
    MeterHigh,          // Meter/bar at high levels

    // Console messages
    ConsoleNormal,      // Normal console message
    ConsoleError,       // Error message
    ConsoleWarning,     // Warning message
    ConsoleSuccess      // Success message
};

class Theme {
public:
    Theme();
    ~Theme();

    // Load theme from INI file
    bool loadFromFile(const std::string& filename);

    // Get color pair ID for a semantic color (returns ncurses color pair number)
    int getColorPair(ThemeColor color) const;

    // Get theme metadata
    std::string getName() const { return themeName; }
    std::string getAuthor() const { return themeAuthor; }

    // Initialize ncurses color pairs based on current theme
    void initializeColors();

    // Get waveform gradient colors (for oscilloscope, returns array of 20 colors)
    const int* getWaveformGradient() const { return waveformGradient; }

private:
    std::string themeName;
    std::string themeAuthor;

    // Map of semantic colors to ncurses color pair IDs
    std::map<ThemeColor, int> colorPairs;

    // Waveform gradient (20 levels for grayscale shading)
    int waveformGradient[20];

    // Helper to parse color name/number from INI
    int parseColor(const std::string& colorStr);

    // Helper to assign a color pair
    void assignColorPair(ThemeColor themeColor, int pairId, int fg, int bg);
};

// Global theme instance (initialized in main)
extern Theme* globalTheme;
