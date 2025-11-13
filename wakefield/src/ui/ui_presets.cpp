#include "../ui.h"
#include "../preset.h"
#include "../sequencer.h"
#include <fstream>
#include <sstream>

extern Sequencer* sequencer;

void UI::refreshPresetList() {
    availablePresets = PresetManager::listPresets();
}

void UI::loadPreset(const std::string& filename) {
    std::string baseDir = PresetManager::getPresetDirectory();
    std::string presetPath = baseDir + "/" + filename + ".txt";
    loadPresetFromPath(presetPath, filename);
}

void UI::loadPresetFromPath(const std::string& path, const std::string& displayName) {
    pushAutosaveSuppression();

    std::ifstream pf(path);
    std::string full;
    if (pf.good()) {
        std::stringstream ss; ss << pf.rdbuf();
        full = ss.str();
    }

    if (!PresetManager::loadPresetFromPath(path, params)) {
        addConsoleMessage("Failed to load preset: " + displayName);
        popAutosaveSuppression();
        return;
    }

    currentPresetName = displayName;
    resetUndoHistory();
    applyUnifiedPresetContent(full);

    if (!full.empty()) {
        baselinePresetContent = full;
        baselineAvailable = true;
    } else {
        baselinePresetContent = buildUnifiedPresetContent(displayName);
        baselineAvailable = true;
    }

    popAutosaveSuppression();
    notifyAutosaveNeeded("preset_load");
}

void UI::savePreset(const std::string& filename) {
    const std::string baseDir = PresetManager::getPresetDirectory();
    const std::string presetPath = baseDir + "/" + filename + ".txt";

    std::string cachedContent;
    if (!writeUnifiedPresetToPath(presetPath, filename, &cachedContent)) {
        addConsoleMessage("Failed to save preset: " + filename);
        return;
    }

    currentPresetName = filename;
    refreshPresetList();

    baselinePresetContent = cachedContent.empty() ? buildUnifiedPresetContent(filename) : cachedContent;
    baselineAvailable = true;
}

std::string UI::buildUnifiedPresetContent(const std::string& name) {
    std::ostringstream out;
    out << PresetManager::serializeToString(params);
    out << "\n#STATE_BEGIN\n" << serializeState() << "#STATE_END\n";
    if (sequencer) {
        out << "#SEQ_META_BEGIN\n";
        out << "tempo=" << sequencer->getTempo() << "\n";
        for (int t = 0; t < 4; ++t) {
            const auto& track = sequencer->getTrack(t);
            const auto& constraints = track.getConstraints();
            const auto& euclid = track.getEuclideanPattern();
            out << "t" << t << ".len=" << track.getPattern().getLength() << "\n";
            out << "t" << t << ".subdiv=" << static_cast<int>(track.getPattern().getResolution()) << "\n";
            out << "t" << t << ".muted=" << (track.isMuted() ? 1 : 0) << "\n";
            out << "t" << t << ".solo=" << (track.isSolo() ? 1 : 0) << "\n";
            out << "t" << t << ".eu_hits=" << euclid.getHits() << "\n";
            out << "t" << t << ".eu_steps=" << euclid.getSteps() << "\n";
            out << "t" << t << ".eu_rot=" << euclid.getRotation() << "\n";
            out << "t" << t << ".scale=" << static_cast<int>(constraints.getScale()) << "\n";
            out << "t" << t << ".root=" << constraints.getRootNote() << "\n";
            out << "t" << t << ".oct_min=" << constraints.getOctaveMin() << "\n";
            out << "t" << t << ".oct_max=" << constraints.getOctaveMax() << "\n";
            out << "t" << t << ".density=" << constraints.getDensity() << "\n";
            out << "t" << t << ".max_interval=" << constraints.getMaxInterval() << "\n";
            out << "t" << t << ".contour=" << static_cast<int>(constraints.getContour()) << "\n";
            out << "t" << t << ".gravity=" << constraints.getGravityNote() << "\n";
            out << "t" << t << ".phase_driver=" << static_cast<int>(sequencer->getTrackPhaseDriver(t)) << "\n";
        }
        out << "#SEQ_META_END\n";
    }
    auto writeTrackCsv = [&](int idx){
        if (!sequencer) return;
        const Pattern& pat = sequencer->getTrack(idx).getPattern();
        out << "#SEQ_TRACK" << (idx+1) << "_BEGIN\n";
        out << "step,active,locked,note,velocity,gate,probability\n";
        for (int i = 0; i < pat.getLength(); ++i) {
            const auto& s = pat.getStep(i);
            out << i << "," << (s.active?1:0) << "," << (s.locked?1:0) << ","
                << s.midiNote << "," << s.velocity << "," << s.gateLength << "," << s.probability << "\n";
        }
        out << "#SEQ_TRACK" << (idx+1) << "_END\n";
    };
    writeTrackCsv(0); writeTrackCsv(1); writeTrackCsv(2); writeTrackCsv(3);
    if (synth) {
        out << "#SAMPLERS_BEGIN\n";
        out << "sample_dir=" << getSampleDirectory() << "\n";
        for (int i = 0; i < 4; ++i) {
            int sidx = synth->getSamplerSampleIndex(i);
            const SampleBank* bank = synth->getSampleBank();
            const char* name = (bank && sidx >= 0) ? bank->getSampleName(sidx) : nullptr;
            out << "sampler" << i << '=' << (name ? name : "") << "\n";
        }
        out << "#SAMPLERS_END\n";
    }
    return out.str();
}

