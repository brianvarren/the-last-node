#define RTAUDIO_EXCEPTIONS
#include <RtAudio.h>
#include <iostream>
#include <fstream>
#include <csignal>
#include <unistd.h>
#include <locale.h>
#include <cmath>
#include <sys/stat.h>
#include <pwd.h>
#include "synth.h"
#include "midi.h"
#include "ui.h"
#include "preset.h"
#include "loop_manager.h"
#include "parameter_smoother.h"
#include "sequencer.h"

// Global instances
static Synth* synth = nullptr;
static MidiHandler* midiHandler = nullptr;
static SynthParameters* synthParams = nullptr;
static UI* ui = nullptr;
LoopManager* loopManager = nullptr;  // Non-static so UI can access it
Sequencer* sequencer = nullptr;  // Non-static so UI can access it
static bool running = true;

void signalHandler(int signum) {
    running = false;
}

// Helper to get config directory
std::string getConfigDirectory() {
    const char* homeDir = getenv("HOME");
    if (!homeDir) {
        struct passwd* pw = getpwuid(getuid());
        homeDir = pw->pw_dir;
    }
    return std::string(homeDir) + "/.config/wakefield";
}

// Read device config
void readDeviceConfig(int& audioDeviceId, int& midiPort) {
    std::string configPath = getConfigDirectory() + "/device_config.txt";
    std::ifstream file(configPath);
    
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("audio_device=") == 0) {
                audioDeviceId = std::stoi(line.substr(13));
            } else if (line.find("midi_port=") == 0) {
                midiPort = std::stoi(line.substr(10));
            }
        }
        file.close();
    }
}

// Write device config
void writeDeviceConfig(int audioDeviceId, int midiPort) {
    std::string configDir = getConfigDirectory();
    mkdir(configDir.c_str(), 0755);
    
    std::string configPath = configDir + "/device_config.txt";
    std::ofstream file(configPath);
    
    if (file.is_open()) {
        file << "audio_device=" << audioDeviceId << "\n";
        file << "midi_port=" << midiPort << "\n";
        file.close();
    }
}

// Restart app with new devices
void restartWithNewDevices(int audioDeviceId, int midiPort, SynthParameters* params, char** argv) {
    // Save current state as temp preset
    PresetManager::savePreset("__temp_restart__", params);
    
    // Write new device config
    writeDeviceConfig(audioDeviceId, midiPort);
    
    // Restart the application
    execv(argv[0], argv);
    
    // If execv fails, we'll continue running
    std::cerr << "Failed to restart application\n";
}

// Helper function to map MIDI CC value (0-127) to parameter range
float mapCCToParameter(int ccValue, float minVal, float maxVal, bool logarithmic = false) {
    float normalized = ccValue / 127.0f;  // 0.0 to 1.0

    if (logarithmic) {
        // Logarithmic mapping (for frequency-like parameters)
        float logMin = std::log(minVal);
        float logMax = std::log(maxVal);
        return std::exp(logMin + normalized * (logMax - logMin));
    } else {
        // Linear mapping
        return minVal + normalized * (maxVal - minVal);
    }
}

