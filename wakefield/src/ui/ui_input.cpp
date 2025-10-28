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

    // Preset browser handling
    if (presetBrowserActive) {
        handlePresetBrowserInput(ch);
        // Don't return if 'q'/'Q' - let it propagate to quit check
        if (ch != 'q' && ch != 'Q') return;
    }

    // Main page action button and mixer handling
    if (currentPage == UIPage::MAIN && !textInputActive && !numericInputActive) {
        switch (ch) {
            case KEY_LEFT:
                mainPageFocusLeft = true;
                return;
            case KEY_RIGHT:
                mainPageFocusLeft = false;
                return;
            case KEY_UP:
                if (mainPageFocusLeft) {
                    if (mainPageActionIndex > 0) {
                        mainPageActionIndex--;
                    }
                } else {
                    if (mainPageMixerChannel > 0) {
                        mainPageMixerChannel--;
                    }
                }
                return;
            case KEY_DOWN:
                if (mainPageFocusLeft) {
                    if (mainPageActionIndex < 6) {
                        mainPageActionIndex++;
                    }
                } else {
                    if (mainPageMixerChannel < 11) {
                        mainPageMixerChannel++;
                    }
                }
                return;
            case '+':
            case '=':
                if (mainPageFocusLeft && mainPageActionIndex == 4) {  // Mutate Amount
                    globalMutatePercentage = std::min(100.0f, globalMutatePercentage + 5.0f);
                }
                return;
            case '-':
            case '_':
                if (mainPageFocusLeft && mainPageActionIndex == 4) {  // Mutate Amount
                    globalMutatePercentage = std::max(0.0f, globalMutatePercentage - 5.0f);
                }
                return;
            case '\n':
            case KEY_ENTER:
                if (mainPageFocusLeft) {
                    // Execute selected action
                    switch (mainPageActionIndex) {
                        case 0:  // Save Preset
                            startTextInput();
                            return;
                        case 1:  // Load Preset
                            startPresetBrowser();
                            return;
                        case 2:  // Global Randomize
                            randomizeAllParameters();
                            addConsoleMessage("Global randomize applied (whitelist-respecting)");
                            return;
                        case 3:  // Global Mutate
                            mutateAllParameters(std::clamp(globalMutatePercentage / 100.0f, 0.0f, 1.0f));
                            addConsoleMessage("Global mutate applied");
                            return;
                        case 4:  // Mutate Amount (no action on enter)
                            return;
                        case 5:  // Global Reset
                            resetAllParametersToNeutral();
                            addConsoleMessage("Global parameters reset to neutral");
                            return;
                        case 6:  // CPU Monitor Toggle
                            cpuMonitor.setEnabled(!cpuMonitor.isEnabled());
                            addConsoleMessage(cpuMonitor.isEnabled() ? "CPU Monitor: ON" : "CPU Monitor: OFF");
                            return;
                    }
                }
                return;

            // Mixer controls (only when focused on right column)
            case 'm': case 'M': {
                if (mainPageFocusLeft) return;
                // Toggle mute for selected channel
                if (mainPageMixerChannel < 4) {
                    params->oscMuted[mainPageMixerChannel] = !params->oscMuted[mainPageMixerChannel].load();
                } else if (mainPageMixerChannel < 8) {
                    int idx = mainPageMixerChannel - 4;
                    params->samplerMuted[idx] = !params->samplerMuted[idx].load();
                } else {
                    int idx = mainPageMixerChannel - 8;
                    params->chaosMuted[idx] = !params->chaosMuted[idx].load();
                }
                return;
            }
            case 's': case 'S': {
                if (mainPageFocusLeft) return;
                // Toggle solo for selected channel
                if (mainPageMixerChannel < 4) {
                    params->oscSolo[mainPageMixerChannel] = !params->oscSolo[mainPageMixerChannel].load();
                } else if (mainPageMixerChannel < 8) {
                    int idx = mainPageMixerChannel - 4;
                    params->samplerSolo[idx] = !params->samplerSolo[idx].load();
                } else {
                    int idx = mainPageMixerChannel - 8;
                    params->chaosSolo[idx] = !params->chaosSolo[idx].load();
                }
                return;
            }
            case '[': case ']': {
                if (mainPageFocusLeft) return;
                // Adjust level for selected channel
                float delta = (ch == ']') ? 0.05f : -0.05f;
                if (mainPageMixerChannel < 4) {
                    float newLevel = std::max(0.0f, std::min(1.0f, params->getOscLevel(mainPageMixerChannel) + delta));
                    params->setOscLevel(mainPageMixerChannel, newLevel);
                } else if (mainPageMixerChannel < 8) {
                    int idx = mainPageMixerChannel - 4;
                    float newLevel = std::max(0.0f, std::min(1.0f, synth->getSamplerLevel(idx) + delta));
                    synth->setSamplerLevel(idx, newLevel);
                }
                // TODO: Add chaos level control when chaos level parameters are added
                return;
            }
        }
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
        // Filter page uses dynamic parameter list based on filter type
        if (currentPage == UIPage::FILTER) {
            pageParams = getFilterParameterIds();
        } else {
            pageParams = getParameterIdsForPage(currentPage);
        }
    }

    auto setPage = [&](UIPage target) {
        currentPage = target;
        std::vector<int> newPageParams;
        if (currentPage == UIPage::FILTER) {
            newPageParams = getFilterParameterIds();
        } else {
            newPageParams = getParameterIdsForPage(currentPage);
        }
        if (!newPageParams.empty()) {
            selectedParameterId = newPageParams[0];
        }
    };

    // Tab key cycles forward through pages (matches F-key order)
    // F1=Main, F2=Osc, F3=Samp, F4=LFO, F5=Env, F6=Filter, F7=FX, F8=Chaos, F9=Mod, F10=FM, F11=Seq, F12=Config
    if (ch == '\t') {
        if (currentPage == UIPage::MAIN) setPage(UIPage::OSCILLATOR);
        else if (currentPage == UIPage::OSCILLATOR) setPage(UIPage::SAMPLER);
        else if (currentPage == UIPage::SAMPLER) setPage(UIPage::LFO);
        else if (currentPage == UIPage::LFO) setPage(UIPage::ENV);
        else if (currentPage == UIPage::ENV) setPage(UIPage::FILTER);
        else if (currentPage == UIPage::FILTER) setPage(UIPage::REVERB);
        else if (currentPage == UIPage::REVERB) setPage(UIPage::CHAOS);
        else if (currentPage == UIPage::CHAOS) setPage(UIPage::MOD);
        else if (currentPage == UIPage::MOD) setPage(UIPage::FM);
        else if (currentPage == UIPage::FM) setPage(UIPage::SEQUENCER);
        else if (currentPage == UIPage::SEQUENCER) setPage(UIPage::CONFIG);
        else if (currentPage == UIPage::CONFIG) setPage(UIPage::MAIN);
        else setPage(UIPage::MAIN);
        return;
    }

    // Shift+Tab cycles backward through pages (matches F-key order in reverse)
    if (ch == KEY_BTAB || ch == 353) {  // KEY_BTAB = Shift+Tab, 353 = some terminals
        if (currentPage == UIPage::MAIN) setPage(UIPage::CONFIG);
        else if (currentPage == UIPage::OSCILLATOR) setPage(UIPage::MAIN);
        else if (currentPage == UIPage::SAMPLER) setPage(UIPage::OSCILLATOR);
        else if (currentPage == UIPage::LFO) setPage(UIPage::SAMPLER);
        else if (currentPage == UIPage::ENV) setPage(UIPage::LFO);
        else if (currentPage == UIPage::FILTER) setPage(UIPage::ENV);
        else if (currentPage == UIPage::REVERB) setPage(UIPage::FILTER);
        else if (currentPage == UIPage::CHAOS) setPage(UIPage::REVERB);
        else if (currentPage == UIPage::MOD) setPage(UIPage::CHAOS);
        else if (currentPage == UIPage::FM) setPage(UIPage::MOD);
        else if (currentPage == UIPage::SEQUENCER) setPage(UIPage::FM);
        else if (currentPage == UIPage::CONFIG) setPage(UIPage::SEQUENCER);
        else setPage(UIPage::MAIN);
        return;
    }

    // Function keys jump directly to pages
    // F1=Main, F2=Osc, F3=Samp, F4=LFO, F5=Env, F6=Filter, F7=FX, F8=Chaos, F9=Mod, F10=FM, F11=Seq, F12=Config
    switch (ch) {
        case KEY_F(1):
            setPage(UIPage::MAIN);
            return;
        case KEY_F(2):
            setPage(UIPage::OSCILLATOR);
            return;
        case KEY_F(3):
            setPage(UIPage::SAMPLER);
            return;
        case KEY_F(4):
            setPage(UIPage::LFO);
            return;
        case KEY_F(5):
            setPage(UIPage::ENV);
            return;
        case KEY_F(6):
            setPage(UIPage::FILTER);
            return;
        case KEY_F(7):
            setPage(UIPage::REVERB);  // FX page (reverb)
            return;
        case KEY_F(8):
            setPage(UIPage::CHAOS);
            return;
        case KEY_F(9):
            setPage(UIPage::MOD);
            return;
        case KEY_F(10):
            setPage(UIPage::FM);
            return;
        case KEY_F(11):
            setPage(UIPage::SEQUENCER);
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
            case 'x':
            case 'X':
            case 'c':
            case 'C':
                // Clear the current modulation slot
                modulationSlots[modMatrixCursorRow] = ModulationSlot();
                addConsoleMessage("Cleared modulation slot " + std::to_string(modMatrixCursorRow + 1));
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
    // SAMPLER and FILTER pages: Up/Down navigate, Left/Right adjust
    // Other pages: Up/Down/Left/Right all navigate (legacy behavior)
    if (currentPage == UIPage::SAMPLER || currentPage == UIPage::FILTER) {
        if (ch == KEY_UP) {
            selectPrevParameter();
            return;
        } else if (ch == KEY_DOWN) {
            selectNextParameter();
            return;
        } else if (ch == KEY_LEFT || ch == KEY_SLEFT) {
            if (!pageParams.empty() && !numericInputActive) {
                if (std::find(pageParams.begin(), pageParams.end(), selectedParameterId) == pageParams.end()) {
                    selectedParameterId = pageParams.front();
                }
                // Skip adjustment for sample name (ID 69) - it's select-only
                if (selectedParameterId != 69) {
                    bool fine = (ch == KEY_SLEFT);  // Shift+Left for fine adjustment
                    adjustParameter(selectedParameterId, false, fine);  // decrease
                }
            }
            return;
        } else if (ch == KEY_RIGHT || ch == KEY_SRIGHT) {
            if (!pageParams.empty() && !numericInputActive) {
                if (std::find(pageParams.begin(), pageParams.end(), selectedParameterId) == pageParams.end()) {
                    selectedParameterId = pageParams.front();
                }
                // Skip adjustment for sample name (ID 69) - it's select-only
                if (selectedParameterId != 69) {
                    bool fine = (ch == KEY_SRIGHT);  // Shift+Right for fine adjustment
                    adjustParameter(selectedParameterId, true, fine);  // increase
                }
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
    // (except on SEQUENCER page which has its own Enter key handler)
    // SAMPLER page: Enter on sample name (ID 69) opens browser, otherwise numeric input
    if (currentPage == UIPage::SAMPLER && (ch == '\n' || ch == KEY_ENTER)) {
        if (selectedParameterId == 69) {
            // Sample name selected - open sample browser
            startSampleBrowser();
        } else {
            // Other parameter - normal numeric input
            startNumericInput(selectedParameterId);
        }
        return;
    } else if (currentPage != UIPage::SEQUENCER && (ch == '\n' || ch == KEY_ENTER)) {
        startNumericInput(selectedParameterId);
        return;
    }

    // 'H' key shows help (uppercase only to avoid conflicts)
    if (ch == 'H') {
        showHelp();
        return;
    }

    // 'L' starts MIDI learn; 'l' toggles parameter lock (randomize immunity)
    if (ch == 'L') {
        startMidiLearn(selectedParameterId);
        return;
    }
    if (ch == 'l') {
        if (currentPage == UIPage::FM) {
            int src = fmMatrixCursorRow;
            int tgt = fmMatrixCursorCol;
            fmMatrixLocked[tgt][src] = !fmMatrixLocked[tgt][src];
            addConsoleMessage(std::string("FM cell ") + std::to_string(src) + "," + std::to_string(tgt) + (fmMatrixLocked[tgt][src] ? " locked" : " unlocked"));
            return;
        }
        if (currentPage == UIPage::MOD) {
            modSlotLocked[modMatrixCursorRow] = !modSlotLocked[modMatrixCursorRow];
            addConsoleMessage(std::string("Mod slot ") + std::to_string(modMatrixCursorRow + 1) + (modSlotLocked[modMatrixCursorRow] ? " locked" : " unlocked"));
            return;
        }
        InlineParameter* p = getParameter(selectedParameterId);
        if (p) {
            p->randomizable = !p->randomizable;
            addConsoleMessage(std::string("Parameter ") + p->name + (p->randomizable ? ": unlocked" : ": locked"));
        }
        return;
    }

    // Per-page actions on parameter pages
    if (parameterPage) {
        if (ch == 'G') { randomizePageParameters(currentPage); addConsoleMessage("Page randomize applied"); return; }
        if (ch == 'M') { mutatePageParameters(currentPage, std::clamp(globalMutatePercentage/100.0f, 0.0f, 1.0f)); addConsoleMessage("Page mutate applied"); return; }
        if (ch == 'R') { resetPageParameters(currentPage); addConsoleMessage("Page parameters reset"); return; }
    }

    // Transport and looping hotkeys (keep these)
    if (ch == ' ') {
        // Spacebar behavior depends on current page
        if (currentPage == UIPage::SEQUENCER) {
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
        }
    }

    // Chaos selection (1-4 keys on CHAOS page)
    if (currentPage == UIPage::CHAOS) {
        switch (ch) {
            case '1': currentChaosIndex = 0; addConsoleMessage("Chaos 1 selected"); return;
            case '2': currentChaosIndex = 1; addConsoleMessage("Chaos 2 selected"); return;
            case '3': currentChaosIndex = 2; addConsoleMessage("Chaos 3 selected"); return;
            case '4': currentChaosIndex = 3; addConsoleMessage("Chaos 4 selected"); return;
        }
    }

    // Chaos reset action (shift+r when on CHAOS page)
    if (currentPage == UIPage::CHAOS && ch == 'R') {
        synth->resetChaosGenerator(currentChaosIndex);
        addConsoleMessage("Chaos generator reset");
        return;
    }

    // FM Matrix navigation and editing (16 sources × 8 targets)
    if (currentPage == UIPage::FM) {
        auto adjustFMDepth = [&](float delta) {
            float depth = params->getFMDepth(fmMatrixCursorCol, fmMatrixCursorRow);
            depth = std::max(-0.99f, std::min(0.99f, depth + delta));
            params->setFMDepth(fmMatrixCursorCol, fmMatrixCursorRow, depth);
        };

        switch (ch) {
            case KEY_UP:
                fmMatrixCursorRow = (fmMatrixCursorRow - 1 + 16) % 16;
                return;

            case KEY_DOWN:
                fmMatrixCursorRow = (fmMatrixCursorRow + 1) % 16;
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

            case 'g':
            case 'G':  // Generate/Randomize FM matrix (skip locked)
                {
                    for (int target = 0; target < 8; ++target) {
                        for (int source = 0; source < 16; ++source) {
                            if (fmMatrixLocked[target][source]) continue;
                            float randomDepth = (static_cast<float>(rand()) / RAND_MAX) * 1.98f - 0.99f;  // -0.99 to +0.99
                            params->setFMDepth(target, source, randomDepth * 0.3f);  // Scale to 30% for subtler results
                        }
                    }
                    addConsoleMessage("FM matrix randomized");
                }
                return;

            case 'r':
            case 'R':  // Reset FM matrix to zero (skip locked)
                {
                    for (int target = 0; target < 8; ++target) {
                        for (int source = 0; source < 16; ++source) {
                            if (!fmMatrixLocked[target][source]) {
                                params->setFMDepth(target, source, 0.0f);
                            }
                        }
                    }
                    addConsoleMessage("FM matrix reset");
                }
                return;

            case 'm':
            case 'M':  // Mutate FM matrix (20% variation, skip locked)
                {
                    for (int target = 0; target < 8; ++target) {
                        for (int source = 0; source < 16; ++source) {
                            float currentDepth = params->getFMDepth(target, source);
                            if (currentDepth != 0.0f && !fmMatrixLocked[target][source]) {  // Only mutate non-zero values
                                float variation = (static_cast<float>(rand()) / RAND_MAX) * 0.4f - 0.2f;  // ±20%
                                float newDepth = std::max(-0.99f, std::min(0.99f, currentDepth + variation));
                                params->setFMDepth(target, source, newDepth);
                            }
                        }
                    }
                    addConsoleMessage("FM matrix mutated (±20%)");
                }
                return;
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
