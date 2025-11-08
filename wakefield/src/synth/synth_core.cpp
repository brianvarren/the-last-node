// ============================================================================
// SYNTH CORE - Constructor, Destructor, and Initialization
// ============================================================================
//
// RESPONSIBILITIES:
// - Synth class constructor: initialize all DSP components with sample rate
// - Synth class destructor: clean shutdown of audio pipeline threads
// - Initial state setup: filter initialization, voice allocation, default values
// - Fast math lookup table initialization
// - Effects pipeline thread lifecycle management
//
// DOES NOT CONTAIN:
// - Audio processing logic (see synth_process.cpp)
// - Voice management (see synth_voices.cpp)
// - Parameter updates (see synth_parameters.cpp)
// - Modulation routing (see synth_modulation.cpp)
//
// ============================================================================

#include "../synth.h"
#include "../clock.h"
#include "../ui.h"
#include "../fast_math.h"

Synth::Synth(float sampleRate)
    : sampleRate(sampleRate)
    , masterVolume(0.7f)  // Increased from 0.5 due to conservative normalization
    , reverbEnabled(false)
    , filterEnabled(false)
    , compressorEnabled(true)
    , currentFilterType(0)
    , ui(nullptr)
    , params(nullptr)
    , clock(nullptr)
    , reverb(sampleRate)
    , compressor(sampleRate)
    , filterL(sampleRate)
    , filterR(sampleRate) {

    // Initialize shelf filters
    highShelfL.setSampleRate(sampleRate);
    highShelfR.setSampleRate(sampleRate);
    lowShelfL.setSampleRate(sampleRate);
    lowShelfR.setSampleRate(sampleRate);

    ladderFilterL.setSampleRate(sampleRate);
    ladderFilterR.setSampleRate(sampleRate);
    diodeFilterL.setSampleRate(sampleRate);
    diodeFilterR.setSampleRate(sampleRate);
    bandpassFilterL.setSampleRate(sampleRate);
    bandpassFilterR.setSampleRate(sampleRate);
    bandpass8FilterL.setSampleRate(sampleRate);
    bandpass8FilterR.setSampleRate(sampleRate);
    bandpass2FilterL.setSampleRate(sampleRate);
    bandpass2FilterR.setSampleRate(sampleRate);
    notchFilterL.setSampleRate(sampleRate);
    notchFilterR.setSampleRate(sampleRate);

    // Initialize fast math lookup tables
    FastMath::initSinTable();

    // Initialize voices with sample rate
    for (int i = 0; i < MAX_VOICES; ++i) {
        voices.emplace_back(sampleRate);
    }

    // Phase 5: Initialize sampler levels to 0.6 (reduced from 0.8 for better headroom)
    for (auto& voice : voices) {
        for (int s = 0; s < SAMPLERS_PER_VOICE; ++s) {
            voice.samplers[s].setLevel(0.6f);
        }
    }

    // Initialize chaos generators with sample rate
    for (int i = 0; i < 4; ++i) {
        chaos[i].setSampleRate(sampleRate);
    }

    std::fill(std::begin(globalOscOutputs), std::end(globalOscOutputs), 0.0f);
    std::fill(std::begin(globalSamplerOutputs), std::end(globalSamplerOutputs), 0.0f);
    std::fill(std::begin(globalOscOutputsPrev), std::end(globalOscOutputsPrev), 0.0f);
    std::fill(std::begin(globalSamplerOutputsPrev), std::end(globalSamplerOutputsPrev), 0.0f);

    compressor.setAutoMakeup(false);
    compressor.setManualMakeup(12.0f);

    // Initialize effects pipeline threading
    effectsThreadRunning = true;
    effectsThread = std::thread(&Synth::effectsThreadFunc, this);
}

Synth::~Synth() {
    // Shutdown effects thread
    effectsThreadRunning = false;
    effectsCV.notify_all();
    if (effectsThread.joinable()) {
        effectsThread.join();
    }
}

float Synth::midiNoteToFrequency(int midiNote) {
    // MIDI note 69 = A4 = 440 Hz
    // Formula: f = 440 * 2^((n-69)/12)
    return 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
}

void Synth::setParams(SynthParameters* params_ptr) {
    params = params_ptr;
    // Update all voice pointers for FM matrix access and base levels
    for (auto& voice : voices) {
        voice.params = params_ptr;
        voice.synth = this;
    }
}

void Synth::resetAudioState() {
    for (auto& voice : voices) {
        voice.forceSilence();
    }
    std::fill(fmSourceBuffer.begin(), fmSourceBuffer.end(), 0.0f);
    for (int i = 0; i < 4; ++i) {
        chaosBufferX[i].clear();
        chaosBufferY[i].clear();
        chaosOutputs[i] = 0.0f;
    }
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        freeRunningVoiceActive[i] = false;
        freeRunningVoiceIndex[i] = -1;
        oscillatorNoteSourceFree[i] = false;
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        freeRunningSamplerActive[i] = false;
        freeRunningSamplerVoiceIndex[i] = -1;
    }
}