// Helper function to apply MIDI CC to a parameter
void applyMIDICCToParameter(int paramId, int ccValue) {
    if (!synthParams) return;

    switch (paramId) {
        // Global parameters
        case 1:  // Waveform (ENUM 0-3)
            synthParams->waveform = static_cast<int>(mapCCToParameter(ccValue, 0, 3));
            break;
        case 2:  // Attack (exponential)
            {
                float mapped = mapCCToParameter(ccValue, 0.001f, 30.0f, true);
                synthParams->attack = mapped;
                synthParams->setEnvAttack(0, mapped);
            }
            break;
        case 3:  // Decay (exponential)
            {
                float mapped = mapCCToParameter(ccValue, 0.001f, 30.0f, true);
                synthParams->decay = mapped;
                synthParams->setEnvDecay(0, mapped);
            }
            break;
        case 4:  // Sustain (linear)
            {
                float mapped = mapCCToParameter(ccValue, 0.0f, 1.0f);
                synthParams->sustain = mapped;
                synthParams->setEnvSustain(0, mapped);
            }
            break;
        case 5:  // Release (exponential)
            {
                float mapped = mapCCToParameter(ccValue, 0.001f, 30.0f, true);
                synthParams->release = mapped;
                synthParams->setEnvRelease(0, mapped);
            }
            break;
        case 6:  // Master Volume (linear)
            synthParams->masterVolume = mapCCToParameter(ccValue, 0.0f, 1.0f);
            break;

        // OSCILLATOR page parameters (Oscillator 1 for now)
        case 10:  // Mode (ENUM 0-1)
            synthParams->osc1Mode = static_cast<int>(mapCCToParameter(ccValue, 0, 1));
            break;
        case 11:  // Frequency (logarithmic)
            synthParams->osc1Freq = mapCCToParameter(ccValue, 20.0f, 2000.0f, true);
            break;
        case 12:  // Morph (linear)
            synthParams->osc1Morph = mapCCToParameter(ccValue, 0.0001f, 0.9999f);
            break;
        case 13:  // Duty (linear)
            synthParams->osc1Duty = mapCCToParameter(ccValue, 0.0f, 1.0f);
            break;
        case 14:  // Ratio (logarithmic, 0.125-16.0)
            synthParams->osc1Ratio = mapCCToParameter(ccValue, 0.125f, 16.0f, true);
            break;
        case 15:  // Offset (linear, -1000 to 1000 Hz)
            synthParams->osc1Offset = mapCCToParameter(ccValue, -1000.0f, 1000.0f);
            break;
        // REVERB page parameters
        case 20:  // Reverb Type (ENUM 0-4)
            synthParams->reverbType = static_cast<int>(mapCCToParameter(ccValue, 0, 4));
            break;
        case 21:  // Reverb Enabled (BOOL)
            synthParams->reverbEnabled = (ccValue > 63);
            break;
        case 22:  // Delay Time (linear)
            synthParams->reverbDelayTime = mapCCToParameter(ccValue, 0.0f, 1.0f);
            break;
        case 23:  // Size (linear)
            synthParams->reverbSize = mapCCToParameter(ccValue, 0.0f, 1.0f);
            break;
        case 24:  // Damping (linear)
            synthParams->reverbDamping = mapCCToParameter(ccValue, 0.0f, 0.99f);
            break;
        case 25:  // Mix (linear)
            synthParams->reverbMix = mapCCToParameter(ccValue, 0.0f, 1.0f);
            break;
        case 26:  // Decay (linear)
            synthParams->reverbDecay = mapCCToParameter(ccValue, 0.0f, 1.0f);
            break;
        case 27:  // Diffusion (linear)
            synthParams->reverbDiffusion = mapCCToParameter(ccValue, 0.0f, 0.99f);
            break;
        case 28:  // Mod Depth (linear)
            synthParams->reverbModDepth = mapCCToParameter(ccValue, 0.0f, 1.0f);
            break;
        case 29:  // Mod Freq (linear)
            synthParams->reverbModFreq = mapCCToParameter(ccValue, 0.0f, 10.0f);
            break;

        // FILTER page parameters
        case 30:  // Filter Type (ENUM 0-3)
            synthParams->filterType = static_cast<int>(mapCCToParameter(ccValue, 0, 3));
            break;
        case 31:  // Filter Enabled (BOOL)
            synthParams->filterEnabled = (ccValue > 63);
            break;
        case 32:  // Cutoff (logarithmic)
            synthParams->filterCutoff = mapCCToParameter(ccValue, 20.0f, 20000.0f, true);
            break;
        case 33:  // Gain (linear)
            synthParams->filterGain = mapCCToParameter(ccValue, -24.0f, 24.0f);
            break;

        // LOOPER page parameters
        case 40:  // Current Loop (INT 0-3)
            synthParams->currentLoop = static_cast<int>(mapCCToParameter(ccValue, 0, 3));
            break;
        case 41:  // Overdub Mix (linear)
            synthParams->overdubMix = mapCCToParameter(ccValue, 0.0f, 1.0f);
            break;
    }
}

