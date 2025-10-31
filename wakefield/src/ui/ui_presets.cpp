#include "../ui.h"
#include "../preset.h"
#include "../sequencer.h"

extern Sequencer* sequencer;

void UI::refreshPresetList() {
    availablePresets = PresetManager::listPresets();
}

void UI::loadPreset(const std::string& filename) {
    if (PresetManager::loadPreset(filename, params)) {
        currentPresetName = filename;
        resetUndoHistory();
        // Also load sequencer patterns if available
        if (sequencer) {
            std::string baseDir = PresetManager::getPresetDirectory();
            for (int t = 0; t < 4; ++t) {
                std::string trackPath = baseDir + "/" + filename + "_track" + std::to_string(t+1) + ".csv";
                // Attempt load (if file missing, leave pattern as-is)
                try {
                    sequencer->getTrack(t).getPattern().loadFromFile(trackPath);
                } catch (...) {
                    // Ignore errors for missing/invalid files
                }
            }
        }
    }
}

void UI::savePreset(const std::string& filename) {
    if (PresetManager::savePreset(filename, params)) {
        // Save sequencer patterns alongside preset
        if (sequencer) {
            std::string baseDir = PresetManager::getPresetDirectory();
            for (int t = 0; t < 4; ++t) {
                std::string trackPath = baseDir + "/" + filename + "_track" + std::to_string(t+1) + ".csv";
                sequencer->getTrack(t).getPattern().saveToFile(trackPath);
            }
        }
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