bool UI::writeUnifiedPresetToPath(const std::string& path, const std::string& name, std::string* outContent) {
    std::string content = buildUnifiedPresetContent(name);
    if (outContent) {
        *outContent = content;
    }
    try {
        std::ofstream f(path);
        if (!f.is_open()) {
            return false;
        }
        f << content;
    } catch (...) {
        return false;
    }
    return true;
}

void UI::applyUnifiedPresetContent(const std::string& full) {
    if (full.empty()) return;
    auto findBlock = [&](const char* beginTag, const char* endTag) -> std::string {
        size_t b = full.find(beginTag);
        size_t e = full.find(endTag);
        if (b == std::string::npos || e == std::string::npos || e <= b) return {};
        b += std::char_traits<char>::length(beginTag);
        return full.substr(b, e - b);
    };
    try { std::string state = findBlock("#STATE_BEGIN\n", "#STATE_END"); if (!state.empty()) applySerializedState(state); } catch (...) {}
    if (sequencer) {
        try {
            std::string meta = findBlock("#SEQ_META_BEGIN\n", "#SEQ_META_END");
            if (!meta.empty()) {
                std::istringstream f(meta);
                std::string line;
                auto trim = [](std::string s){ size_t a=s.find_first_not_of(" \t\r\n"); size_t b=s.find_last_not_of(" \t\r\n"); if (a==std::string::npos) return std::string(); return s.substr(a,b-a+1); };
                while (std::getline(f, line)) {
                    if (line.empty() || line[0] == '#') continue;
                    auto eq = line.find('='); if (eq == std::string::npos) continue;
                    std::string key = trim(line.substr(0, eq));
                    std::string val = trim(line.substr(eq + 1));
                    if (key == "tempo") { try { sequencer->setTempo(std::stod(val)); } catch (...) {} continue; }
                    if (key.rfind("t", 0) == 0 && key.size() > 2) {
                        size_t dot = key.find('.'); if (dot == std::string::npos) continue;
                        int tIdx = std::stoi(key.substr(1, dot - 1)); if (tIdx < 0 || tIdx >= 4) continue;
                        std::string field = key.substr(dot + 1);
                        auto& track = sequencer->getTrack(tIdx);
                        auto& constraints = track.getConstraints();
                        auto& euclid = track.getEuclideanPattern();
                        try {
                            if (field == "len") track.getPattern().setLength(std::stoi(val));
                            else if (field == "subdiv") track.setSubdivision(static_cast<Subdivision>(std::stoi(val)));
                            else if (field == "muted") track.setMuted(val == "1" || val == "true");
                            else if (field == "solo") track.setSolo(val == "1" || val == "true");
                            else if (field == "eu_hits") euclid.setHits(std::stoi(val));
                            else if (field == "eu_steps") euclid.setSteps(std::stoi(val));
                            else if (field == "eu_rot") euclid.setRotation(std::stoi(val));
                            else if (field == "scale") constraints.setScale(static_cast<Scale>(std::stoi(val)));
                            else if (field == "root") constraints.setRootNote(std::stoi(val));
                            else if (field == "oct_min") constraints.setOctaveRange(std::stoi(val), constraints.getOctaveMax());
                            else if (field == "oct_max") constraints.setOctaveRange(constraints.getOctaveMin(), std::stoi(val));
                            else if (field == "density") constraints.setDensity(std::stof(val));
                            else if (field == "max_interval") constraints.setMaxInterval(std::stoi(val));
                            else if (field == "contour") constraints.setContour(static_cast<Contour>(std::stoi(val)));
                            else if (field == "gravity") constraints.setGravityNote(std::stoi(val));
                            else if (field == "phase_driver") sequencer->setTrackPhaseDriver(tIdx, static_cast<Sequencer::PhaseDriver>(std::stoi(val)));
                        } catch (...) {}
                    }
                }
            }
        } catch (...) {}
        auto loadCsv = [&](int idx){ std::string csv = findBlock((std::string("#SEQ_TRACK") + std::to_string(idx+1) + "_BEGIN\n").c_str(), (std::string("#SEQ_TRACK") + std::to_string(idx+1) + "_END").c_str()); if (csv.empty()) return; Pattern& pat = sequencer->getTrack(idx).getPattern(); std::istringstream f(csv); std::string line; std::getline(f,line); int i=0; while (std::getline(f,line) && i<pat.getLength()){ if(line.empty()) continue; std::stringstream ss(line); std::string tok; std::getline(ss,tok,','); std::getline(ss,tok,','); pat.getStep(i).active=(tok=="1"); std::getline(ss,tok,','); pat.getStep(i).locked=(tok=="1"); std::getline(ss,tok,','); pat.getStep(i).midiNote=std::stoi(tok); std::getline(ss,tok,','); pat.getStep(i).velocity=std::stoi(tok); std::getline(ss,tok,','); pat.getStep(i).gateLength=std::stof(tok); std::getline(ss,tok,','); pat.getStep(i).probability=std::stof(tok); ++i; } };
        loadCsv(0); loadCsv(1); loadCsv(2); loadCsv(3);
    }
    if (synth) {
        try {
            std::string sblock = findBlock("#SAMPLERS_BEGIN\n", "#SAMPLERS_END");
            if (!sblock.empty()) {
                auto* bank = synth->getSampleBank();
                std::istringstream f(sblock);
                std::string line; std::string sampleDir;
                auto trim = [](std::string s){ size_t a=s.find_first_not_of(" \t\r\n"); size_t b=s.find_last_not_of(" \t\r\n"); if (a==std::string::npos) return std::string(); return s.substr(a,b-a+1); };
                while (std::getline(f, line)) {
                    if (line.empty() || line[0] == '#') continue;
                    auto eq = line.find('='); if (eq == std::string::npos) continue;
                    std::string key = trim(line.substr(0, eq));
                    std::string val = trim(line.substr(eq + 1));
                    if (key == "sample_dir") { sampleDir = val; continue; }
                    if (key.rfind("sampler", 0) == 0 && key.size() > 7) {
                        int idx = std::stoi(key.substr(7)); if (idx < 0 || idx >= 4) continue;
                        if (!val.empty() && bank) {
                            int sidx = bank->findSampleByName(val.c_str());
                            if (sidx < 0 && !sampleDir.empty()) { bank->loadSamplesFromDirectory(sampleDir.c_str()); sidx = bank->findSampleByName(val.c_str()); }
                            if (sidx >= 0) synth->setSamplerSample(idx, sidx);
                        }
                    }
                }
                if (!sampleDir.empty()) setSampleDirectory(sampleDir);
            }
        } catch (...) {}
    }
}