// Callbacks for MIDI events
void onNoteOn(int note, int velocity) {
    if (synth) {
        synth->noteOn(note, velocity);
    }
}

void onNoteOff(int note) {
    if (synth) {
        synth->noteOff(note);
    }
}

// Callback for MIDI CC messages
void onControlChange(int controller, int value) {
    if (!synthParams) return;
    
    // Check if we're in the new unified MIDI learn mode
    if (synthParams->midiLearnActive.load()) {
        int paramId = synthParams->midiLearnParameterId.load();

        // Learn CC for any parameter
        if (paramId >= 0 && paramId < 50) {
            synthParams->parameterCCMap[paramId] = controller;
            synthParams->midiLearnActive = false;
            synthParams->midiLearnParameterId = -1;

            // Also update legacy filterCutoffCC if it's the filter cutoff parameter
            if (paramId == 32) {
                synthParams->filterCutoffCC = controller;
            }

            if (ui) {
                std::string paramName = ui->getParameterName(paramId);
                ui->addConsoleMessage("Learned CC#" + std::to_string(controller) + " for " + paramName);
            }
            return;  // Exit early after learning
        }
    }
    
    // Legacy: Check if we're in CC learn mode (filter) - for backwards compatibility
    if (synthParams->ccLearnMode.load() && synthParams->ccLearnTarget.load() == 0) {
        // Learn this CC for filter cutoff
        synthParams->filterCutoffCC = controller;
        synthParams->ccLearnMode = false;
        synthParams->ccLearnTarget = -1;
        if (ui) {
            ui->addConsoleMessage("Learned CC#" + std::to_string(controller) + " for Filter Cutoff");
        }
    }
    
    // Check if we're in looper MIDI learn mode
    if (synthParams->loopMidiLearnMode.load()) {
        int target = synthParams->loopMidiLearnTarget.load();
        if (target == 0) {
            synthParams->loopRecPlayCC = controller;
            if (ui) ui->addConsoleMessage("Learned CC#" + std::to_string(controller) + " for Loop Rec/Play");
        } else if (target == 1) {
            synthParams->loopOverdubCC = controller;
            if (ui) ui->addConsoleMessage("Learned CC#" + std::to_string(controller) + " for Loop Overdub");
        } else if (target == 2) {
            synthParams->loopStopCC = controller;
            if (ui) ui->addConsoleMessage("Learned CC#" + std::to_string(controller) + " for Loop Stop");
        } else if (target == 3) {
            synthParams->loopClearCC = controller;
            if (ui) ui->addConsoleMessage("Learned CC#" + std::to_string(controller) + " for Loop Clear");
        }
        synthParams->loopMidiLearnMode = false;
        synthParams->loopMidiLearnTarget = -1;
    }
    
    // Process CC messages - check if any parameter is mapped to this controller
    // TODO: Add parameter smoothing at control rate (100Hz) to prevent zipper noise
    // Current implementation: Atomic writes provide thread-safe direct updates
    for (int paramId = 0; paramId < 50; ++paramId) {
        int mappedCC = synthParams->parameterCCMap[paramId].load();
        if (mappedCC >= 0 && mappedCC == controller) {
            applyMIDICCToParameter(paramId, value);
        }
    }

    // Legacy: Process CC messages if they're mapped to parameters
    int cutoffCC = synthParams->filterCutoffCC.load();
    if (cutoffCC >= 0 && controller == cutoffCC) {
        // Map MIDI CC value (0-127) to frequency range (20-20000 Hz) logarithmically
        // MIDI 0 = 20 Hz, MIDI 127 = 20000 Hz
        float normalized = value / 127.0f;  // 0.0 to 1.0
        // Logarithmic mapping: 20 Hz to 20 kHz
        float frequency = 20.0f * std::pow(1000.0f, normalized);  // 20 * (1000^norm)
        synthParams->filterCutoff = frequency;
    }
    
    // Process looper CC mappings (toggle on high values > 64)
    if (loopManager) {
        int recPlayCC = synthParams->loopRecPlayCC.load();
        if (recPlayCC >= 0 && controller == recPlayCC && value > 64) {
            Looper* loop = loopManager->getCurrentLoop();
            if (loop) loop->pressRecPlay();
        }
        
        int overdubCC = synthParams->loopOverdubCC.load();
        if (overdubCC >= 0 && controller == overdubCC && value > 64) {
            Looper* loop = loopManager->getCurrentLoop();
            if (loop) loop->pressOverdub();
        }
        
        int stopCC = synthParams->loopStopCC.load();
        if (stopCC >= 0 && controller == stopCC && value > 64) {
            Looper* loop = loopManager->getCurrentLoop();
            if (loop) loop->pressStop();
        }
        
        int clearCC = synthParams->loopClearCC.load();
        if (clearCC >= 0 && controller == clearCC && value > 64) {
            Looper* loop = loopManager->getCurrentLoop();
            if (loop) loop->pressClear();
        }
    }
}

