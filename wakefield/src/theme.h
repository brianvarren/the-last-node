#pragma once

#include <string>
#include <map>
#include <ncurses.h>

// Semantic color identifiers (used throughout the UI code)
enum class ThemeColor {
    // Text and backgrounds
    TextNormal,
    Background,

    // Selection and focus
    SelectionFg,
    SelectionBg,
    Cursor,

    // Parameter states
    ParamLocked,
    ParamModulated,
    ParamMidiMapped,

    // UI chrome
    TabActiveFg,
    TabActiveBg,
    TabInactiveFg,
    TabInactiveBg,
    Border,
    Header,
    HintText,

    // Status indicators
    StatusActive,
    StatusInactive,
    StatusError,
    StatusWarning,

    // Special elements
    MidiLearnPopupFg,
    MidiLearnPopupBg,
    PopupBorder,

    // Console messages
    ConsoleNormal,
    ConsoleError,
    ConsoleWarning,
    ConsoleSuccess
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
