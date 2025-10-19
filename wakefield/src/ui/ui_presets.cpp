#include "../ui.h"
#include "../preset.h"

void UI::refreshPresetList() {
    availablePresets = PresetManager::listPresets();
}

void UI::loadPreset(const std::string& filename) {
    if (PresetManager::loadPreset(filename, params)) {
        currentPresetName = filename;
    }
}

void UI::savePreset(const std::string& filename) {
    if (PresetManager::savePreset(filename, params)) {
        currentPresetName = filename;
        refreshPresetList();
    }
}

void UI::startTextInput() {
    textInputActive = true;
    textInputBuffer.clear();
}

void UI::handleTextInput(int ch) {
    if (ch == '\n' || ch == KEY_ENTER) {
        // Save preset with entered name
        if (!textInputBuffer.empty()) {
            savePreset(textInputBuffer);
        }
        finishTextInput();
    } else if (ch == 27) {  // Escape
        finishTextInput();
        clear();  // Clear screen to refresh properly
    } else if (ch == KEY_BACKSPACE || ch == 127) {
        if (!textInputBuffer.empty()) {
            textInputBuffer.pop_back();
        }
    } else if (ch >= 32 && ch < 127) {  // Printable characters
        if (textInputBuffer.length() < 30) {
            textInputBuffer += static_cast<char>(ch);
        }
    }
}

void UI::finishTextInput() {
    textInputActive = false;
    textInputBuffer.clear();
}
