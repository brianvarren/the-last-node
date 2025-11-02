#define RTAUDIO_EXCEPTIONS
#include <RtAudio.h>
#include <iostream>
#include <fstream>
#include <csignal>
#include <unistd.h>
#include <locale.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <sys/stat.h>
#include <pwd.h>

// SSE intrinsics for denormal prevention
#ifdef __SSE__
#include <xmmintrin.h>
#include <pmmintrin.h>
#endif
#include "synth.h"
#include "midi.h"
#include "ui.h"
#include "preset.h"
#include "parameter_smoother.h"
#include "sequencer.h"
#include "clock.h"

// Global instances
static Synth* synth = nullptr;
static MidiHandler* midiHandler = nullptr;
static SynthParameters* synthParams = nullptr;
static UI* ui = nullptr;
Sequencer* sequencer = nullptr;  // Non-static so UI can access it
static Clock* transportClock = nullptr;
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
void readDeviceConfig(int& audioDeviceId, int& midiPort, unsigned int& bufferSize) {
    std::string configPath = getConfigDirectory() + "/device_config.txt";
    std::ifstream file(configPath);
    
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("audio_device=") == 0) {
                audioDeviceId = std::stoi(line.substr(13));
            } else if (line.find("midi_port=") == 0) {
                midiPort = std::stoi(line.substr(10));
            } else if (line.find("buffer_size=") == 0) {
                bufferSize = static_cast<unsigned int>(std::stoi(line.substr(12)));
            }
        }
        file.close();
    }
}

// Write device config
void writeDeviceConfig(int audioDeviceId, int midiPort, unsigned int bufferSize) {
    std::string configDir = getConfigDirectory();
    mkdir(configDir.c_str(), 0755);
    
    std::string configPath = configDir + "/device_config.txt";
    std::ofstream file(configPath);
    
    if (file.is_open()) {
        file << "audio_device=" << audioDeviceId << "\n";
        file << "midi_port=" << midiPort << "\n";
        file << "buffer_size=" << bufferSize << "\n";
        file.close();
    }
}

// Restart app with new devices
void restartWithNewDevices(int audioDeviceId, int midiPort, unsigned int bufferSize,
                           SynthParameters* params, char** argv) {
    // Save current state as temp preset
    PresetManager::savePreset("__temp_restart__", params);
    
    // Write new device config
    writeDeviceConfig(audioDeviceId, midiPort, bufferSize);
    
    // Restart the application
    execv(argv[0], argv);
    
    // If execv fails, we'll continue running
    std::cerr << "Failed to restart application\n";
}

// Helper function to apply MIDI CC to a parameter
void applyMIDICCToParameter(int paramId, int ccValue) {
    if (!synthParams) return;

    float normalized = std::clamp(static_cast<float>(ccValue) / 127.0f, 0.0f, 1.0f);

    const InlineParameter* parameter = ParameterRegistry::Lookup(paramId);
    if (!parameter) {
        return;
    }

    if (paramId == 400) {
        return;
    }

    float value = parameter->denormalize(normalized);

    int oscIndex = synthParams->parameterContextOsc[paramId].load();
    int lfoIndex = synthParams->parameterContextLFO[paramId].load();
    int envIndex = synthParams->parameterContextEnv[paramId].load();
    int samplerIndex = synthParams->parameterContextSampler[paramId].load();
    int chaosIndex = synthParams->parameterContextChaos[paramId].load();

    synthParams->applyParameterValue(paramId,
                                     value,
                                     synth,
                                     oscIndex,
                                     lfoIndex,
                                     envIndex,
                                     samplerIndex,
                                     chaosIndex);
}

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

void onControlChange(int controller, int value) {
    if (!synthParams) return;

    if (synthParams->midiLearnActive.load()) {
        int paramId = synthParams->midiLearnParameterId.load();

        if (paramId >= 0 && paramId < SynthParameters::kMaxParamMap) {
            synthParams->parameterCCMap[paramId] = controller;
            synthParams->midiLearnActive = false;
            synthParams->midiLearnParameterId = -1;

            if (paramId == 32) {
                synthParams->filterCutoffCC = controller;
            }

            if (ui) {
                std::string paramName = ui->getParameterName(paramId);
                ui->addConsoleMessage("Learned CC#" + std::to_string(controller) + " for param ID " +
                                      std::to_string(paramId) + " (" + paramName + ")");
            }
            return;
        }
    }

    if (synthParams->ccLearnMode.load() && synthParams->ccLearnTarget.load() == 0) {
        synthParams->filterCutoffCC = controller;
        synthParams->ccLearnMode = false;
        synthParams->ccLearnTarget = -1;
        if (ui) {
            ui->addConsoleMessage("Learned CC#" + std::to_string(controller) + " for Filter Cutoff");
        }
    }

    for (int paramId = 0; paramId < SynthParameters::kMaxParamMap; ++paramId) {
        int mappedCC = synthParams->parameterCCMap[paramId].load();
        if (mappedCC >= 0 && mappedCC == controller) {
            if (ui) {
                ui->addConsoleMessage("Applying CC#" + std::to_string(controller) + " value=" +
                                      std::to_string(value) + " to param ID " + std::to_string(paramId));
            }
            applyMIDICCToParameter(paramId, value);
        }
    }

    int cutoffCC = synthParams->filterCutoffCC.load();
    if (cutoffCC >= 0 && controller == cutoffCC) {
        applyMIDICCToParameter(32, value);
    }

}

