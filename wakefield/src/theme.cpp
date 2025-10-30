#include "theme.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

// Global theme instance
Theme* globalTheme = nullptr;

Theme::Theme() : themeName("Default"), themeAuthor("Unknown") {
    // Initialize waveform gradient to grayscale by default
    for (int i = 0; i < 20; ++i) {
        waveformGradient[i] = 7 + i;  // Color pairs 7-26
    }
}

Theme::~Theme() {
}

// Trim whitespace from string
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

// Parse color name or number
int Theme::parseColor(const std::string& colorStr) {
    std::string lower = colorStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Check for standard color names
    if (lower == "black") return COLOR_BLACK;
    if (lower == "red") return COLOR_RED;
    if (lower == "green") return COLOR_GREEN;
    if (lower == "yellow") return COLOR_YELLOW;
    if (lower == "blue") return COLOR_BLUE;
    if (lower == "magenta") return COLOR_MAGENTA;
    if (lower == "cyan") return COLOR_CYAN;
    if (lower == "white") return COLOR_WHITE;

    // Try to parse as number (256-color mode)
    try {
        int num = std::stoi(colorStr);
        if (num >= 0 && num < COLORS) {
            return num;
        }
    } catch (...) {
        // Fall through to default
    }

    return COLOR_WHITE;  // Default fallback
}

bool Theme::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::map<std::string, std::string> config;
    std::string section;
    std::string line;

    // Parse INI file
    while (std::getline(file, line)) {
        line = trim(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Check for section header
        if (line[0] == '[' && line.back() == ']') {
            section = line.substr(1, line.length() - 2);
            continue;
        }

        // Parse key=value
        size_t eqPos = line.find('=');
        if (eqPos != std::string::npos) {
            std::string key = trim(line.substr(0, eqPos));
            std::string value = trim(line.substr(eqPos + 1));

            // Remove inline comments
            size_t commentPos = value.find('#');
            if (commentPos != std::string::npos) {
                value = trim(value.substr(0, commentPos));
            }

            if (!section.empty()) {
                config[section + "." + key] = value;
            }
        }
    }

    file.close();

    // Extract theme metadata
    if (config.count("theme.name")) {
        themeName = config["theme.name"];
    }
    if (config.count("theme.author")) {
        themeAuthor = config["theme.author"];
    }

    // Store color definitions (we'll assign pairs in initializeColors)
    // For now, just store the raw color values
    // This is a simplified implementation - full version would parse all colors

    return true;
}

void Theme::assignColorPair(ThemeColor themeColor, int pairId, int fg, int bg) {
    init_pair(pairId, fg, bg);
    colorPairs[themeColor] = pairId;
}

void Theme::initializeColors() {
    if (!has_colors()) {
        return;
    }

    start_color();

    // Assign color pairs based on semantic meaning
    // Pair IDs 1-6 are reserved for legacy/core UI elements
    assignColorPair(ThemeColor::Cursor, 1, COLOR_CYAN, COLOR_BLACK);
    assignColorPair(ThemeColor::ParamModulated, 2, COLOR_GREEN, COLOR_BLACK);
    assignColorPair(ThemeColor::Header, 3, COLOR_YELLOW, COLOR_BLACK);
    assignColorPair(ThemeColor::ParamLocked, 4, COLOR_RED, COLOR_BLACK);
    assignColorPair(ThemeColor::TabActiveFg, 5, COLOR_WHITE, COLOR_BLUE);
    assignColorPair(ThemeColor::TabInactiveFg, 6, COLOR_BLACK, COLOR_CYAN);

    // Additional semantic colors
    assignColorPair(ThemeColor::TextNormal, 27, COLOR_WHITE, COLOR_BLACK);
    assignColorPair(ThemeColor::ParamMidiMapped, 28, COLOR_YELLOW, COLOR_BLACK);
    assignColorPair(ThemeColor::StatusActive, 29, COLOR_GREEN, COLOR_BLACK);
    assignColorPair(ThemeColor::StatusInactive, 30, COLOR_WHITE, COLOR_BLACK);
    assignColorPair(ThemeColor::StatusError, 31, COLOR_RED, COLOR_BLACK);
    assignColorPair(ThemeColor::StatusWarning, 32, COLOR_YELLOW, COLOR_BLACK);
    assignColorPair(ThemeColor::MidiLearnPopupFg, 33, COLOR_YELLOW, COLOR_BLACK);
    assignColorPair(ThemeColor::PopupBorder, 34, COLOR_YELLOW, COLOR_BLACK);
    assignColorPair(ThemeColor::ConsoleNormal, 35, COLOR_WHITE, COLOR_BLACK);
    assignColorPair(ThemeColor::ConsoleError, 36, COLOR_RED, COLOR_BLACK);
    assignColorPair(ThemeColor::ConsoleWarning, 37, COLOR_YELLOW, COLOR_BLACK);
    assignColorPair(ThemeColor::ConsoleSuccess, 38, COLOR_GREEN, COLOR_BLACK);
    assignColorPair(ThemeColor::Border, 39, COLOR_CYAN, COLOR_BLACK);
    assignColorPair(ThemeColor::HintText, 40, COLOR_YELLOW, COLOR_BLACK);

    // Initialize waveform gradient (pairs 7-26 for 20 levels)
    if (COLORS >= 256) {
        for (int i = 0; i < 20; ++i) {
            int grayColor = 232 + i * 23 / 19;
            init_pair(7 + i, grayColor, COLOR_BLACK);
            waveformGradient[i] = 7 + i;
        }
    } else {
        // Fallback for 8-color mode
        init_pair(7, COLOR_BLACK, COLOR_BLACK);
        init_pair(8, COLOR_BLACK, COLOR_BLACK);
        init_pair(9, COLOR_WHITE, COLOR_BLACK);
        init_pair(10, COLOR_WHITE, COLOR_BLACK);
        init_pair(11, COLOR_WHITE, COLOR_BLACK);
        init_pair(12, COLOR_CYAN, COLOR_BLACK);

        for (int i = 0; i < 6; ++i) {
            waveformGradient[i] = 7 + i;
        }
        for (int i = 6; i < 20; ++i) {
            waveformGradient[i] = 12;  // Use brightest for remaining
        }
    }
}

int Theme::getColorPair(ThemeColor color) const {
    auto it = colorPairs.find(color);
    if (it != colorPairs.end()) {
        return it->second;
    }
    return 1;  // Default to cyan
}
