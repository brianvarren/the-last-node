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
    // If sidecars missing but bundle exists, import it first
    {
        std::string baseDir = PresetManager::getPresetDirectory();
        std::ifstream stateCheck(baseDir + "/" + filename + ".state");
        if (!stateCheck.good()) {
            std::string bundle = baseDir + "/" + filename + ".wkbundle";
            std::ifstream b(bundle);
            if (b.good()) {
                PresetManager::importPresetBundle(bundle);
            }
        }
    }
    if (!PresetManager::loadPreset(filename, params)) return;

    currentPresetName = filename;
    resetUndoHistory();

    std::string baseDir = PresetManager::getPresetDirectory();

    // 1) Load serialized UI state sidecar if present (captures all parameters, FM, MOD, UI indices)
    try {
        std::string statePath = baseDir + "/" + filename + ".state";
        std::ifstream f(statePath);
        if (f.good()) {
            std::stringstream ss; ss << f.rdbuf();
            applySerializedState(ss.str());
        }
    } catch (...) {
        // Ignore state load errors
    }

    // 2) Load sequencer patterns
    if (sequencer) {
        for (int t = 0; t < 4; ++t) {
            std::string trackPath = baseDir + "/" + filename + "_track" + std::to_string(t+1) + ".csv";
            try {
                sequencer->getTrack(t).getPattern().loadFromFile(trackPath);
            } catch (...) {
            }
        }
    }

    // 3) Load sequencer meta (tempo, track constraints, euclidean, mute/solo, subdivision, rotation, phase driver)
    if (sequencer) {
        try {
            std::string seqMetaPath = baseDir + "/" + filename + ".seq.txt";
            std::ifstream f(seqMetaPath);
            if (f.good()) {
                std::string line;
                while (std::getline(f, line)) {
                    if (line.empty() || line[0] == '#') continue;
                    auto eq = line.find('=');
                    if (eq == std::string::npos) continue;
                    std::string key = line.substr(0, eq);
                    std::string val = line.substr(eq + 1);
                    auto trim = [](std::string s){
                        size_t a = s.find_first_not_of(" \t\r\n");
                        size_t b = s.find_last_not_of(" \t\r\n");
                        if (a == std::string::npos) return std::string();
                        return s.substr(a, b - a + 1);
                    };
                    key = trim(key); val = trim(val);

                    if (key == "tempo") {
                        try { sequencer->setTempo(std::stod(val)); } catch (...) {}
                        continue;
                    }
                    // Keys are of the form t{idx}.field
                    if (key.rfind("t", 0) == 0 && key.size() > 2) {
                        size_t dot = key.find('.');
                        if (dot == std::string::npos) continue;
                        int tIdx = std::stoi(key.substr(1, dot - 1));
                        if (tIdx < 0 || tIdx >= 4) continue;
                        std::string field = key.substr(dot + 1);

                        auto& track = sequencer->getTrack(tIdx);
                        auto& constraints = track.getConstraints();
                        auto& euclid = track.getEuclideanPattern();

                        try {
                            if (field == "len") {
                                track.getPattern().setLength(std::stoi(val));
                            } else if (field == "subdiv") {
                                int denom = std::stoi(val);
                                track.setSubdivision(static_cast<Subdivision>(denom));
                            } else if (field == "muted") {
                                track.setMuted(val == "1" || val == "true");
                            } else if (field == "solo") {
                                track.setSolo(val == "1" || val == "true");
                            } else if (field == "eu_hits") {
                                euclid.setHits(std::stoi(val));
                            } else if (field == "eu_steps") {
                                euclid.setSteps(std::stoi(val));
                            } else if (field == "eu_rot") {
                                euclid.setRotation(std::stoi(val));
                            } else if (field == "scale") {
                                constraints.setScale(static_cast<Scale>(std::stoi(val)));
                            } else if (field == "root") {
                                constraints.setRootNote(std::stoi(val));
                            } else if (field == "oct_min") {
                                int minOct = std::stoi(val);
                                constraints.setOctaveRange(minOct, constraints.getOctaveMax());
                            } else if (field == "oct_max") {
                                int maxOct = std::stoi(val);
                                constraints.setOctaveRange(constraints.getOctaveMin(), maxOct);
                            } else if (field == "density") {
                                constraints.setDensity(std::stof(val));
                            } else if (field == "max_interval") {
                                constraints.setMaxInterval(std::stoi(val));
                            } else if (field == "contour") {
                                constraints.setContour(static_cast<Contour>(std::stoi(val)));
                            } else if (field == "gravity") {
                                constraints.setGravityNote(std::stoi(val));
                            } else if (field == "phase_driver") {
                                sequencer->setTrackPhaseDriver(tIdx,
                                    static_cast<Sequencer::PhaseDriver>(std::stoi(val)));
                            }
                        } catch (...) {
                            // Ignore malformed values
                        }
                    }
                }
            }
        } catch (...) {
        }
    }

    // 4) Load sampler selections (sample names + optional directory)
    if (synth) {
        try {
            std::string sampPath = baseDir + "/" + filename + ".samplers.txt";
            std::ifstream f(sampPath);
            if (f.good()) {
                auto* bank = synth->getSampleBank();
                std::string line;
                std::string sampleDir;
                while (std::getline(f, line)) {
                    if (line.empty() || line[0] == '#') continue;
                    auto eq = line.find('=');
                    if (eq == std::string::npos) continue;
                    std::string key = line.substr(0, eq);
                    std::string val = line.substr(eq + 1);
                    auto trim = [](std::string s){
                        size_t a = s.find_first_not_of(" \t\r\n");
                        size_t b = s.find_last_not_of(" \t\r\n");
                        if (a == std::string::npos) return std::string();
                        return s.substr(a, b - a + 1);
                    };
                    key = trim(key); val = trim(val);
                    if (key == "sample_dir") {
                        sampleDir = val;
                        continue;
                    }
                    if (key.rfind("sampler", 0) == 0 && key.size() > 8) {
                        int idx = std::stoi(key.substr(7));
                        if (idx < 0 || idx >= 4) continue;
                        if (!val.empty() && bank) {
                            int sidx = bank->findSampleByName(val.c_str());
                            if (sidx < 0 && !sampleDir.empty()) {
                                // Try loading samples from saved directory to resolve name
                                bank->loadSamplesFromDirectory(sampleDir.c_str());
                                sidx = bank->findSampleByName(val.c_str());
                            }
                            if (sidx >= 0) {
                                synth->setSamplerSample(idx, sidx);
                            }
                        }
                    }
                }
                if (!sampleDir.empty()) {
                    // Remember sample directory in UI for browsing
                    setSampleDirectory(sampleDir);
                }
            }
        } catch (...) {
        }
    }
}

