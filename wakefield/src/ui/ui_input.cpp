#include "../ui.h"
#include "../looper.h"
#include "../loop_manager.h"
#include "../sequencer.h"
#include <algorithm>

// External references to global objects from main.cpp
extern LoopManager* loopManager;
extern Sequencer* sequencer;

void UI::handleInput(int ch) {
    // Handle help mode
    if (helpActive) {
        if (ch == 'h' || ch == 'H' || ch == 27 || ch == 'q' || ch == 'Q') {  // H, Esc, or Q to close
            hideHelp();
        } else if (ch == KEY_UP || ch == 'k' || ch == 'K') {
            helpScrollOffset = std::max(0, helpScrollOffset - 1);
        } else if (ch == KEY_DOWN || ch == 'j' || ch == 'J') {
            helpScrollOffset++;
        } else if (ch == KEY_PPAGE) {  // Page Up
            helpScrollOffset = std::max(0, helpScrollOffset - 10);
        } else if (ch == KEY_NPAGE) {  // Page Down
            helpScrollOffset += 10;
        }
        return;
    }

    // Handle preset text input mode
    if (textInputActive) {
        handleTextInput(ch);
        return;
    }

    if (sequencerScaleMenuActive) {
        handleSequencerScaleMenuInput(ch);
        return;
    }

    // Handle numeric input mode for parameters
    if (numericInputActive) {
        if (ch == '\n' || ch == KEY_ENTER) {
            finishNumericInput();
        } else if (ch == 27) {  // Escape key
            if (numericInputIsSequencer) {
                cancelSequencerNumericInput();
            }
            numericInputActive = false;
            numericInputBuffer.clear();
            clear();  // Clear screen to refresh properly
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            if (!numericInputBuffer.empty()) {
                numericInputBuffer.pop_back();
            }
        } else if (ch >= 32 && ch < 127) {  // Printable characters
            if (numericInputBuffer.length() < 20) {
                numericInputBuffer += static_cast<char>(ch);
            }
        }
        return;
    }

    // Handle MIDI learn mode - allow Escape to cancel
    if (params->midiLearnActive.load()) {
        if (ch == 27) {  // Escape key
            finishMidiLearn();
            addConsoleMessage("MIDI Learn cancelled");
            clear();  // Clear screen to refresh properly
        }
        return;  // Don't process other keys during MIDI learn
    }

    // Sequencer-specific navigation and editing
    if (currentPage == UIPage::SEQUENCER && sequencer) {
        if (handleSequencerInput(ch)) {
            return;
        }
    }

    // Get current page parameter IDs (for non-sequencer pages)
    std::vector<int> pageParams;
    if (currentPage != UIPage::SEQUENCER) {
        pageParams = getParameterIdsForPage(currentPage);
    }

    // Tab key cycles forward through pages
    if (ch == '\t') {
        if (currentPage == UIPage::MAIN) currentPage = UIPage::OSCILLATOR;
        else if (currentPage == UIPage::OSCILLATOR) currentPage = UIPage::LFO;
        else if (currentPage == UIPage::LFO) currentPage = UIPage::ENV;
        else if (currentPage == UIPage::ENV) currentPage = UIPage::FM;
        else if (currentPage == UIPage::FM) currentPage = UIPage::REVERB;
        else if (currentPage == UIPage::REVERB) currentPage = UIPage::FILTER;
        else if (currentPage == UIPage::FILTER) currentPage = UIPage::LOOPER;
        else if (currentPage == UIPage::LOOPER) currentPage = UIPage::SEQUENCER;
        else if (currentPage == UIPage::SEQUENCER) currentPage = UIPage::CONFIG;
        else currentPage = UIPage::MAIN;

        // Set selected parameter to first parameter on new page
        std::vector<int> newPageParams = getParameterIdsForPage(currentPage);
        if (!newPageParams.empty()) {
            selectedParameterId = newPageParams[0];
        }
        return;
    }

    // Ctrl+Tab (KEY_BTAB or Shift+Tab) cycles backward through pages
    if (ch == KEY_BTAB || ch == 353) {  // KEY_BTAB = Shift+Tab, 353 = some terminals
        if (currentPage == UIPage::MAIN) currentPage = UIPage::CONFIG;
        else if (currentPage == UIPage::OSCILLATOR) currentPage = UIPage::MAIN;
        else if (currentPage == UIPage::LFO) currentPage = UIPage::OSCILLATOR;
        else if (currentPage == UIPage::ENV) currentPage = UIPage::LFO;
        else if (currentPage == UIPage::FM) currentPage = UIPage::ENV;
        else if (currentPage == UIPage::REVERB) currentPage = UIPage::FM;
        else if (currentPage == UIPage::FILTER) currentPage = UIPage::REVERB;
        else if (currentPage == UIPage::LOOPER) currentPage = UIPage::FILTER;
        else if (currentPage == UIPage::SEQUENCER) currentPage = UIPage::LOOPER;
        else if (currentPage == UIPage::CONFIG) currentPage = UIPage::SEQUENCER;

        // Set selected parameter to first parameter on new page
        std::vector<int> newPageParams = getParameterIdsForPage(currentPage);
        if (!newPageParams.empty()) {
            selectedParameterId = newPageParams[0];
        }
        return;
    }

    // Up/Down arrows navigate between parameters on current page
    if (currentPage != UIPage::SEQUENCER && ch == KEY_UP) {
        if (!pageParams.empty()) {
            auto it = std::find(pageParams.begin(), pageParams.end(), selectedParameterId);
            if (it != pageParams.end() && it != pageParams.begin()) {
                selectedParameterId = *(--it);
            } else {
                selectedParameterId = pageParams.back(); // Wrap to end
            }
        }
        return;
    } else if (currentPage != UIPage::SEQUENCER && ch == KEY_DOWN) {
        if (!pageParams.empty()) {
            auto it = std::find(pageParams.begin(), pageParams.end(), selectedParameterId);
            if (it != pageParams.end() && (it + 1) != pageParams.end()) {
                selectedParameterId = *(++it);
            } else {
                selectedParameterId = pageParams.front(); // Wrap to beginning
            }
        }
        return;
    }

    // Left/Right arrows adjust parameter values
    if (currentPage != UIPage::SEQUENCER && ch == KEY_LEFT) {
        adjustParameter(selectedParameterId, false);
        return;
    } else if (currentPage != UIPage::SEQUENCER && ch == KEY_RIGHT) {
        adjustParameter(selectedParameterId, true);
        return;
    }

    // Enter key starts numeric input for current parameter
    if (currentPage != UIPage::SEQUENCER && (ch == '\n' || ch == KEY_ENTER)) {
        startNumericInput(selectedParameterId);
        return;
    }

    // 'H' key shows help (uppercase only to avoid conflicts)
    if (ch == 'H') {
        showHelp();
        return;
    }

    // 'L' key starts MIDI learn for current parameter
    if (ch == 'L' || ch == 'l') {
        startMidiLearn(selectedParameterId);
        return;
    }

    // Transport and looping hotkeys (keep these)
    if (ch == ' ') {
        // Spacebar behavior depends on current page
        if (currentPage == UIPage::LOOPER) {
            if (loopManager) {
                Looper* loop = loopManager->getCurrentLoop();
                if (loop) loop->pressRecPlay();
            }
        } else if (currentPage == UIPage::SEQUENCER) {
            if (sequencer) {
                if (sequencer->isPlaying()) {
                    sequencer->stop();
                } else {
                    sequencer->play();
                }
            }
        }
        return;
    }

    // Oscillator selection (1-4 keys on OSCILLATOR page)
    if (currentPage == UIPage::OSCILLATOR) {
        switch (ch) {
            case '1': currentOscillatorIndex = 0; addConsoleMessage("Oscillator 1 selected"); return;
            case '2': currentOscillatorIndex = 1; addConsoleMessage("Oscillator 2 selected"); return;
            case '3': currentOscillatorIndex = 2; addConsoleMessage("Oscillator 3 selected"); return;
            case '4': currentOscillatorIndex = 3; addConsoleMessage("Oscillator 4 selected"); return;
        }
    }

    // LFO selection (1-4 keys on LFO page)
    if (currentPage == UIPage::LFO) {
        switch (ch) {
            case '1': currentLFOIndex = 0; addConsoleMessage("LFO 1 selected"); return;
            case '2': currentLFOIndex = 1; addConsoleMessage("LFO 2 selected"); return;
            case '3': currentLFOIndex = 2; addConsoleMessage("LFO 3 selected"); return;
            case '4': currentLFOIndex = 3; addConsoleMessage("LFO 4 selected"); return;
        }
    }

    // Envelope selection (1-4 keys on ENV page)
    if (currentPage == UIPage::ENV) {
        switch (ch) {
            case '1': currentEnvelopeIndex = 0; addConsoleMessage("Envelope 1 selected"); return;
            case '2': currentEnvelopeIndex = 1; addConsoleMessage("Envelope 2 selected"); return;
            case '3': currentEnvelopeIndex = 2; addConsoleMessage("Envelope 3 selected"); return;
            case '4': currentEnvelopeIndex = 3; addConsoleMessage("Envelope 4 selected"); return;
        }
    }

    // FM Matrix navigation and editing
    if (currentPage == UIPage::FM) {
        switch (ch) {
            case KEY_UP:
                fmMatrixCursorRow = (fmMatrixCursorRow - 1 + 4) % 4;
                // Skip diagonal (self-modulation)
                if (fmMatrixCursorRow == fmMatrixCursorCol) {
                    fmMatrixCursorRow = (fmMatrixCursorRow - 1 + 4) % 4;
                }
                return;

            case KEY_DOWN:
                fmMatrixCursorRow = (fmMatrixCursorRow + 1) % 4;
                // Skip diagonal (self-modulation)
                if (fmMatrixCursorRow == fmMatrixCursorCol) {
                    fmMatrixCursorRow = (fmMatrixCursorRow + 1) % 4;
                }
                return;

            case KEY_LEFT:
                fmMatrixCursorCol = (fmMatrixCursorCol - 1 + 4) % 4;
                // Skip diagonal (self-modulation)
                if (fmMatrixCursorRow == fmMatrixCursorCol) {
                    fmMatrixCursorCol = (fmMatrixCursorCol - 1 + 4) % 4;
                }
                // Also decrease FM depth
                if (fmMatrixCursorRow != fmMatrixCursorCol) {
                    float depth = params->getFMDepth(fmMatrixCursorCol, fmMatrixCursorRow);
                    params->setFMDepth(fmMatrixCursorCol, fmMatrixCursorRow, std::max(0.0f, depth - 0.05f));
                }
                return;

            case KEY_RIGHT:
                fmMatrixCursorCol = (fmMatrixCursorCol + 1) % 4;
                // Skip diagonal (self-modulation)
                if (fmMatrixCursorRow == fmMatrixCursorCol) {
                    fmMatrixCursorCol = (fmMatrixCursorCol + 1) % 4;
                }
                // Also increase FM depth
                if (fmMatrixCursorRow != fmMatrixCursorCol) {
                    float depth = params->getFMDepth(fmMatrixCursorCol, fmMatrixCursorRow);
                    params->setFMDepth(fmMatrixCursorCol, fmMatrixCursorRow, std::min(1.0f, depth + 0.05f));
                }
                return;

            case '+':
            case '=':
                if (fmMatrixCursorRow != fmMatrixCursorCol) {
                    float depth = params->getFMDepth(fmMatrixCursorCol, fmMatrixCursorRow);
                    params->setFMDepth(fmMatrixCursorCol, fmMatrixCursorRow, std::min(1.0f, depth + 0.1f));
                }
                return;

            case '-':
            case '_':
                if (fmMatrixCursorRow != fmMatrixCursorCol) {
                    float depth = params->getFMDepth(fmMatrixCursorCol, fmMatrixCursorRow);
                    params->setFMDepth(fmMatrixCursorCol, fmMatrixCursorRow, std::max(0.0f, depth - 0.1f));
                }
                return;

            case ' ':  // Space bar: quick presets (0%, 50%, 100%)
                if (fmMatrixCursorRow != fmMatrixCursorCol) {
                    float depth = params->getFMDepth(fmMatrixCursorCol, fmMatrixCursorRow);
                    if (depth < 0.01f) {
                        params->setFMDepth(fmMatrixCursorCol, fmMatrixCursorRow, 0.5f);
                    } else if (depth < 0.75f) {
                        params->setFMDepth(fmMatrixCursorCol, fmMatrixCursorRow, 1.0f);
                    } else {
                        params->setFMDepth(fmMatrixCursorCol, fmMatrixCursorRow, 0.0f);
                    }
                }
                return;
        }
    }

    // Looper-specific hotkeys (only active on looper page)
    if (currentPage == UIPage::LOOPER) {
        switch (ch) {
            // Loop selection (1-4)
            case '1': params->currentLoop = 0; break;
            case '2': params->currentLoop = 1; break;
            case '3': params->currentLoop = 2; break;
            case '4': params->currentLoop = 3; break;

            // Overdub toggle (O/o)
            case 'O':
            case 'o':
                if (loopManager) {
                    Looper* loop = loopManager->getCurrentLoop();
                    if (loop) loop->pressOverdub();
                }
                break;

            // Stop (S/s)
            case 'S':
            case 's':
                if (loopManager) {
                    Looper* loop = loopManager->getCurrentLoop();
                    if (loop) loop->pressStop();
                }
                break;

            // Clear (C/c)
            case 'C':
            case 'c':
                if (loopManager) {
                    Looper* loop = loopManager->getCurrentLoop();
                    if (loop) loop->pressClear();
                }
                break;

            // Overdub mix ([/])
            case '[':
                params->overdubMix = std::max(0.0f, params->overdubMix.load() - 0.05f);
                break;
            case ']':
                params->overdubMix = std::min(1.0f, params->overdubMix.load() + 0.05f);
                break;
        }
    }

    // Sequencer-specific hotkeys (only active on sequencer page)
    if (currentPage == UIPage::SEQUENCER && sequencer) {
        switch (ch) {
            // Generate pattern (G/g)
            case 'G':
            case 'g':
                sequencer->generatePattern();
                addConsoleMessage("Generated new pattern");
                break;

            // Mutate pattern (M/m)
            case 'M':
            case 'm':
                sequencer->mutatePattern(0.2f);  // 20% mutation
                addConsoleMessage("Pattern mutated");
                break;

            // Reset sequencer (R/r)
            case 'R':
            case 'r':
                sequencer->reset();
                addConsoleMessage("Sequencer reset");
                break;

            // Tempo control (T/Y)
            case 'T':
            case 't':
                {
                    double newTempo = std::max(20.0, sequencer->getTempo() - 5.0);
                    sequencer->setTempo(newTempo);
                }
                break;
            case 'Y':
            case 'y':
                {
                    double newTempo = std::min(300.0, sequencer->getTempo() + 5.0);
                    sequencer->setTempo(newTempo);
                }
                break;

            // Euclidean hits control (H/J)
            case 'H':
            case 'h':
                {
                    EuclideanPattern& euc = sequencer->getEuclideanPattern();
                    int newHits = std::max(1, euc.getHits() - 1);
                    sequencer->setEuclideanPattern(newHits, euc.getSteps(), euc.getRotation());
                    sequencer->regenerateUnlocked();
                    addConsoleMessage("Euclidean hits: " + std::to_string(newHits));
                }
                break;
            case 'J':
            case 'j':
                {
                    EuclideanPattern& euc = sequencer->getEuclideanPattern();
                    int newHits = std::min(euc.getSteps(), euc.getHits() + 1);
                    sequencer->setEuclideanPattern(newHits, euc.getSteps(), euc.getRotation());
                    sequencer->regenerateUnlocked();
                    addConsoleMessage("Euclidean hits: " + std::to_string(newHits));
                }
                break;

            // Track selection (1-4)
            case '1':
                sequencer->setCurrentTrack(0);
                addConsoleMessage("Sequencer: Track 1 selected");
                break;
            case '2':
                sequencer->setCurrentTrack(1);
                addConsoleMessage("Sequencer: Track 2 selected");
                break;
            case '3':
                sequencer->setCurrentTrack(2);
                addConsoleMessage("Sequencer: Track 3 selected");
                break;
            case '4':
                sequencer->setCurrentTrack(3);
                addConsoleMessage("Sequencer: Track 4 selected");
                break;

            // Pattern rotation ([/])
            case '[':
                sequencer->rotatePattern(-1);
                addConsoleMessage("Sequencer: Rotated pattern left");
                break;
            case ']':
                sequencer->rotatePattern(1);
                addConsoleMessage("Sequencer: Rotated pattern right");
                break;
        }
    }
}