void UI::resetToBaseline() {
    if (baselineAvailable && !baselinePresetContent.empty()) {
        captureUndoSnapshot("reset_baseline");
        applyUnifiedPresetContent(baselinePresetContent);
        addConsoleMessage("Restored to preset baseline");
        return;
    }
    // Fallback: neutral reset
    resetAllParametersToNeutral();
    addConsoleMessage("Baseline unavailable; reset to neutral");
}

void UI::finishGlobalResetPopup(bool applySelection) {
    if (!globalResetPopupActive) return;
    if (applySelection) {
        if (globalResetPopupIndex == 0) {
            resetToBaseline();
        } else {
            // Full re-initialization: reset engine state and parameters
            if (synth) {
                synth->resetAudioState();
            }
            if (params) {
                params->resetToInitDefaults();
            }
            // Reset UI modulation state and locks to init
            for (int i = 0; i < 16; ++i) {
                modulationSlots[i] = ModulationSlot();
                modSlotLocked[i] = false;
            }
            for (int t = 0; t < kFMTargetCount; ++t) {
                for (int s = 0; s < kFMSourceCount; ++s) {
                    fmMatrixLocked[t][s] = true; // default: locked for safety
                }
            }
            addConsoleMessage("Reset to init defaults (engine + params reinitialized)");
        }
    }
    globalResetPopupActive = false;
}

void UI::handleGlobalResetPopupInput(int ch) {
    switch (ch) {
        case KEY_UP:
        case 'k':
        case 'K':
            globalResetPopupIndex = (globalResetPopupIndex - 1 + 2) % 2;
            break;
        case KEY_DOWN:
        case 'j':
        case 'J':
            globalResetPopupIndex = (globalResetPopupIndex + 1) % 2;
            break;
        case '\n':
        case KEY_ENTER:
            finishGlobalResetPopup(true);
            break;
        case 27:  // Esc
            finishGlobalResetPopup(false);
            break;
        default:
            break;
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

void UI::finishQuitPopup(bool confirmed) {
    quitPopupActive = false;
    // The confirmed flag will be checked by the caller
}

void UI::handleQuitPopupInput(int ch) {
    switch (ch) {
        case 'y':
        case 'Y':
            finishQuitPopup(true);
            break;
        case 'n':
        case 'N':
        case 27:  // Esc
            finishQuitPopup(false);
            break;
        default:
            break;
    }
}

void UI::finishClearModMatrixPopup(bool confirmed) {
    clearModMatrixPopupActive = false;
    if (confirmed) {
        captureUndoSnapshot("clear_all_mod_slots");
        for (int i = 0; i < 16; ++i) {
            if (!modSlotLocked[i]) {
                modulationSlots[i] = ModulationSlot();
            }
        }
        addConsoleMessage("Cleared all unlocked modulation slots");
    }
}

void UI::handleClearModMatrixPopupInput(int ch) {
    switch (ch) {
        case 'y':
        case 'Y':
            finishClearModMatrixPopup(true);
            break;
        case 'n':
        case 'N':
        case 27:  // Esc
            finishClearModMatrixPopup(false);
            break;
        default:
            break;
    }
}
