#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
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
#include <chrono>
#include <atomic>
#include <cstdlib>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <sys/stat.h>
#include <pwd.h>
#if defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#include <sched.h>
#endif

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
static std::atomic<uint64_t> gAudioUnderrunCounter{0};

struct AudioDeviceRecord {
    int id;
    RtAudio::DeviceInfo info;
};

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

// Write enumerated RtAudio devices to a helper file for manual reference
std::string writeAudioDeviceListFile(const std::vector<AudioDeviceRecord>& devices) {
    if (devices.empty()) {
        return {};
    }

    std::string configDir = getConfigDirectory();
    mkdir(configDir.c_str(), 0755);

    std::string filePath = configDir + "/available_audio_devices.txt";
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return {};
    }

    file << "Wakefield Audio Device List\n";
    file << "Last updated (epoch): " << std::time(nullptr) << "\n";
    file << "Use the index in 'audio_device=<id>' inside device_config.txt\n\n";

    for (const auto& record : devices) {
        const auto& info = record.info;
        file << "[" << record.id << "] " << info.name << "\n";
        file << "  outputs: " << info.outputChannels
             << "  inputs: " << info.inputChannels
             << "  duplex: " << info.duplexChannels << "\n";
        file << "  preferred_sample_rate: " << info.preferredSampleRate << "\n";
        file << "  sample_rates:";
        if (info.sampleRates.empty()) {
            file << " (none reported)";
        } else {
            for (auto rate : info.sampleRates) {
                file << " " << rate;
            }
        }
        file << "\n";
        file << "  is_default_output: " << (info.isDefaultOutput ? "yes" : "no")
             << "  is_default_input: " << (info.isDefaultInput ? "yes" : "no") << "\n\n";
    }

    return filePath;
}

// Read device config
void readDeviceConfig(int& audioDeviceId, int& midiPort, unsigned int& bufferSize, unsigned int& sampleRate) {
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
            } else if (line.find("sample_rate=") == 0) {
                sampleRate = static_cast<unsigned int>(std::stoi(line.substr(12)));
            }
        }
        file.close();
    }
}

// Write device config
void writeDeviceConfig(int audioDeviceId, int midiPort, unsigned int bufferSize, unsigned int sampleRate) {
    std::string configDir = getConfigDirectory();
    mkdir(configDir.c_str(), 0755);

    std::string configPath = configDir + "/device_config.txt";
    std::ofstream file(configPath);

    if (file.is_open()) {
        file << "audio_device=" << audioDeviceId << "\n";
        file << "midi_port=" << midiPort << "\n";
        file << "buffer_size=" << bufferSize << "\n";
        file << "sample_rate=" << sampleRate << "\n";
        file.close();
    }
}

// Restart app with new devices
void restartWithNewDevices(int audioDeviceId, int midiPort, unsigned int bufferSize, unsigned int sampleRate,
                           SynthParameters* params, char** argv) {
    // Save current state as temp preset
    PresetManager::savePreset("__temp_restart__", params);
    // Save full UI/Sequencer/Sampler state alongside temp preset if available
    try {
        std::string baseDir = PresetManager::getPresetDirectory();
        std::string name = "__temp_restart__";
        if (ui) {
            // State sidecar
            std::ofstream f(baseDir + "/" + name + ".state");
            if (f.is_open()) { f << ui->serializeState(); }
        }
        if (sequencer) {
            // Patterns
            for (int t = 0; t < 4; ++t) {
                std::string trackPath = baseDir + "/" + name + "_track" + std::to_string(t+1) + ".csv";
                sequencer->getTrack(t).getPattern().saveToFile(trackPath);
            }
            // Sequencer meta
            std::ofstream sm(baseDir + "/" + name + ".seq.txt");
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
        }
        if (synth) {
            // Sampler selections
            std::ofstream sf(baseDir + "/" + name + ".samplers.txt");
            if (sf.is_open()) {
                // Save current sample directory (if UI provided)
                if (ui) {
                    sf << "sample_dir=" << ui->getSampleDirectory() << "\n";
                }
                for (int i = 0; i < 4; ++i) {
                    int sidx = synth->getSamplerSampleIndex(i);
                    const SampleBank* bank = synth->getSampleBank();
                    const char* sname = (bank && sidx >= 0) ? bank->getSampleName(sidx) : nullptr;
                    sf << "sampler" << i << '=' << (sname ? sname : "") << "\n";
                }
            }
        }
    } catch (...) {}
    
    // Write new device config
    writeDeviceConfig(audioDeviceId, midiPort, bufferSize, sampleRate);
    
    // Restart the application
    execv(argv[0], argv);
    
    // If execv fails, we'll continue running
    std::cerr << "Failed to restart application\n";
}

