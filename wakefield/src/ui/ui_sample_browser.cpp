#include "../ui.h"
#include "../synth.h"
#include "../sample_bank.h"
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <limits.h>

void UI::startSampleBrowser() {
    sampleBrowserActive = true;
    sampleBrowserSelectedIndex = 0;
    sampleBrowserScrollOffset = 0;

    // Convert current directory to absolute path for proper navigation
    char resolved[PATH_MAX];
    if (realpath(sampleBrowserCurrentDir.c_str(), resolved) != nullptr) {
        sampleBrowserCurrentDir = resolved;
    }

    refreshSampleBrowserFiles();
}

void UI::refreshSampleBrowserFiles() {
    sampleBrowserFiles.clear();
    sampleBrowserDirs.clear();

    DIR* dir = opendir(sampleBrowserCurrentDir.c_str());
    if (!dir) {
        // If directory doesn't exist, try to create samples directory
        if (sampleBrowserCurrentDir == "samples") {
            mkdir("samples", 0755);
            dir = opendir(sampleBrowserCurrentDir.c_str());
        }
        if (!dir) return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;

        // Skip hidden files and current directory
        if (name[0] == '.' && name != "..") continue;

        std::string fullPath = sampleBrowserCurrentDir + "/" + name;
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                sampleBrowserDirs.push_back(name);
            } else if (S_ISREG(st.st_mode)) {
                // Check if it's a .wav file
                if (name.length() > 4) {
                    std::string ext = name.substr(name.length() - 4);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".wav") {
                        sampleBrowserFiles.push_back(name);
                    }
                }
            }
        }
    }
    closedir(dir);

    // Sort directories and files
    std::sort(sampleBrowserDirs.begin(), sampleBrowserDirs.end());
    std::sort(sampleBrowserFiles.begin(), sampleBrowserFiles.end());

    // Reset selection if out of range
    int totalItems = sampleBrowserDirs.size() + sampleBrowserFiles.size();
    if (sampleBrowserSelectedIndex >= totalItems) {
        sampleBrowserSelectedIndex = std::max(0, totalItems - 1);
    }
}

void UI::handleSampleBrowserInput(int ch) {
    int totalItems = sampleBrowserDirs.size() + sampleBrowserFiles.size();

    switch (ch) {
        case KEY_UP:
            if (sampleBrowserSelectedIndex > 0) {
                sampleBrowserSelectedIndex--;
                // Adjust scroll offset if needed
                if (sampleBrowserSelectedIndex < sampleBrowserScrollOffset) {
                    sampleBrowserScrollOffset = sampleBrowserSelectedIndex;
                }
            }
            break;

        case KEY_DOWN:
            if (sampleBrowserSelectedIndex < totalItems - 1) {
                sampleBrowserSelectedIndex++;
                // Adjust scroll offset if needed (show max 20 items)
                const int maxVisible = 20;
                if (sampleBrowserSelectedIndex >= sampleBrowserScrollOffset + maxVisible) {
                    sampleBrowserScrollOffset = sampleBrowserSelectedIndex - maxVisible + 1;
                }
            }
            break;

        case '\n':
        case KEY_ENTER:
            finishSampleBrowser(true);
            break;

        case 27: // Escape
        case 'q':
        case 'Q':
            finishSampleBrowser(false);
            break;
    }
}

void UI::finishSampleBrowser(bool applySelection) {
    if (applySelection) {
        int totalDirs = sampleBrowserDirs.size();

        // Check if selection is a directory
        if (sampleBrowserSelectedIndex < totalDirs) {
            std::string selectedDir = sampleBrowserDirs[sampleBrowserSelectedIndex];

            // Handle parent directory
            if (selectedDir == "..") {
                // Go up one directory
                size_t lastSlash = sampleBrowserCurrentDir.find_last_of('/');
                if (lastSlash != std::string::npos) {
                    // If we're at root ("/"), don't go higher
                    if (lastSlash == 0) {
                        sampleBrowserCurrentDir = "/";
                    } else {
                        sampleBrowserCurrentDir = sampleBrowserCurrentDir.substr(0, lastSlash);
                    }
                }
                // If no slash found, we're in relative path - shouldn't happen with absolute paths
            } else {
                // Enter subdirectory
                if (sampleBrowserCurrentDir == "/") {
                    sampleBrowserCurrentDir = "/" + selectedDir;
                } else {
                    sampleBrowserCurrentDir += "/" + selectedDir;
                }
            }

            // Refresh files in new directory
            sampleBrowserSelectedIndex = 0;
            sampleBrowserScrollOffset = 0;
            refreshSampleBrowserFiles();
            return; // Don't close browser
        }

        // Selection is a file
        int fileIndex = sampleBrowserSelectedIndex - totalDirs;
        if (fileIndex >= 0 && fileIndex < static_cast<int>(sampleBrowserFiles.size())) {
            std::string selectedFile = sampleBrowserFiles[fileIndex];
            std::string fullPath = sampleBrowserCurrentDir + "/" + selectedFile;
            loadSampleForCurrentSampler(fullPath);
        }
    }

    sampleBrowserActive = false;
}

void UI::loadSampleForCurrentSampler(const std::string& filepath) {
    if (!synth) {
        addConsoleMessage("Error: Synth not initialized");
        return;
    }

    // Load the WAV file into SampleBank
    SampleBank* sampleBank = synth->getSampleBank();
    if (!sampleBank) {
        addConsoleMessage("Error: Sample bank not available");
        return;
    }

    int sampleIndex = sampleBank->loadSingleFile(filepath.c_str());
    if (sampleIndex < 0) {
        addConsoleMessage("Failed to load: " + filepath);
        return;
    }

    // Set the sample on the current sampler (all voices)
    synth->setSamplerSample(currentSamplerIndex, sampleIndex);

    // Extract filename for console message
    size_t lastSlash = filepath.find_last_of("/\\");
    std::string filename = (lastSlash != std::string::npos) ?
                          filepath.substr(lastSlash + 1) : filepath;

    addConsoleMessage("Loaded: " + filename);
}
