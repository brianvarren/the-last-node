#include "../ui.h"
#include "../looper.h"
#include "../loop_manager.h"
#include "../sequencer.h"
#include <algorithm>

// External references to global objects from main.cpp
extern LoopManager* loopManager;
extern Sequencer* sequencer;

void UI::handleInput(int ch) {
    // Check for Ctrl+K to toggle MIDI keyboard mode (KEY_CTRL + 'k' = 11 in most terminals)
    if (ch == 11) {  // Ctrl+K
        midiKeyboardMode = !midiKeyboardMode;
        if (midiKeyboardMode) {
            addConsoleMessage("MIDI Keyboard Mode: ON (Octave " + std::to_string(midiKeyboardOctave) + ")");
        } else {
            // Release all active notes when exiting keyboard mode
            for (int note : activeKeyboardNotes) {
                synth->noteOff(note);
            }
            activeKeyboardNotes.clear();
            addConsoleMessage("MIDI Keyboard Mode: OFF");
        }
        return;
    }

    // Handle MIDI keyboard mode input (musical typing)
    if (midiKeyboardMode) {
        // Map of keys to semitone offsets from base note
        // Piano-style layout: ZSXDCVGBHNJM (lower row white keys)
        //                     Q2W3ER5T6Y7U (upper row black keys)
        // Additional: I9O0P (extend range)
        int semitoneOffset = -1;

        switch (ch) {
            // Lower row (white keys) - C major scale starting from base note
            case 'z': case 'Z': semitoneOffset = 0; break;   // C
            case 's': case 'S': semitoneOffset = 1; break;   // C#
            case 'x': case 'X': semitoneOffset = 2; break;   // D
            case 'd': case 'D': semitoneOffset = 3; break;   // D#
            case 'c': case 'C': semitoneOffset = 4; break;   // E
            case 'v': case 'V': semitoneOffset = 5; break;   // F
            case 'g': case 'G': semitoneOffset = 6; break;   // F#
            case 'b': case 'B': semitoneOffset = 7; break;   // G
            case 'h': case 'H': semitoneOffset = 8; break;   // G#
            case 'n': case 'N': semitoneOffset = 9; break;   // A
            case 'j': case 'J': semitoneOffset = 10; break;  // A#
            case 'm': case 'M': semitoneOffset = 11; break;  // B
            case ',': case '<': semitoneOffset = 12; break;  // C (next octave)

            // Upper row (chromatic) - second octave
            case 'q': case 'Q': semitoneOffset = 12; break;  // C
            case '2': case '@': semitoneOffset = 13; break;  // C#
            case 'w': case 'W': semitoneOffset = 14; break;  // D
            case '3': case '#': semitoneOffset = 15; break;  // D#
            case 'e': case 'E': semitoneOffset = 16; break;  // E
            case 'r': case 'R': semitoneOffset = 17; break;  // F
            case '5': case '%': semitoneOffset = 18; break;  // F#
            case 't': case 'T': semitoneOffset = 19; break;  // G
            case '6': case '^': semitoneOffset = 20; break;  // G#
            case 'y': case 'Y': semitoneOffset = 21; break;  // A
            case '7': case '&': semitoneOffset = 22; break;  // A#
            case 'u': case 'U': semitoneOffset = 23; break;  // B
            case 'i': case 'I': semitoneOffset = 24; break;  // C (two octaves up)
            case '9': case '(': semitoneOffset = 25; break;  // C#
            case 'o': case 'O': semitoneOffset = 26; break;  // D
            case '0': case ')': semitoneOffset = 27; break;  // D#
            case 'p': case 'P': semitoneOffset = 28; break;  // E

            // Octave control
            case 'a': case 'A':  // Lower octave
                midiKeyboardOctave = std::max(0, midiKeyboardOctave - 1);
                addConsoleMessage("MIDI Keyboard: Octave " + std::to_string(midiKeyboardOctave));
                return;
            case 'f': case 'F':  // Raise octave (avoiding conflict with 'f' key note)
                // Changed to use different key
                break;
            case '\'': case '"':  // Raise octave (using apostrophe/quote key)
                midiKeyboardOctave = std::min(10, midiKeyboardOctave + 1);
                addConsoleMessage("MIDI Keyboard: Octave " + std::to_string(midiKeyboardOctave));
                return;

            // Allow escape to exit MIDI keyboard mode
            case 27:  // ESC
                midiKeyboardMode = false;
                for (int note : activeKeyboardNotes) {
                    synth->noteOff(note);
                }
                activeKeyboardNotes.clear();
                addConsoleMessage("MIDI Keyboard Mode: OFF");
                return;
        }

        if (semitoneOffset >= 0) {
            // Calculate MIDI note number (C4 = 60)
            int baseNote = (midiKeyboardOctave * 12) + 12;  // C of current octave
            int midiNote = baseNote + semitoneOffset;

            if (midiNote >= 0 && midiNote <= 127) {
                // Check if note is already active (avoid retriggering on key repeat)
                bool alreadyActive = false;
                for (int activeNote : activeKeyboardNotes) {
                    if (activeNote == midiNote) {
                        alreadyActive = true;
                        break;
                    }
                }

                if (!alreadyActive) {
                    synth->noteOn(midiNote, 100);  // Fixed velocity of 100
                    activeKeyboardNotes.push_back(midiNote);
                }
            }
        }

        // In MIDI keyboard mode, we consume all input except for critical controls
        return;
    }

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
            if (numericInputIsMod) {
                finishModMatrixAmountInput();
            } else {
                finishNumericInput();
            }
        } else if (ch == 27) {  // Escape key
            if (numericInputIsSequencer) {
                cancelSequencerNumericInput();
            }
            numericInputActive = false;
            numericInputIsMod = false;
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

    // Sample browser handling
    if (sampleBrowserActive) {
        handleSampleBrowserInput(ch);
        // Don't return if 'q'/'Q' - let it propagate to quit check
        if (ch != 'q' && ch != 'Q') return;
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

    auto setPage = [&](UIPage target) {
        currentPage = target;
        std::vector<int> newPageParams = getParameterIdsForPage(currentPage);
        if (!newPageParams.empty()) {
            selectedParameterId = newPageParams[0];
        }
    };

    // Tab key cycles forward through pages
    if (ch == '\t') {
        if (currentPage == UIPage::OSCILLATOR) setPage(UIPage::SAMPLER);
        else if (currentPage == UIPage::SAMPLER) setPage(UIPage::MIXER);
        else if (currentPage == UIPage::MIXER) setPage(UIPage::LFO);
        else if (currentPage == UIPage::LFO) setPage(UIPage::ENV);
        else if (currentPage == UIPage::ENV) setPage(UIPage::FM);
        else if (currentPage == UIPage::FM) setPage(UIPage::MOD);
        else if (currentPage == UIPage::MOD) setPage(UIPage::REVERB);
        else if (currentPage == UIPage::REVERB) setPage(UIPage::FILTER);
        else if (currentPage == UIPage::FILTER) setPage(UIPage::LOOPER);
        else if (currentPage == UIPage::LOOPER) setPage(UIPage::SEQUENCER);
        else if (currentPage == UIPage::SEQUENCER) setPage(UIPage::CONFIG);
        else setPage(UIPage::OSCILLATOR);
        return;
    }

    // Ctrl+Tab (KEY_BTAB or Shift+Tab) cycles backward through pages
    if (ch == KEY_BTAB || ch == 353) {  // KEY_BTAB = Shift+Tab, 353 = some terminals
        if (currentPage == UIPage::OSCILLATOR) setPage(UIPage::CONFIG);
        else if (currentPage == UIPage::SAMPLER) setPage(UIPage::OSCILLATOR);
        else if (currentPage == UIPage::MIXER) setPage(UIPage::SAMPLER);
        else if (currentPage == UIPage::LFO) setPage(UIPage::MIXER);
        else if (currentPage == UIPage::ENV) setPage(UIPage::LFO);
        else if (currentPage == UIPage::FM) setPage(UIPage::ENV);
        else if (currentPage == UIPage::MOD) setPage(UIPage::FM);
        else if (currentPage == UIPage::REVERB) setPage(UIPage::MOD);
        else if (currentPage == UIPage::FILTER) setPage(UIPage::REVERB);
        else if (currentPage == UIPage::LOOPER) setPage(UIPage::FILTER);
        else if (currentPage == UIPage::SEQUENCER) setPage(UIPage::LOOPER);
        else if (currentPage == UIPage::CONFIG) setPage(UIPage::SEQUENCER);
        return;
    }

    // Function keys jump directly to pages
    switch (ch) {
        case KEY_F(1):
            setPage(UIPage::LOOPER);
            addConsoleMessage("Sampler page pending – showing Looper page");
            return;
        case KEY_F(2):
            setPage(UIPage::OSCILLATOR);
            return;
        case KEY_F(3):
            setPage(UIPage::ENV);
            return;
        case KEY_F(4):
            setPage(UIPage::LFO);
            return;
        case KEY_F(5):
            setPage(UIPage::REVERB);
            return;
        case KEY_F(6):
            setPage(UIPage::FILTER);
            return;
        case KEY_F(7):
            setPage(UIPage::MIXER);
            return;
        case KEY_F(8):
            setPage(UIPage::MOD);
            return;
        case KEY_F(9):
            setPage(UIPage::SEQUENCER);
            addConsoleMessage("Chaos page pending – showing Sequencer");
            return;
        case KEY_F(10):
            setPage(UIPage::FM);
            return;
        case KEY_F(11):
            setPage(UIPage::MOD);
            return;
        case KEY_F(12):
            setPage(UIPage::CONFIG);
            return;
    }

    if (currentPage == UIPage::MOD) {
        // Handle MOD matrix menu if active
        if (modMatrixMenuActive) {
            handleModMatrixMenuInput(ch);
            // Don't return if 'q'/'Q' - let it propagate to quit check
            if (ch != 'q' && ch != 'Q') return;
        }

        switch (ch) {
            case KEY_UP:
                modMatrixCursorRow = (modMatrixCursorRow + 15) % 16;
                return;
            case KEY_DOWN:
                modMatrixCursorRow = (modMatrixCursorRow + 1) % 16;
                return;
            case KEY_LEFT:
                modMatrixCursorCol = (modMatrixCursorCol + 4) % 5;
                return;
            case KEY_RIGHT:
                modMatrixCursorCol = (modMatrixCursorCol + 1) % 5;
                return;
            case '\n':
            case KEY_ENTER:
                // Column 2 is Amount - use numeric input
                if (modMatrixCursorCol == 2) {
                    startModMatrixAmountInput();
                } else {
                    // All other columns use menu selection
                    startModMatrixMenu();
                }
                return;
        }
    }

    // LFO page no longer has zoom controls (using static waveform preview now)

    bool parameterPage = currentPage != UIPage::SEQUENCER && currentPage != UIPage::FM && currentPage != UIPage::MOD;

    auto selectPrevParameter = [&]() {
        if (pageParams.empty()) return;
        auto it = std::find(pageParams.begin(), pageParams.end(), selectedParameterId);
        if (it == pageParams.end()) {
            selectedParameterId = pageParams.front();
        } else if (it == pageParams.begin()) {
            selectedParameterId = pageParams.back();
        } else {
            selectedParameterId = *(--it);
        }
    };

    auto selectNextParameter = [&]() {
        if (pageParams.empty()) return;
        auto it = std::find(pageParams.begin(), pageParams.end(), selectedParameterId);
        if (it == pageParams.end()) {
            selectedParameterId = pageParams.front();
        } else if ((it + 1) == pageParams.end()) {
            selectedParameterId = pageParams.front();
        } else {
            selectedParameterId = *(++it);
        }
    };

    // Arrow key behavior for parameter pages
    // SAMPLER page: Up/Down navigate, Left/Right adjust
    // Other pages: Up/Down/Left/Right all navigate (legacy behavior)
    if (currentPage == UIPage::SAMPLER) {
        if (ch == KEY_UP) {
            selectPrevParameter();
            return;
        } else if (ch == KEY_DOWN) {
            selectNextParameter();
            return;
        } else if (ch == KEY_LEFT) {
            if (!pageParams.empty() && !numericInputActive) {
                if (std::find(pageParams.begin(), pageParams.end(), selectedParameterId) == pageParams.end()) {
                    selectedParameterId = pageParams.front();
                }
                adjustParameter(selectedParameterId, false, false);  // decrease, coarse
            }
            return;
        } else if (ch == KEY_RIGHT) {
            if (!pageParams.empty() && !numericInputActive) {
                if (std::find(pageParams.begin(), pageParams.end(), selectedParameterId) == pageParams.end()) {
                    selectedParameterId = pageParams.front();
                }
                adjustParameter(selectedParameterId, true, false);  // increase, coarse
            }
            return;
        }
    } else if (parameterPage && (ch == KEY_UP || ch == KEY_LEFT)) {
        selectPrevParameter();
        return;
    } else if (parameterPage && (ch == KEY_DOWN || ch == KEY_RIGHT)) {
        selectNextParameter();
        return;
    }

    // +/- keys adjust the currently selected parameter (Shift for fine control)
    if (parameterPage && !numericInputActive && !pageParams.empty()) {
        if (ch == '-' || ch == '_' || ch == '=' || ch == '+') {
            if (std::find(pageParams.begin(), pageParams.end(), selectedParameterId) == pageParams.end()) {
                selectedParameterId = pageParams.front();
            }
            bool fineAdjust = (ch == '_' || ch == '+');
            bool increase = (ch == '=' || ch == '+');
            adjustParameter(selectedParameterId, increase, fineAdjust);
            return;
        }
    }

    // Enter key starts numeric input for current parameter
    // (except on SEQUENCER and SAMPLER pages which have their own Enter key handlers)
    if (currentPage != UIPage::SEQUENCER && currentPage != UIPage::SAMPLER && (ch == '\n' || ch == KEY_ENTER)) {
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

    // Sampler selection (1-4 keys on SAMPLER page)
    if (currentPage == UIPage::SAMPLER) {
        switch (ch) {
            case '1': currentSamplerIndex = 0; addConsoleMessage("Sampler 1 selected"); return;
            case '2': currentSamplerIndex = 1; addConsoleMessage("Sampler 2 selected"); return;
            case '3': currentSamplerIndex = 2; addConsoleMessage("Sampler 3 selected"); return;
            case '4': currentSamplerIndex = 3; addConsoleMessage("Sampler 4 selected"); return;
            case '\n':
            case KEY_ENTER:
                // Open sample browser
                startSampleBrowser();
                return;
        }
    }

    // FM Matrix navigation and editing
    if (currentPage == UIPage::FM) {
        auto adjustFMDepth = [&](float delta) {
            float depth = params->getFMDepth(fmMatrixCursorCol, fmMatrixCursorRow);
            depth = std::max(-0.99f, std::min(0.99f, depth + delta));
            params->setFMDepth(fmMatrixCursorCol, fmMatrixCursorRow, depth);
        };

        switch (ch) {
            case KEY_UP:
                fmMatrixCursorRow = (fmMatrixCursorRow - 1 + 8) % 8;
                return;

            case KEY_DOWN:
                fmMatrixCursorRow = (fmMatrixCursorRow + 1) % 8;
                return;

            case KEY_LEFT:
                fmMatrixCursorCol = (fmMatrixCursorCol - 1 + 8) % 8;
                return;

            case KEY_RIGHT:
                fmMatrixCursorCol = (fmMatrixCursorCol + 1) % 8;
                return;

            case '+':
            case '=':
                adjustFMDepth(0.05f);
                return;

            case '-':
            case '_':
                adjustFMDepth(-0.05f);
                return;

            case ' ':  // Space bar: cycle 0 -> +99 -> -99 -> 0
                {
                    float depth = params->getFMDepth(fmMatrixCursorCol, fmMatrixCursorRow);
                    if (depth > 0.05f) {
                        params->setFMDepth(fmMatrixCursorCol, fmMatrixCursorRow, -0.99f);
                    } else if (depth < -0.05f) {
                        params->setFMDepth(fmMatrixCursorCol, fmMatrixCursorRow, 0.0f);
                    } else {
                        params->setFMDepth(fmMatrixCursorCol, fmMatrixCursorRow, 0.99f);
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

    // Mixer-specific hotkeys (only active on mixer page)
    if (currentPage == UIPage::MIXER) {
        // Determine which channel is selected based on selectedParameterId
        // OSC channels: IDs 50-53, SAMP channels: IDs 54-57
        int channelIndex = -1;
        bool isOscillator = false;

        if (selectedParameterId >= 50 && selectedParameterId <= 53) {
            channelIndex = selectedParameterId - 50;
            isOscillator = true;
        } else if (selectedParameterId >= 54 && selectedParameterId <= 57) {
            channelIndex = selectedParameterId - 54;
            isOscillator = false;
        }

        if (channelIndex >= 0 && channelIndex < 4) {
            switch (ch) {
                case 'M':
                case 'm':
                    // Toggle mute for selected channel
                    if (isOscillator) {
                        params->oscMuted[channelIndex] = !params->oscMuted[channelIndex].load();
                    } else {
                        params->samplerMuted[channelIndex] = !params->samplerMuted[channelIndex].load();
                    }
                    return;

                case 'S':
                case 's':
                    // Toggle solo for selected channel
                    if (isOscillator) {
                        params->oscSolo[channelIndex] = !params->oscSolo[channelIndex].load();
                    } else {
                        params->samplerSolo[channelIndex] = !params->samplerSolo[channelIndex].load();
                    }
                    return;
            }
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