void UI::savePreset(const std::string& filename) {
    if (!PresetManager::savePreset(filename, params)) return;

    std::string baseDir = PresetManager::getPresetDirectory();

    // 1) Save full UI state as sidecar
    try {
        std::string statePath = baseDir + "/" + filename + ".state";
        std::ofstream f(statePath);
        if (f.is_open()) {
            f << serializeState();
        }
    } catch (...) {
        // Ignore write errors
    }

    // 2) Save sequencer patterns and meta
    if (sequencer) {
        for (int t = 0; t < 4; ++t) {
            std::string trackPath = baseDir + "/" + filename + "_track" + std::to_string(t+1) + ".csv";
            sequencer->getTrack(t).getPattern().saveToFile(trackPath);
        }
        try {
            std::string seqMetaPath = baseDir + "/" + filename + ".seq.txt";
            std::ofstream sm(seqMetaPath);
            if (sm.is_open()) {
                sm << "tempo=" << sequencer->getTempo() << "\n";
                for (int t = 0; t < 4; ++t) {
                    const auto& track = sequencer->getTrack(t);
                    const auto& constraints = track.getConstraints();
                    const auto& euclid = track.getEuclideanPattern();
                    sm << "t" << t << ".len=" << track.getPattern().getLength() << "\n";
                    sm << "t" << t << ".subdiv=" << static_cast<int>(track.getPattern().getResolution()) << "\n";
                    sm << "t" << t << ".muted=" << (track.isMuted() ? 1 : 0) << "\n";
                    sm << "t" << t << ".solo=" << (track.isSolo() ? 1 : 0) << "\n";
                    sm << "t" << t << ".eu_hits=" << euclid.getHits() << "\n";
                    sm << "t" << t << ".eu_steps=" << euclid.getSteps() << "\n";
                    sm << "t" << t << ".eu_rot=" << euclid.getRotation() << "\n";
                    sm << "t" << t << ".scale=" << static_cast<int>(constraints.getScale()) << "\n";
                    sm << "t" << t << ".root=" << constraints.getRootNote() << "\n";
                    sm << "t" << t << ".oct_min=" << constraints.getOctaveMin() << "\n";
                    sm << "t" << t << ".oct_max=" << constraints.getOctaveMax() << "\n";
                    sm << "t" << t << ".density=" << constraints.getDensity() << "\n";
                    sm << "t" << t << ".max_interval=" << constraints.getMaxInterval() << "\n";
                    sm << "t" << t << ".contour=" << static_cast<int>(constraints.getContour()) << "\n";
                    sm << "t" << t << ".gravity=" << constraints.getGravityNote() << "\n";
                    sm << "t" << t << ".phase_driver=" << static_cast<int>(sequencer->getTrackPhaseDriver(t)) << "\n";
                }
            }
        } catch (...) {
        }
    }

    // 3) Save sampler selections (by sample name) and current sample directory
    if (synth) {
        try {
            std::string sampPath = baseDir + "/" + filename + ".samplers.txt";
            std::ofstream sf(sampPath);
            if (sf.is_open()) {
                // Save current sample directory (write even if empty)
                sf << "sample_dir=" << getSampleDirectory() << "\n";
                for (int i = 0; i < 4; ++i) {
                    int sidx = synth->getSamplerSampleIndex(i);
                    const SampleBank* bank = synth->getSampleBank();
                    const char* name = (bank && sidx >= 0) ? bank->getSampleName(sidx) : nullptr;
                    sf << "sampler" << i << '=' << (name ? name : "") << "\n";
                }
            }
        } catch (...) {
        }
    }

    currentPresetName = filename;
    refreshPresetList();

    // Also create a single-file bundle for sharing
    PresetManager::exportPresetBundle(filename);
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
