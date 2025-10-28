#include "../ui.h"
#include "../preset.h"
#include <algorithm>

void UI::startPresetBrowser() {
    presetBrowserActive = true;
    presetBrowserSelectedIndex = 0;
    presetBrowserScrollOffset = 0;
    refreshPresetBrowserList();
}

void UI::refreshPresetBrowserList() {
    presetBrowserPresets = PresetManager::listPresets();

    // Sort presets alphabetically
    std::sort(presetBrowserPresets.begin(), presetBrowserPresets.end());

    // Reset selection if out of range
    if (presetBrowserSelectedIndex >= static_cast<int>(presetBrowserPresets.size())) {
        presetBrowserSelectedIndex = std::max(0, static_cast<int>(presetBrowserPresets.size()) - 1);
    }
}

void UI::handlePresetBrowserInput(int ch) {
    int totalItems = presetBrowserPresets.size();

    switch (ch) {
        case KEY_UP:
            if (presetBrowserSelectedIndex > 0) {
                presetBrowserSelectedIndex--;
                // Adjust scroll offset if needed
                if (presetBrowserSelectedIndex < presetBrowserScrollOffset) {
                    presetBrowserScrollOffset = presetBrowserSelectedIndex;
                }
            }
            break;

        case KEY_DOWN:
            if (presetBrowserSelectedIndex < totalItems - 1) {
                presetBrowserSelectedIndex++;
                // Adjust scroll offset if needed (show max 20 items)
                const int maxVisible = 20;
                if (presetBrowserSelectedIndex >= presetBrowserScrollOffset + maxVisible) {
                    presetBrowserScrollOffset = presetBrowserSelectedIndex - maxVisible + 1;
                }
            }
            break;

        case '\n':
        case KEY_ENTER:
            finishPresetBrowser(true);
            break;

        case 27: // Escape
        case 'q':
        case 'Q':
            finishPresetBrowser(false);
            break;
    }
}

void UI::finishPresetBrowser(bool applySelection) {
    if (applySelection && presetBrowserSelectedIndex >= 0 &&
        presetBrowserSelectedIndex < static_cast<int>(presetBrowserPresets.size())) {
        std::string selectedPreset = presetBrowserPresets[presetBrowserSelectedIndex];
        loadPreset(selectedPreset);
    }

    presetBrowserActive = false;
}