// Audio callback function
int audioCallback(void* outputBuffer, void* /*inputBuffer*/,
                  unsigned int nFrames,
                  double /*streamTime*/,
                  RtAudioStreamStatus status,
                  void* /*userData*/) {

    // Temp buffers for synth output (before looper)
    static std::vector<float> tempBuffer;
    static std::vector<float> tempL;
    static std::vector<float> tempR;

    // Parameter smoothers (10ms smoothing time at 48kHz = ~100Hz update rate)
    static bool smoothersInitialized = false;
    static ParameterSmoother attackSmoother;
    static ParameterSmoother decaySmoother;
    static ParameterSmoother sustainSmoother;
    static ParameterSmoother releaseSmoother;
    static ParameterSmoother masterVolumeSmoother;
    static ParameterSmoother oscillatorFreqSmoother;
    static ParameterSmoother oscillatorMorphSmoother;
    static ParameterSmoother oscillatorDutySmoother;
    static ParameterSmoother reverbDelayTimeSmoother;
    static ParameterSmoother reverbSizeSmoother;
    static ParameterSmoother reverbDampingSmoother;
    static ParameterSmoother reverbMixSmoother;
    static ParameterSmoother reverbDecaySmoother;
    static ParameterSmoother reverbDiffusionSmoother;
    static ParameterSmoother reverbModDepthSmoother;
    static ParameterSmoother reverbModFreqSmoother;
    static ParameterSmoother filterCutoffSmoother;
    static ParameterSmoother filterGainSmoother;
    static ParameterSmoother overdubMixSmoother;

    float* buffer = static_cast<float*>(outputBuffer);

    if (status) {
        std::cout << "Stream underflow detected!" << std::endl;
    }

    // Process pending MIDI messages first
    if (midiHandler) {
        midiHandler->processMessages(onNoteOn, onNoteOff, onControlChange);
    }
    
    // Update synth parameters from UI (atomic reads are thread-safe)
    // Use smoothers to prevent zipper noise
    if (synth && synthParams) {
        // Initialize smoothers on first run
        if (!smoothersInitialized) {
            attackSmoother.reset(synthParams->attack.load());
            decaySmoother.reset(synthParams->decay.load());
            sustainSmoother.reset(synthParams->sustain.load());
            releaseSmoother.reset(synthParams->release.load());
            masterVolumeSmoother.reset(synthParams->masterVolume.load());
            oscillatorFreqSmoother.reset(synthParams->osc1Freq.load());
            oscillatorMorphSmoother.reset(synthParams->osc1Morph.load());
            oscillatorDutySmoother.reset(synthParams->osc1Duty.load());
            reverbDelayTimeSmoother.reset(synthParams->reverbDelayTime.load());
            reverbSizeSmoother.reset(synthParams->reverbSize.load());
            reverbDampingSmoother.reset(synthParams->reverbDamping.load());
            reverbMixSmoother.reset(synthParams->reverbMix.load());
            reverbDecaySmoother.reset(synthParams->reverbDecay.load());
            reverbDiffusionSmoother.reset(synthParams->reverbDiffusion.load());
            reverbModDepthSmoother.reset(synthParams->reverbModDepth.load());
            reverbModFreqSmoother.reset(synthParams->reverbModFreq.load());
            filterCutoffSmoother.reset(synthParams->filterCutoff.load());
            filterGainSmoother.reset(synthParams->filterGain.load());
            overdubMixSmoother.reset(synthParams->overdubMix.load());
            smoothersInitialized = true;
        }

        // Update smoother targets from atomic parameters
        attackSmoother.setTarget(synthParams->attack.load());
        decaySmoother.setTarget(synthParams->decay.load());
        sustainSmoother.setTarget(synthParams->sustain.load());
        releaseSmoother.setTarget(synthParams->release.load());
        masterVolumeSmoother.setTarget(synthParams->masterVolume.load());
        oscillatorFreqSmoother.setTarget(synthParams->osc1Freq.load());
        oscillatorMorphSmoother.setTarget(synthParams->osc1Morph.load());
        oscillatorDutySmoother.setTarget(synthParams->osc1Duty.load());
        reverbDelayTimeSmoother.setTarget(synthParams->reverbDelayTime.load());
        reverbSizeSmoother.setTarget(synthParams->reverbSize.load());
        reverbDampingSmoother.setTarget(synthParams->reverbDamping.load());
        reverbMixSmoother.setTarget(synthParams->reverbMix.load());
        reverbDecaySmoother.setTarget(synthParams->reverbDecay.load());
        reverbDiffusionSmoother.setTarget(synthParams->reverbDiffusion.load());
        reverbModDepthSmoother.setTarget(synthParams->reverbModDepth.load());
        reverbModFreqSmoother.setTarget(synthParams->reverbModFreq.load());
        filterCutoffSmoother.setTarget(synthParams->filterCutoff.load());
        filterGainSmoother.setTarget(synthParams->filterGain.load());
        overdubMixSmoother.setTarget(synthParams->overdubMix.load());

        // Process smoothers (one step per audio callback)
        float smoothedAttack = attackSmoother.process();
        float smoothedDecay = decaySmoother.process();
        float smoothedSustain = sustainSmoother.process();
        float smoothedRelease = releaseSmoother.process();
        float smoothedMasterVolume = masterVolumeSmoother.process();
        float smoothedOscillatorFreq = oscillatorFreqSmoother.process();
        float smoothedOscillatorMorph = oscillatorMorphSmoother.process();
        float smoothedOscillatorDuty = oscillatorDutySmoother.process();
        float smoothedReverbDelayTime = reverbDelayTimeSmoother.process();
        float smoothedReverbSize = reverbSizeSmoother.process();
        float smoothedReverbDamping = reverbDampingSmoother.process();
        float smoothedReverbMix = reverbMixSmoother.process();
        float smoothedReverbDecay = reverbDecaySmoother.process();
        float smoothedReverbDiffusion = reverbDiffusionSmoother.process();
        float smoothedReverbModDepth = reverbModDepthSmoother.process();
        float smoothedReverbModFreq = reverbModFreqSmoother.process();
        float smoothedFilterCutoff = filterCutoffSmoother.process();
        float smoothedFilterGain = filterGainSmoother.process();

        // Update synth with smoothed values
        synth->updateEnvelopeParameters(
            smoothedAttack,
            smoothedDecay,
            smoothedSustain,
            smoothedRelease
        );
        synth->setMasterVolume(smoothedMasterVolume);

        // Update per-oscillator parameters (oscillator 1 uses smoothed values)
        for (int oscIndex = 0; oscIndex < OSCILLATORS_PER_VOICE; ++oscIndex) {
            BrainwaveMode mode = static_cast<BrainwaveMode>(synthParams->getOscMode(oscIndex));
            int shape = synthParams->getOscShape(oscIndex);
            float baseFreq = (oscIndex == 0) ? smoothedOscillatorFreq : synthParams->getOscFrequency(oscIndex);
            float morph = (oscIndex == 0) ? smoothedOscillatorMorph : synthParams->getOscMorph(oscIndex);
            float duty = (oscIndex == 0) ? smoothedOscillatorDuty : synthParams->getOscDuty(oscIndex);
            float ratio = synthParams->getOscRatio(oscIndex);
            float offset = synthParams->getOscOffset(oscIndex);
            float amp = synthParams->getOscAmp(oscIndex);
            float level = synthParams->getOscLevel(oscIndex);

            synth->setOscillatorState(
                oscIndex,
                mode,
                shape,
                baseFreq,
                morph,
                duty,
                ratio,
                offset,
                amp,
                level
            );
        }

        // Update reverb parameters (discrete params not smoothed)
        synth->setReverbEnabled(synthParams->reverbEnabled.load());
        synth->updateReverbParameters(
            smoothedReverbDelayTime,
            smoothedReverbSize,
            smoothedReverbDamping,
            smoothedReverbMix,
            smoothedReverbDecay,
            smoothedReverbDiffusion,
            smoothedReverbModDepth,
            smoothedReverbModFreq
        );

        // Update filter parameters (discrete params not smoothed)
        synth->setFilterEnabled(synthParams->filterEnabled.load());
        synth->updateFilterParameters(
            synthParams->filterType.load(),
            smoothedFilterCutoff,
            smoothedFilterGain
        );

        // Update LFO parameters
        // Get tempo from sequencer for LFO sync
        float currentTempo = 120.0f;  // Default tempo
        if (sequencer) {
            currentTempo = sequencer->getTempo();
        }

        for (int i = 0; i < 4; ++i) {
            synth->updateLFOParameters(
                i,
                synthParams->getLfoPeriod(i),
                synthParams->getLfoSyncMode(i),
                synthParams->getLfoShape(i),
                synthParams->getLfoMorph(i),
                synthParams->getLfoDuty(i),
                synthParams->getLfoFlip(i),
                synthParams->getLfoResetOnNote(i),
                currentTempo
            );
        }
    }

    // Update looper parameters
    if (loopManager && synthParams) {
        loopManager->selectLoop(synthParams->currentLoop.load());
        float smoothedOverdubMix = overdubMixSmoother.process();
        loopManager->setOverdubMix(smoothedOverdubMix);
    }

    // Process sequencer (triggers notes based on patterns)
    if (sequencer) {
        sequencer->process(nFrames);
    }

    // Process LFOs (once per buffer, before synthesis)
    // Note: Sample rate is passed as 48000.0f (hardcoded for now)
    if (synth) {
        synth->processLFOs(48000.0f, nFrames);
    }

    // Generate audio from synth (with effects) into temp buffer
    if (synth && loopManager) {
        // Resize temp buffers if needed
        size_t stereoFrames = nFrames * 2;
        if (tempBuffer.size() < stereoFrames) {
            tempBuffer.resize(stereoFrames);
            tempL.resize(nFrames);
            tempR.resize(nFrames);
        }

        // Process synth into temp buffer
        synth->process(tempBuffer.data(), nFrames, 2);
        
        // Deinterleave for looper processing
        for (unsigned int i = 0; i < nFrames; ++i) {
            tempL[i] = tempBuffer[i * 2];
            tempR[i] = tempBuffer[i * 2 + 1];
        }
        
        // Process through loopers (post-effects)
        std::vector<float> outL(nFrames);
        std::vector<float> outR(nFrames);
        loopManager->processBlock(tempL.data(), tempR.data(), outL.data(), outR.data(), nFrames);
        
        // Interleave output
        for (unsigned int i = 0; i < nFrames; ++i) {
            buffer[i * 2] = outL[i];
            buffer[i * 2 + 1] = outR[i];
        }
    } else if (synth) {
        // No looper, just process synth directly
        synth->process(buffer, nFrames, 2);
    }
    
    return 0;
}

