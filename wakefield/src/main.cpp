#define RTAUDIO_EXCEPTIONS
#include <RtAudio.h>
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <cmath>
#include "synth.h"
#include "midi.h"
#include "ui.h"

// Global instances
static Synth* synth = nullptr;
static MidiHandler* midiHandler = nullptr;
static SynthParameters* synthParams = nullptr;
static bool running = true;

void signalHandler(int signum) {
    running = false;
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
        std::cout << "Learned CC#" << controller << " for Filter Cutoff\n";
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

int main() {
    // Set up signal handler for Ctrl+C
    signal(SIGINT, signalHandler);
    
    // Create synth parameters
    synthParams = new SynthParameters();
    
    // Initialize MIDI
    midiHandler = new MidiHandler();
    if (!midiHandler->initialize()) {
        std::cerr << "Failed to initialize MIDI\n";
        delete midiHandler;
        delete synthParams;
        return 1;
    }
    
    // List and open MIDI ports
    midiHandler->listPorts();
    
    // Try to find Arturia keyboard
    int arturiaPort = midiHandler->findPortByName("arturia");
    if (arturiaPort >= 0) {
        std::cout << "Found Arturia keyboard at port " << arturiaPort << "\n";
        midiHandler->openPort(arturiaPort);
    } else {
        std::cout << "Arturia not found, trying first available port...\n";
        midiHandler->openPort(0);
    }
    
    // Initialize Audio
    RtAudio audio;
    
    unsigned int devices = audio.getDeviceCount();
    if (devices == 0) {
        std::cerr << "No audio devices found!\n";
        delete midiHandler;
        delete synthParams;
        return 1;
    }
    
    // Set up stream parameters
    RtAudio::StreamParameters parameters;
    parameters.deviceId = audio.getDefaultOutputDevice();
    parameters.nChannels = 2;  // Stereo
    parameters.firstChannel = 0;
    
    unsigned int sampleRate = 48000;
    unsigned int bufferFrames = 256;
    
    // Create synth instance
    synth = new Synth(static_cast<float>(sampleRate));
    
    try {
        audio.openStream(&parameters, nullptr, RTAUDIO_FLOAT32,
                        sampleRate, &bufferFrames, &audioCallback);
        
        audio.startStream();
        
        // Initialize UI
        UI ui(synth, synthParams);
        if (!ui.initialize()) {
            std::cerr << "Failed to initialize UI\n";
            audio.stopStream();
            audio.closeStream();
            delete synth;
            delete midiHandler;
            delete synthParams;
            return 1;
        }
        
        // Main UI loop
        while (running && audio.isStreamRunning()) {
            // Update UI and handle input
            if (!ui.update()) {
                running = false;  // User pressed 'q'
                break;
            }
            
            // Draw UI
            int activeVoices = synth->getActiveVoiceCount();
            ui.draw(activeVoices);
            
            // Sleep to control refresh rate (~20 FPS)
            usleep(50000);  // 50ms
        }
        
        // Clean shutdown
        if (audio.isStreamRunning()) {
            audio.stopStream();
        }
        if (audio.isStreamOpen()) {
            audio.closeStream();
        }
        
    } catch (RtAudio::RtAudioError& e) {
        std::cerr << "RtAudio error: " << e.getMessage() << '\n';
        delete synth;
        delete midiHandler;
        delete synthParams;
        return 1;
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        delete synth;
        delete midiHandler;
        delete synthParams;
        return 1;
    }
    
    // Clean up
    delete synth;
    delete midiHandler;
    delete synthParams;
    
    return 0;
}