// Helper function to apply MIDI CC to a parameter
void applyMIDICCToParameter(int paramId, int ccValue) {
    if (!synthParams) return;

    const InlineParameter* parameter = ParameterRegistry::Lookup(paramId);
    if (!parameter) {
        return;
    }

    if (paramId == 400) {
        return;
    }

    float ccNormalized = std::clamp(static_cast<float>(ccValue) / 127.0f, 0.0f, 1.0f);

    // MIDI Pickup Mode: prevent parameter jumps when controller hasn't picked up current value
    bool pickedUp = synthParams->parameterCCPickedUp[paramId].load();
    int lastCCValue = synthParams->parameterCCLastValue[paramId].load();

    if (!pickedUp && lastCCValue >= 0) {
        // Get current parameter value to check if CC has picked it up
        if (ui) {
            int oscIndex = synthParams->parameterContextOsc[paramId].load();
            int lfoIndex = synthParams->parameterContextLFO[paramId].load();
            int envIndex = synthParams->parameterContextEnv[paramId].load();
            int samplerIndex = synthParams->parameterContextSampler[paramId].load();
            int chaosIndex = synthParams->parameterContextChaos[paramId].load();

            // Temporarily set UI context indices to get the right parameter value
            int savedOsc = ui->currentOscillatorIndex;
            int savedLfo = ui->currentLFOIndex;
            int savedEnv = ui->currentEnvelopeIndex;
            int savedSampler = ui->currentSamplerIndex;
            int savedChaos = ui->currentChaosIndex;

            if (oscIndex >= 0) ui->currentOscillatorIndex = oscIndex;
            if (lfoIndex >= 0) ui->currentLFOIndex = lfoIndex;
            if (envIndex >= 0) ui->currentEnvelopeIndex = envIndex;
            if (samplerIndex >= 0) ui->currentSamplerIndex = samplerIndex;
            if (chaosIndex >= 0) ui->currentChaosIndex = chaosIndex;

            float currentValue = ui->getParameterValue(paramId);
            float currentNormalized = parameter->normalize(currentValue);

            // Restore saved indices
            ui->currentOscillatorIndex = savedOsc;
            ui->currentLFOIndex = savedLfo;
            ui->currentEnvelopeIndex = savedEnv;
            ui->currentSamplerIndex = savedSampler;
            ui->currentChaosIndex = savedChaos;

            // Check if CC value is within pickup tolerance (3/127 ≈ 2.4%)
            constexpr float pickupTolerance = 3.0f / 127.0f;
            if (std::abs(ccNormalized - currentNormalized) <= pickupTolerance) {
                // CC has picked up the current value - allow control
                synthParams->parameterCCPickedUp[paramId] = true;
                pickedUp = true;
            } else {
                // Not picked up yet - ignore this CC message
                synthParams->parameterCCLastValue[paramId] = ccValue;
                return;
            }
        }
    }

    // Store last CC value
    synthParams->parameterCCLastValue[paramId] = ccValue;

    // Apply the CC value to the parameter
    float value = parameter->denormalize(ccNormalized);

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
    // If the UI is waiting for a sequencer note entry, consume this MIDI note as input
    if (ui && ui->handleMidiNoteForSequencerNumeric(note)) {
        return;
    }
    // If the UI is waiting for a frequency input (free-running oscillator), consume this MIDI note
    if (ui && ui->handleMidiNoteForFrequencyInput(note)) {
        return;
    }
    // If the UI is waiting for a sampler tune input, consume this MIDI note
    if (ui && ui->handleMidiNoteForTuneInput(note)) {
        return;
    }
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

            // Reset pickup state when learning a new CC mapping
            synthParams->parameterCCLastValue[paramId] = -1;
            synthParams->parameterCCPickedUp[paramId] = false;

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
    auto callbackStart = std::chrono::high_resolution_clock::now();

    static std::chrono::high_resolution_clock::time_point previousCallbackStart;
    static bool havePreviousCallbackStart = false;
    uint32_t intervalMicros = 0;
    if (havePreviousCallbackStart) {
        intervalMicros = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(callbackStart - previousCallbackStart).count());
    }
    previousCallbackStart = callbackStart;
    havePreviousCallbackStart = true;

    int schedulerPolicy = -1;
    int schedulerPriority = -1;
    int callbackCpu = -1;