// Audio callback function
int audioCallback(void* outputBuffer, void* /*inputBuffer*/,
                  unsigned int nFrames,
                  double /*streamTime*/,
                  RtAudioStreamStatus status,
                  void* /*userData*/) {

    // Enable flush-to-zero and denormals-are-zero to prevent CPU slowdown
    // Denormal numbers (very small floats) can cause 100x CPU slowdown!
    #ifdef __SSE__
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
    #endif

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
    static ParameterSmoother filterResonanceSmoother;
    static ParameterSmoother filterDriveSmoother;
    static ParameterSmoother filterFeedbackHPSmoother;
    static ParameterSmoother filterSpreadSmoother;
    static ParameterSmoother filterNotchFeedbackSmoother;
    static ParameterSmoother filterWidthSmoother;

    float* buffer = static_cast<float*>(outputBuffer);

    // Detect buffer underruns (audio glitching/crackling)
    if (status) {
        static int underrunCount = 0;
        underrunCount++;
        if (underrunCount % 10 == 1) {  // Print every 10th underrun to avoid spam
            std::cerr << "⚠️  BUFFER UNDERRUN #" << underrunCount
                      << " - Audio thread can't keep up!" << std::endl;
        }
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
            filterResonanceSmoother.reset(synthParams->filterResonance.load());
            filterDriveSmoother.reset(synthParams->filterDrive.load());
            filterFeedbackHPSmoother.reset(synthParams->filterFeedbackHP.load());
            filterSpreadSmoother.reset(synthParams->filterSpread.load());
            filterNotchFeedbackSmoother.reset(synthParams->filterNotchFeedback.load());
            filterWidthSmoother.reset(synthParams->filterBandWidth.load());
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
        filterResonanceSmoother.setTarget(synthParams->filterResonance.load());
        filterDriveSmoother.setTarget(synthParams->filterDrive.load());
        filterFeedbackHPSmoother.setTarget(synthParams->filterFeedbackHP.load());
        filterSpreadSmoother.setTarget(synthParams->filterSpread.load());
        filterNotchFeedbackSmoother.setTarget(synthParams->filterNotchFeedback.load());
        filterWidthSmoother.setTarget(synthParams->filterBandWidth.load());
        filterSpreadSmoother.setTarget(synthParams->filterSpread.load());
        filterNotchFeedbackSmoother.setTarget(synthParams->filterNotchFeedback.load());

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
        float smoothedFilterResonance = filterResonanceSmoother.process();
        float smoothedFilterDrive = filterDriveSmoother.process();
        float smoothedFilterFeedbackHP = filterFeedbackHPSmoother.process();
        float smoothedFilterSpread = filterSpreadSmoother.process();
        float smoothedFilterNotchFeedback = filterNotchFeedbackSmoother.process();
        float smoothedFilterWidth = filterWidthSmoother.process();

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
            smoothedFilterGain,
            smoothedFilterResonance,
            smoothedFilterDrive,
            smoothedFilterFeedbackHP,
            smoothedFilterSpread,
            smoothedFilterNotchFeedback,
            smoothedFilterWidth
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

    // Process sequencer (triggers notes based on patterns)
    if (sequencer) {
        sequencer->process(nFrames);
    }

    // Process LFOs (once per buffer, before synthesis)
    // Note: Sample rate is passed as 48000.0f (hardcoded for now)
    if (synth) {
        synth->processLFOs(48000.0f, nFrames);
        synth->processChaos(nFrames);
    }

    // Generate audio from synth
    if (synth) {
        synth->process(buffer, nFrames, 2);
    } else {
        std::fill(buffer, buffer + nFrames * 2, 0.0f);
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
    unsigned int preferredBufferSize = 256;
    readDeviceConfig(preferredAudioDevice, preferredMidiPort, preferredBufferSize);
    
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
    unsigned int bufferFrames = preferredBufferSize > 0 ? preferredBufferSize : 256;
    
    // Create synth instance
    synth = new Synth(static_cast<float>(sampleRate));

    // Load samples from ../samples directory (relative to project root)
    auto resolveSampleDirectory = []() -> std::string {
        std::vector<std::string> candidates;
        if (const char* envPath = std::getenv("WAKEFIELD_SAMPLES")) {
            candidates.emplace_back(envPath);
        }
        candidates.emplace_back("samples");
        candidates.emplace_back("build/samples");
        candidates.emplace_back("../samples");
        candidates.emplace_back("../build/samples");
        candidates.emplace_back("../../samples");

        for (const auto& candidate : candidates) {
            try {
                std::filesystem::path path(candidate);
                if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
                    return std::filesystem::canonical(path).string();
                }
            } catch (...) {
                continue;
            }
        }
        return {};
    };

    const std::string samplePath = resolveSampleDirectory();
    int samplesLoaded = 0;
    if (!samplePath.empty()) {
        samplesLoaded = synth->getSampleBank()->loadSamplesFromDirectory(samplePath.c_str());
    }

    if (samplesLoaded > 0) {
        std::cout << "Loaded " << samplesLoaded << " samples from " << samplePath << std::endl;
        // Load first sample into first sampler
        synth->setSamplerSample(0, 0);
        std::cout << "Loaded first sample into Sampler 1" << std::endl;
        if (ui) {
            ui->setSampleDirectory(samplePath);
        }
    } else {
        std::cout << "Warning: No samples found in default locations (samples, build/samples, ../samples)" << std::endl;
    }

    // Create shared transport clock
    transportClock = new Clock(static_cast<float>(sampleRate));

    // Provide clock to synth and sequencer
    synth->setClock(transportClock);

    // Create sequencer
    sequencer = new Sequencer(transportClock, synth);

    // Initialize UI first (before audio)
    ui = new UI(synth, synthParams);
    if (!ui->initialize()) {
        std::cerr << "Failed to initialize UI\n";
        delete ui;
        delete synth;
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
    
    RtAudio::StreamParameters parameters;
    parameters.deviceId = audioDeviceIdToUse;
    parameters.nChannels = 2;  // Stereo
    parameters.firstChannel = 0;

    if (deviceCount > 0 && parameters.deviceId >= 0) {
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
            restartWithNewDevices(newAudioDevice, newMidiPort, bufferFrames, synthParams, argv);
            
            // If restart failed, continue running
            ui->clearDeviceChangeRequest();
            ui->addConsoleMessage("Restart failed, continuing with current devices");
        }

        if (ui->isBufferSizeChangeRequested()) {
            unsigned int requestedBuffer = static_cast<unsigned int>(ui->getRequestedBufferSize());

            if (!(deviceCount > 0 && parameters.deviceId >= 0)) {
                ui->addConsoleMessage("No audio device available to change buffer size");
                ui->clearBufferSizeChangeRequest();
            } else if (requestedBuffer == 0) {
                ui->addConsoleMessage("Invalid audio buffer size request");
                ui->clearBufferSizeChangeRequest();
            } else {
                unsigned int previousBuffer = bufferFrames;
                std::string failureReason;
                bool reopened = false;

                try {
                    if (audio.isStreamRunning()) {
                        audio.stopStream();
                    }
                    if (audio.isStreamOpen()) {
                        audio.closeStream();
                    }
                } catch (...) {
                    // Ignore exceptions while shutting down stream
                }

                audioAvailable = false;
                if (synth) {
                    synth->resetAudioState();
                }
                if (transportClock) {
                    transportClock->reset();
                }

                unsigned int openFrames = requestedBuffer;
                try {
                    audio.openStream(&parameters, nullptr, RTAUDIO_FLOAT32,
                                     sampleRate, &openFrames, &audioCallback);
                    audio.startStream();
                    bufferFrames = openFrames;
                    audioAvailable = true;
                    reopened = true;
                } catch (std::exception& e) {
                    failureReason = e.what();
                } catch (...) {
                    failureReason = "unknown error";
                }

                if (!reopened) {
                    // Attempt to restore previous buffer size
                    try {
                        unsigned int restoreFrames = previousBuffer;
                        audio.openStream(&parameters, nullptr, RTAUDIO_FLOAT32,
                                         sampleRate, &restoreFrames, &audioCallback);
                        audio.startStream();
                        bufferFrames = restoreFrames;
                        audioAvailable = true;
                    } catch (...) {
                        audioAvailable = false;
                    }
                    if (failureReason.empty()) {
                        failureReason = "unable to open stream";
                    }
                    ui->addConsoleMessage("Failed to set audio buffer size: " + failureReason);
                    if (audioAvailable) {
                        try {
                            RtAudio::DeviceInfo deviceInfo = audio.getDeviceInfo(parameters.deviceId);
                            audioDeviceName = deviceInfo.name;
                        } catch (...) {
                            // Keep previous name if query fails
                        }
                        ui->setDeviceInfo(audioDeviceName, sampleRate, bufferFrames, midiDeviceName, midiPortToUse);
                    }
                } else {
                    try {
                        RtAudio::DeviceInfo deviceInfo = audio.getDeviceInfo(parameters.deviceId);
                        audioDeviceName = deviceInfo.name;
                    } catch (...) {
                        // Keep previous name if query fails
                    }
                    ui->setDeviceInfo(audioDeviceName, sampleRate, bufferFrames, midiDeviceName, midiPortToUse);
                    writeDeviceConfig(ui->getCurrentAudioDeviceId(), ui->getCurrentMidiPort(), bufferFrames);
                    ui->addConsoleMessage("Audio buffer size set to " + std::to_string(bufferFrames) + " samples");
                }

                ui->clearBufferSizeChangeRequest();
            }
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
    delete midiHandler;
    delete synthParams;

    return 0;
}
