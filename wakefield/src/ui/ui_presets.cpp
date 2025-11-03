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

    // Load the combined preset file if present
    std::ifstream pf(presetPath);
    std::string full;
    if (pf.good()) {
        std::stringstream ss; ss << pf.rdbuf();
        full = ss.str();
    }

    // Always parse base INI via PresetManager (unknown lines/blocks are ignored)
    PresetManager::loadPreset(filename, params);

    currentPresetName = filename;
    resetUndoHistory();

    auto findBlock = [&](const char* beginTag, const char* endTag) -> std::string {
        size_t b = full.find(beginTag);
        size_t e = full.find(endTag);
        if (b == std::string::npos || e == std::string::npos || e <= b) return {};
        b += std::char_traits<char>::length(beginTag);
        return full.substr(b, e - b);
    };

    // 1) UI state block
    try {
        std::string state = findBlock("#STATE_BEGIN\n", "#STATE_END");
        if (!state.empty()) {
            applySerializedState(state);
        }
    } catch (...) {}

    // 2) Sequencer meta
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
    }

    // 3) Sequencer tracks CSV
    auto loadTrackCsv = [&](int idx, const std::string& csv){
        if (!sequencer || csv.empty()) return;
        Pattern& pat = sequencer->getTrack(idx).getPattern();
        std::istringstream f(csv);
        std::string line;
        std::getline(f, line); // header
        int stepIndex = 0;
        while (std::getline(f, line) && stepIndex < pat.getLength()) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string token;
            std::getline(ss, token, ','); // step
            std::getline(ss, token, ','); pat.getStep(stepIndex).active = (token == "1");
            std::getline(ss, token, ','); pat.getStep(stepIndex).locked = (token == "1");
            std::getline(ss, token, ','); pat.getStep(stepIndex).midiNote = std::stoi(token);
            std::getline(ss, token, ','); pat.getStep(stepIndex).velocity = std::stoi(token);
            std::getline(ss, token, ','); pat.getStep(stepIndex).gateLength = std::stof(token);
            std::getline(ss, token, ','); pat.getStep(stepIndex).probability = std::stof(token);
            stepIndex++;
        }
    };
    loadTrackCsv(0, findBlock("#SEQ_TRACK1_BEGIN\n", "#SEQ_TRACK1_END"));
    loadTrackCsv(1, findBlock("#SEQ_TRACK2_BEGIN\n", "#SEQ_TRACK2_END"));
    loadTrackCsv(2, findBlock("#SEQ_TRACK3_BEGIN\n", "#SEQ_TRACK3_END"));
    loadTrackCsv(3, findBlock("#SEQ_TRACK4_BEGIN\n", "#SEQ_TRACK4_END"));

    // 4) Samplers + sample_dir
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
                    if (key.rfind("sampler", 0) == 0 && key.size() > 8) {
                        int idx = std::stoi(key.substr(7)); if (idx < 0 || idx >= 4) continue;
                        if (!val.empty() && bank) {
                            int sidx = bank->findSampleByName(val.c_str());
                            if (sidx < 0 && !sampleDir.empty()) {
                                bank->loadSamplesFromDirectory(sampleDir.c_str());
                                sidx = bank->findSampleByName(val.c_str());
                            }
                            if (sidx >= 0) synth->setSamplerSample(idx, sidx);
                        }
                    }
                }
                if (!sampleDir.empty()) setSampleDirectory(sampleDir);
            }
        } catch (...) {}
    }

    // Fallbacks: handle bundles/sidecars for legacy presets
    if (full.empty()) {
        // If no unified content read, try legacy bundle/sidecars
        // (re-use previous legacy flow)
        // If sidecars missing but bundle exists, import it first
        std::ifstream stateCheck(baseDir + "/" + filename + ".state");
        if (!stateCheck.good()) {
            std::string bundle = baseDir + "/" + filename + ".wkbundle";
            std::ifstream b(bundle);
            if (b.good()) {
                PresetManager::importPresetBundle(bundle);
            }
        }
        // Load state
        try {
            std::ifstream f(baseDir + "/" + filename + ".state"); if (f.good()) { std::stringstream s; s << f.rdbuf(); applySerializedState(s.str()); }
        } catch (...) {}
        // Load patterns
        if (sequencer) {
            for (int t = 0; t < 4; ++t) {
                std::string trackPath = baseDir + "/" + filename + "_track" + std::to_string(t+1) + ".csv";
                try { sequencer->getTrack(t).getPattern().loadFromFile(trackPath); } catch (...) {}
            }
        }
    }

    // Capture baseline from unified content if present; otherwise build from current state
    if (!full.empty()) {
        baselinePresetContent = full;
        baselineAvailable = true;
    } else {
        baselinePresetContent = buildUnifiedPresetContent(filename);
        baselineAvailable = true;
    }
}

void UI::savePreset(const std::string& filename) {
    // Build a single, unified preset file
    const std::string baseDir = PresetManager::getPresetDirectory();
    const std::string presetPath = baseDir + "/" + filename + ".txt";

    std::string content = buildUnifiedPresetContent(filename);
    // Write unified file
    try {
        std::ofstream f(presetPath);
        if (f.is_open()) {
            f << content;
        }
    } catch (...) {}

    currentPresetName = filename;
    refreshPresetList();

    // Update baseline to the saved content
    baselinePresetContent = content;
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
                    if (key.rfind("sampler", 0) == 0 && key.size() > 8) {
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
