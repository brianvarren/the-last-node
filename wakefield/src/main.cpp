#define RTAUDIO_EXCEPTIONS
#include <RtAudio.h>
#include <iostream>
#include <fstream>
#include <csignal>
#include <unistd.h>
#include <cmath>
#include <sys/stat.h>
#include <pwd.h>
#include "synth.h"
#include "midi.h"
#include "ui.h"
#include "preset.h"

// Global instances
static Synth* synth = nullptr;
static MidiHandler* midiHandler = nullptr;
static SynthParameters* synthParams = nullptr;
static UI* ui = nullptr;
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
    
    // Check if we're in CC learn mode
    if (synthParams->ccLearnMode.load() && synthParams->ccLearnTarget.load() == 0) {
        // Learn this CC for filter cutoff
        synthParams->filterCutoffCC = controller;
        synthParams->ccLearnMode = false;
        synthParams->ccLearnTarget = -1;
        if (ui) {
            ui->addConsoleMessage("Learned CC#" + std::to_string(controller) + " for Filter Cutoff");
        }
    }
    
    // Process CC messages if they're mapped to parameters
    int cutoffCC = synthParams->filterCutoffCC.load();
    if (cutoffCC >= 0 && controller == cutoffCC) {
        // Map MIDI CC value (0-127) to frequency range (20-20000 Hz) logarithmically
        // MIDI 0 = 20 Hz, MIDI 127 = 20000 Hz
        float normalized = value / 127.0f;  // 0.0 to 1.0
        // Logarithmic mapping: 20 Hz to 20 kHz
        float frequency = 20.0f * std::pow(1000.0f, normalized);  // 20 * (1000^norm)
        synthParams->filterCutoff = frequency;
    }
}

// Audio callback function
int audioCallback(void* outputBuffer, void* /*inputBuffer*/, 
                  unsigned int nFrames,
                  double /*streamTime*/, 
                  RtAudioStreamStatus status, 
                  void* /*userData*/) {
    
    float* buffer = static_cast<float*>(outputBuffer);
    
    if (status) {
        std::cout << "Stream underflow detected!" << std::endl;
    }
    
    // Process pending MIDI messages first
    if (midiHandler) {
        midiHandler->processMessages(onNoteOn, onNoteOff, onControlChange);
    }
    
    // Update synth parameters from UI (atomic reads are thread-safe)
    if (synth && synthParams) {
        synth->updateEnvelopeParameters(
            synthParams->attack.load(),
            synthParams->decay.load(),
            synthParams->sustain.load(),
            synthParams->release.load()
        );
        synth->setMasterVolume(synthParams->masterVolume.load());
        synth->setWaveform(static_cast<Waveform>(synthParams->waveform.load()));
        
        // Update reverb parameters
        synth->setReverbEnabled(synthParams->reverbEnabled.load());
        synth->updateReverbParameters(
            synthParams->reverbDelayTime.load(),
            synthParams->reverbSize.load(),
            synthParams->reverbDamping.load(),
            synthParams->reverbMix.load(),
            synthParams->reverbDecay.load(),
            synthParams->reverbDiffusion.load(),
            synthParams->reverbModDepth.load(),
            synthParams->reverbModFreq.load()
        );
        
        // Update filter parameters
        synth->setFilterEnabled(synthParams->filterEnabled.load());
        synth->updateFilterParameters(
            synthParams->filterType.load(),
            synthParams->filterCutoff.load(),
            synthParams->filterGain.load()
        );
    }
    
    // Generate audio
    if (synth) {
        synth->process(buffer, nFrames, 2);  // 2 channels for stereo
    }
    
    return 0;
}

int main(int argc, char** argv) {
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
    
    // Clean up
    delete ui;
    delete synth;
    delete midiHandler;
    delete synthParams;
    
    return 0;
}