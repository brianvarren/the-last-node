#include "../ui.h"
#include "../preset.h"
#include <algorithm>
#include <ctime>
#include <sstream>
#include <filesystem>

namespace {
std::string formatAutosaveLabel(const AutosaveInfo& info) {
    if (info.timestamp <= 0) {
        return info.label;
    }
    std::tm tm{};
    char buffer[64];
#if defined(_WIN32)
    localtime_s(&tm, &info.timestamp);
#else
    localtime_r(&info.timestamp, &tm);
#endif
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm) == 0) {
        return info.label;
    }
    std::ostringstream display;
    display << buffer << " | " << info.label;
    return display.str();
}
}

void UI::startPresetBrowser(PresetBrowserMode mode) {
    presetBrowserMode = mode;
    presetBrowserActive = true;
    presetBrowserSelectedIndex = 0;
    presetBrowserScrollOffset = 0;
    refreshPresetBrowserList();
}

void UI::refreshPresetBrowserList() {
    presetBrowserPresets.clear();
    presetBrowserPaths.clear();

    if (presetBrowserMode == PresetBrowserMode::Autosaves) {
        auto autosaves = PresetManager::listAutosaves();
        for (const auto& entry : autosaves) {
            presetBrowserPresets.push_back(formatAutosaveLabel(entry));
            presetBrowserPaths.push_back(entry.path);
        }
    } else {
        auto presets = PresetManager::listPresets();
        std::sort(presets.begin(), presets.end());
        for (const auto& name : presets) {
            presetBrowserPresets.push_back(name);
            presetBrowserPaths.push_back(PresetManager::getPresetPath(name));
        }
    }

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

        case 'd':
        case 'D':
            if (presetBrowserSelectedIndex >= 0 &&
                presetBrowserSelectedIndex < static_cast<int>(presetBrowserPresets.size())) {
                const std::string& path = presetBrowserPaths[presetBrowserSelectedIndex];
                const std::string& label = presetBrowserPresets[presetBrowserSelectedIndex];

                auto removeIfExists = [](const std::filesystem::path& p) {
                    std::error_code ec;
                    std::filesystem::remove(p, ec);
                };

                bool deleted = false;
                if (presetBrowserMode == PresetBrowserMode::Autosaves) {
                    std::error_code ec;
                    deleted = std::filesystem::remove(path, ec);
                } else {
                    std::filesystem::path mainPath(path);
                    std::filesystem::path base = mainPath;
                    base.replace_extension("");
                    std::error_code ec;
                    deleted = std::filesystem::remove(mainPath, ec);
                    removeIfExists(base.string() + ".state");
                    removeIfExists(base.string() + ".seq.txt");
                    removeIfExists(base.string() + ".samplers.txt");
                    removeIfExists(base.string() + ".wkbundle");
                    for (int t = 1; t <= 4; ++t) {
                        removeIfExists(base.string() + "_track" + std::to_string(t) + ".csv");
                    }
                }

                if (deleted) {
                    addConsoleMessage(std::string("Deleted ") +
                                      (presetBrowserMode == PresetBrowserMode::Autosaves ? "autosave: " : "preset: ") +
                                      label);
                    refreshPresetBrowserList();
                } else {
                    addConsoleMessage("Unable to delete " + label);
                }
            }
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
        const std::string& path = presetBrowserPaths[presetBrowserSelectedIndex];
        const std::string& label = presetBrowserPresets[presetBrowserSelectedIndex];
        loadPresetFromPath(path, label);
    }

    presetBrowserActive = false;
}