int main(int argc, char** argv) {
    setlocale(LC_ALL, "");
    // Set up signal handler for Ctrl+C
    signal(SIGINT, signalHandler);
    
    // Create synth parameters
    synthParams = new SynthParameters();
    
    // Read device preferences
    int preferredAudioDevice = -1;
    int preferredMidiPort = -1;
    readDeviceConfig(preferredAudioDevice, preferredMidiPort);
    
    // Initialize MIDI
    midiHandler = new MidiHandler();
    if (!midiHandler->initialize()) {
        std::cerr << "Failed to initialize MIDI\n";
        delete midiHandler;
        delete synthParams;
        return 1;
    }
    
    // Get list of available MIDI devices
    std::vector<std::pair<int, std::string>> midiDevices;
    int portCount = midiHandler->getPortCount();
    for (int i = 0; i < portCount; ++i) {
        std::string portName = midiHandler->getPortName(i);
        midiDevices.push_back({i, portName});
    }
    
    // Open MIDI port (use preference or find Arturia)
    int midiPortToUse = preferredMidiPort;
    if (midiPortToUse < 0 || midiPortToUse >= portCount) {
        // Try to find Arturia keyboard
        int arturiaPort = midiHandler->findPortByName("arturia");
        if (arturiaPort >= 0) {
            std::cout << "Found Arturia keyboard at port " << arturiaPort << "\n";
            midiPortToUse = arturiaPort;
        } else if (portCount > 0) {
            std::cout << "Using first available MIDI port...\n";
            midiPortToUse = 0;
        }
    }
    
    if (midiPortToUse >= 0) {
        midiHandler->openPort(midiPortToUse);
    }
    
    // Initialize Audio
    RtAudio audio;
    bool audioAvailable = false;
    
    unsigned int sampleRate = 48000;
    unsigned int bufferFrames = 256;
    
    // Create synth instance
    synth = new Synth(static_cast<float>(sampleRate));

    // Load samples from ../samples directory (relative to project root)
    std::cout << "Loading samples from ../samples..." << std::endl;
    int samplesLoaded = synth->getSampleBank()->loadSamplesFromDirectory("../samples");
    if (samplesLoaded > 0) {
        std::cout << "Loaded " << samplesLoaded << " samples successfully" << std::endl;
        // Load first sample into first sampler
        synth->setSamplerSample(0, 0);
        std::cout << "Loaded first sample into Sampler 1" << std::endl;
    } else {
        std::cout << "Warning: No samples found in ../samples directory" << std::endl;
    }

    // Create looper manager
    loopManager = new LoopManager(static_cast<float>(sampleRate));

    // Create sequencer
    sequencer = new Sequencer(static_cast<float>(sampleRate), synth);

    // Initialize UI first (before audio)
    ui = new UI(synth, synthParams);
    if (!ui->initialize()) {
        std::cerr << "Failed to initialize UI\n";
        delete ui;
        delete synth;
        delete loopManager;
        delete midiHandler;
        delete synthParams;
        return 1;
    }
    
    // Link synth to UI for oscilloscope
    synth->setUI(ui);

    // Link synth to parameters for FM matrix access
    synth->setParams(synthParams);

    // Set UI pointer for MIDI error messages
    midiHandler->setUI(ui);
    
    // Get list of available audio devices
    std::vector<std::pair<int, std::string>> audioDevices;
    unsigned int deviceCount = audio.getDeviceCount();
    int audioDeviceIdToUse = -1;
    
    for (unsigned int i = 0; i < deviceCount; ++i) {
        try {
            RtAudio::DeviceInfo info = audio.getDeviceInfo(i);
            if (info.outputChannels > 0) {  // Only list output devices
                audioDevices.push_back({static_cast<int>(i), info.name});
            }
        } catch (...) {
            continue;
        }
    }
    
    // Determine which audio device to use
    if (preferredAudioDevice >= 0 && preferredAudioDevice < static_cast<int>(deviceCount)) {
        audioDeviceIdToUse = preferredAudioDevice;
        std::cout << "Using preferred audio device: " << preferredAudioDevice << "\n";
    } else {
        audioDeviceIdToUse = audio.getDefaultOutputDevice();
        std::cout << "Using default audio device: " << audioDeviceIdToUse << "\n";
    }
    
    // Try to initialize audio
    std::string audioDeviceName = "No Audio Device";
    
    if (deviceCount > 0) {
        // Set up stream parameters
        RtAudio::StreamParameters parameters;
        parameters.deviceId = audioDeviceIdToUse;
        parameters.nChannels = 2;  // Stereo
        parameters.firstChannel = 0;
        
        try {
            audio.openStream(&parameters, nullptr, RTAUDIO_FLOAT32,
                            sampleRate, &bufferFrames, &audioCallback);
            
            audio.startStream();
            audioAvailable = true;
            
            // Get device name
            try {
                RtAudio::DeviceInfo deviceInfo = audio.getDeviceInfo(parameters.deviceId);
                audioDeviceName = deviceInfo.name;
            } catch (...) {
                audioDeviceName = "Default Audio Device";
            }
            
            ui->addConsoleMessage("Audio initialized: " + audioDeviceName);
            
        } catch (std::exception& e) {
            std::cerr << "Audio error: " << e.what() << '\n';
            ui->addConsoleMessage("WARNING: Audio failed - running without audio");
            audioAvailable = false;
        }
    } else {
        ui->addConsoleMessage("WARNING: No audio devices found - running without audio");
    }
    
    // Set device information and available devices
    std::string midiDeviceName = midiHandler->getCurrentPortName();
    int midiPort = midiHandler->getCurrentPortNumber();
    ui->setDeviceInfo(audioDeviceName, sampleRate, bufferFrames, midiDeviceName, midiPort);
    ui->setAvailableAudioDevices(audioDevices, audioDeviceIdToUse);
    ui->setAvailableMidiDevices(midiDevices, midiPortToUse);
    
    // Load temp preset if it exists (from previous restart)
    std::string tempPresetPath = PresetManager::getPresetPath("__temp_restart__");
    std::ifstream tempCheck(tempPresetPath);
    if (tempCheck.good()) {
        tempCheck.close();
        PresetManager::loadPreset("__temp_restart__", synthParams);
        ui->addConsoleMessage("Restored previous state");
        // Delete temp preset after loading
        unlink(tempPresetPath.c_str());
    }
    
    // Main UI loop
    float deltaTime = 0.05f;  // 50ms default (20 FPS)
    while (running) {
        // Update UI and handle input
        if (!ui->update()) {
            running = false;  // User pressed 'q'
            break;
        }
        
        // Check for device change request
        if (ui->isDeviceChangeRequested()) {
            int newAudioDevice = ui->getRequestedAudioDevice();
            int newMidiPort = ui->getRequestedMidiDevice();
            
            ui->addConsoleMessage("Restarting with new devices...");
            ui->draw(synth->getActiveVoiceCount());
            refresh();
            usleep(500000);  // Show message for 0.5 seconds
            
            // Clean shutdown before restart
            if (audioAvailable) {
                if (audio.isStreamRunning()) {
                    audio.stopStream();
                }
                if (audio.isStreamOpen()) {
                    audio.closeStream();
                }
            }
            
            // Restart with new devices
            restartWithNewDevices(newAudioDevice, newMidiPort, synthParams, argv);
            
            // If restart failed, continue running
            ui->clearDeviceChangeRequest();
            ui->addConsoleMessage("Restart failed, continuing with current devices");
        }
        
        // Oscilloscopes removed for simplified UI
        
        // Draw UI
        int activeVoices = synth->getActiveVoiceCount();
        ui->draw(activeVoices);
        
        // Sleep to control refresh rate (~20 FPS)
        usleep(50000);  // 50ms
    }
    
    // Clean shutdown
    if (audioAvailable) {
        if (audio.isStreamRunning()) {
            audio.stopStream();
        }
        if (audio.isStreamOpen()) {
            audio.closeStream();
        }
    }

    // Nullify pointers before cleanup to prevent dangling pointer access
    synth->setUI(nullptr);
    if (midiHandler) {
        midiHandler->setUI(nullptr);
    }

    // Clean up
    delete ui;
    delete sequencer;
    delete synth;
    delete loopManager;
    delete midiHandler;
    delete synthParams;

    return 0;
}