#if defined(__unix__) || defined(__APPLE__)
    struct sched_param schedParam;
    int policy = 0;
    if (pthread_getschedparam(pthread_self(), &policy, &schedParam) == 0) {
        schedulerPolicy = policy;
        schedulerPriority = schedParam.sched_priority;
    }
#endif
#if defined(__linux__)
    int cpu = sched_getcpu();
    if (cpu >= 0) {
        callbackCpu = cpu;
    }
#endif

    // Detect buffer underruns (audio glitching/crackling)
    if (status & RTAUDIO_OUTPUT_UNDERFLOW) {
        uint64_t count = ++gAudioUnderrunCounter;
        if (ui) {
            ui->setAudioUnderrunCount(count);
        }
        if (count % 10 == 1) {
            std::cerr << "⚠️  BUFFER UNDERRUN #" << count
                      << " - Audio thread can't keep up!" << std::endl;
        }
    }
    if (status & RTAUDIO_INPUT_OVERFLOW) {
        std::cerr << "⚠️  Input overflow detected\n";
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
            int shapeIndex = synthParams->getOscShape(oscIndex);
            float baseFreq = (oscIndex == 0) ? smoothedOscillatorFreq : synthParams->getOscFrequency(oscIndex);

            // Apply oscillator octave offset
            int octave = synth->getOscillatorOctave(oscIndex);
            baseFreq *= std::pow(2.0f, static_cast<float>(octave));

            float morph = (oscIndex == 0) ? smoothedOscillatorMorph : synthParams->getOscMorph(oscIndex);
            float ratio = synthParams->getOscRatio(oscIndex);
            float offset = synthParams->getOscOffset(oscIndex);
            float amp = synthParams->getOscAmp(oscIndex);
            float level = synthParams->getOscLevel(oscIndex);

            synth->setOscillatorState(
                oscIndex,
                shapeIndex,
                baseFreq,
                morph,
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
                sequencer ? sequencer->getTempo() : 120.0f
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
    constexpr unsigned int nChannels = 2;
    float currentTempo = sequencer ? sequencer->getTempo() : 120.0f;
    if (synth) {
        synth->process(buffer, nFrames, nChannels, currentTempo);
    } else {
        std::fill(buffer, buffer + nFrames * nChannels, 0.0f);
    }

    auto callbackEnd = std::chrono::high_resolution_clock::now();
    uint32_t callbackMicros = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(callbackEnd - callbackStart).count());

    float peak = 0.0f;
    for (unsigned int i = 0; i < nFrames * nChannels; ++i) {
        peak = std::max(peak, std::fabs(buffer[i]));
    }

    uint32_t activeVoices = synth ? static_cast<uint32_t>(synth->getActiveVoiceCount()) : 0;
    if (ui) {
        ui->updateAudioDebugStats(callbackMicros, peak, activeVoices,
                                  intervalMicros, schedulerPolicy, schedulerPriority, callbackCpu);
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
    unsigned int preferredSampleRate = 48000;
    readDeviceConfig(preferredAudioDevice, preferredMidiPort, preferredBufferSize, preferredSampleRate);
    
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

    unsigned int sampleRate = (preferredSampleRate > 0) ? preferredSampleRate : 48000;
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
    std::vector<AudioDeviceRecord> audioDeviceRecords;
    unsigned int deviceCount = audio.getDeviceCount();
    int audioDeviceIdToUse = -1;
    
    for (unsigned int i = 0; i < deviceCount; ++i) {
        try {
            RtAudio::DeviceInfo info = audio.getDeviceInfo(i);
            if (info.outputChannels > 0) {  // Only list output devices
                audioDevices.push_back({static_cast<int>(i), info.name});
                audioDeviceRecords.push_back({static_cast<int>(i), info});
            }
        } catch (...) {
            continue;
        }
    }

    if (!audioDeviceRecords.empty()) {
        std::string deviceListPath = writeAudioDeviceListFile(audioDeviceRecords);
        if (!deviceListPath.empty()) {
            std::cout << "Audio device list written to " << deviceListPath << "\n";
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
        const std::string name = "__temp_restart__";
        const std::string baseDir = PresetManager::getPresetDirectory();
        PresetManager::loadPreset(name, synthParams);
        // Load sidecars if present
        try {
            // UI state
            std::ifstream fs(baseDir + "/" + name + ".state");
            if (fs.good()) { std::stringstream ss; ss << fs.rdbuf(); ui->applySerializedState(ss.str()); }
        } catch (...) {}
        try {
            // Sequencer patterns
            if (sequencer) {
                for (int t = 0; t < 4; ++t) {
                    std::string tp = baseDir + "/" + name + "_track" + std::to_string(t+1) + ".csv";
                    sequencer->getTrack(t).getPattern().loadFromFile(tp);
                }
            }
        } catch (...) {}
        try {
            // Sequencer meta
            if (sequencer) {
                std::ifstream fm(baseDir + "/" + name + ".seq.txt");
                if (fm.good()) {
                    std::string line;
                    while (std::getline(fm, line)) {
                        if (line.empty() || line[0] == '#') continue;
                        auto eq = line.find('='); if (eq == std::string::npos) continue;
                        auto trim = [](std::string s){ size_t a=s.find_first_not_of(" \t\r\n"); size_t b=s.find_last_not_of(" \t\r\n"); if (a==std::string::npos) return std::string(); return s.substr(a,b-a+1); };
                        std::string key = trim(line.substr(0,eq)); std::string val = trim(line.substr(eq+1));
                        if (key == "tempo") { try { sequencer->setTempo(std::stod(val)); } catch (...) {} continue; }
                        if (key.rfind("t",0)==0) {
                            size_t dot = key.find('.'); if (dot==std::string::npos) continue;
                            int tIdx = std::stoi(key.substr(1, dot-1)); if (tIdx<0||tIdx>=4) continue;
                            std::string field = key.substr(dot+1);
                            auto& track = sequencer->getTrack(tIdx);
                            auto& cons = track.getConstraints();
                            auto& eu = track.getEuclideanPattern();
                            try {
                                if (field=="len") track.getPattern().setLength(std::stoi(val));
                                else if (field=="subdiv") track.setSubdivision(static_cast<Subdivision>(std::stoi(val)));
                                else if (field=="muted") track.setMuted(val=="1"||val=="true");
                                else if (field=="solo") track.setSolo(val=="1"||val=="true");
                                else if (field=="eu_hits") eu.setHits(std::stoi(val));
                                else if (field=="eu_steps") eu.setSteps(std::stoi(val));
                                else if (field=="eu_rot") eu.setRotation(std::stoi(val));
                                else if (field=="scale") cons.setScale(static_cast<Scale>(std::stoi(val)));
                                else if (field=="root") cons.setRootNote(std::stoi(val));
                                else if (field=="oct_min") cons.setOctaveRange(std::stoi(val), cons.getOctaveMax());
                                else if (field=="oct_max") cons.setOctaveRange(cons.getOctaveMin(), std::stoi(val));
                                else if (field=="density") cons.setDensity(std::stof(val));
                                else if (field=="max_interval") cons.setMaxInterval(std::stoi(val));
                                else if (field=="contour") cons.setContour(static_cast<Contour>(std::stoi(val)));
                                else if (field=="gravity") cons.setGravityNote(std::stoi(val));
                                else if (field=="phase_driver") sequencer->setTrackPhaseDriver(tIdx, static_cast<Sequencer::PhaseDriver>(std::stoi(val)));
                            } catch (...) {}
                        }
                    }
                }
            }
        } catch (...) {}
        try {
            // Sampler selections
            if (synth) {
                std::ifstream fsamp(baseDir + "/" + name + ".samplers.txt");
                if (fsamp.good()) {
                    auto* bank = synth->getSampleBank();
                    std::string line;
                    while (std::getline(fsamp, line)) {
                        if (line.empty() || line[0]=='#') continue;
                        auto eq = line.find('='); if (eq==std::string::npos) continue;
                        std::string key = line.substr(0,eq); std::string val = line.substr(eq+1);
                        if (key.rfind("sampler",0)==0) { int idx = std::stoi(key.substr(7)); if (idx>=0&&idx<4 && bank) { int sidx = bank->findSampleByName(val.c_str()); if (sidx>=0) synth->setSamplerSample(idx, sidx); } }
                    }
                }
            }
        } catch (...) {}

        ui->addConsoleMessage("Restored previous state");
        // Delete temp preset after loading (sidecars too)
        unlink(tempPresetPath.c_str());
        unlink((baseDir + "/" + name + ".state").c_str());
        unlink((baseDir + "/" + name + ".seq.txt").c_str());
        for (int t = 0; t < 4; ++t) {
            std::string trackPath = baseDir + "/" + name + "_track" + std::to_string(t+1) + ".csv";
            unlink(trackPath.c_str());
        }
        unlink((baseDir + "/" + name + ".samplers.txt").c_str());
    }

    ui->initializeAutosaveSession();
    
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
            restartWithNewDevices(newAudioDevice, newMidiPort, bufferFrames, sampleRate, synthParams, argv);
            
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
                    writeDeviceConfig(ui->getCurrentAudioDeviceId(), ui->getCurrentMidiPort(), bufferFrames, sampleRate);
                    ui->addConsoleMessage("Audio buffer size set to " + std::to_string(bufferFrames) + " samples");
                }

                ui->clearBufferSizeChangeRequest();
            }
        }

        if (ui->isSampleRateChangeRequested()) {
            unsigned int requestedRate = static_cast<unsigned int>(ui->getRequestedSampleRate());

            if (!(deviceCount > 0 && parameters.deviceId >= 0)) {
                ui->addConsoleMessage("No audio device available to change sample rate");
                ui->clearSampleRateChangeRequest();
            } else if (requestedRate == 0) {
                ui->addConsoleMessage("Invalid sample rate request");
                ui->clearSampleRateChangeRequest();
            } else {
                unsigned int previousRate = sampleRate;
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

                // Delete old synth, clock, and sequencer instances (they store sample rate)
                synth->setUI(nullptr);
                ui->setSynthInstance(nullptr);
                delete sequencer;
                delete synth;
                delete transportClock;

                // Create new synth and clock with new sample rate
                synth = new Synth(static_cast<float>(requestedRate));
                synth->setUI(ui);
                synth->setParams(synthParams);
                ui->setSynthInstance(synth);
                transportClock = new Clock(static_cast<float>(requestedRate));
                synth->setClock(transportClock);
                sequencer = new Sequencer(transportClock, synth);

                unsigned int openFrames = bufferFrames;
                try {
                    audio.openStream(&parameters, nullptr, RTAUDIO_FLOAT32,
                                     requestedRate, &openFrames, &audioCallback);
                    audio.startStream();
                    sampleRate = requestedRate;
                    bufferFrames = openFrames;
                    audioAvailable = true;
                    reopened = true;
                } catch (std::exception& e) {
                    failureReason = e.what();
                } catch (...) {
                    failureReason = "unknown error";
                }

                if (!reopened) {
                    // Attempt to restore previous sample rate
                    delete sequencer;
                    delete synth;
                    delete transportClock;
                    synth = new Synth(static_cast<float>(previousRate));
                    synth->setUI(ui);
                    synth->setParams(synthParams);
                    ui->setSynthInstance(synth);
                    transportClock = new Clock(static_cast<float>(previousRate));
                    synth->setClock(transportClock);
                    sequencer = new Sequencer(transportClock, synth);

                    try {
                        unsigned int restoreFrames = bufferFrames;
                        audio.openStream(&parameters, nullptr, RTAUDIO_FLOAT32,
                                         previousRate, &restoreFrames, &audioCallback);
                        audio.startStream();
                        bufferFrames = restoreFrames;
                        audioAvailable = true;
                    } catch (...) {
                        audioAvailable = false;
                    }
                    if (failureReason.empty()) {
                        failureReason = "unable to open stream";
                    }
                    ui->addConsoleMessage("Failed to set sample rate: " + failureReason);
                    if (audioAvailable) {
                        try {
                            RtAudio::DeviceInfo deviceInfo = audio.getDeviceInfo(parameters.deviceId);
                            audioDeviceName = deviceInfo.name;
                        } catch (...) {
                            // Keep previous name if query fails
                        }
                        ui->setDeviceInfo(audioDeviceName, previousRate, bufferFrames, midiDeviceName, midiPortToUse);
                    }
                } else {
                    try {
                        RtAudio::DeviceInfo deviceInfo = audio.getDeviceInfo(parameters.deviceId);
                        audioDeviceName = deviceInfo.name;
                    } catch (...) {
                        // Keep previous name if query fails
                    }
                    ui->setDeviceInfo(audioDeviceName, sampleRate, bufferFrames, midiDeviceName, midiPortToUse);
                    writeDeviceConfig(ui->getCurrentAudioDeviceId(), ui->getCurrentMidiPort(), bufferFrames, sampleRate);
                    ui->addConsoleMessage("Sample rate set to " + std::to_string(sampleRate) + " Hz");
                }

                ui->clearSampleRateChangeRequest();
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
