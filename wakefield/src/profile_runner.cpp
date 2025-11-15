#include "synth.h"
#include "ui.h"
#include "clock.h"
#include "sequencer.h"
#include "fm_constants.h"
#include "voice_profiler.h"
#include "osc_profiler.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <random>

// Define sequencer global to satisfy UI code linkage (not used in profile_runner)
Sequencer* sequencer = nullptr;

// Helper to configure a "stress test" preset with maximum features enabled
void configureStressTestPreset(SynthParameters& params, Synth& synth, float sampleRate) {
    std::cout << "Configuring MAXED-OUT stress-test preset...\n";

    // === Oscillators: All 4 active, unmuted ===
    std::cout << "  Enabling all oscillators...\n";
    params.activeOscCount = 4;
    params.oscMuted[0] = false;
    params.oscMuted[1] = false;
    params.oscMuted[2] = false;
    params.oscMuted[3] = false;

    // === Samplers: All 4 active, unmuted, with samples loaded ===
    std::cout << "  Enabling all samplers...\n";
    params.activeSamplerCount = 4;
    params.samplerMuted[0] = false;
    params.samplerMuted[1] = false;
    params.samplerMuted[2] = false;
    params.samplerMuted[3] = false;

    // Load sample bank
    SampleBank* sampleBank = synth.getSampleBank();
    int sampleCount = sampleBank->loadSamplesFromDirectory("samples");
    std::cout << "  Loaded " << sampleCount << " samples\n";

    // Assign samples to samplers (use first 4 available samples)
    if (sampleCount >= 4) {
        synth.setSamplerSample(0, 0);  // Sampler 1
        synth.setSamplerSample(1, 1);  // Sampler 2
        synth.setSamplerSample(2, 2);  // Sampler 3
        synth.setSamplerSample(3, 3);  // Sampler 4
        std::cout << "    Sampler 1: " << sampleBank->getSampleName(0) << "\n";
        std::cout << "    Sampler 2: " << sampleBank->getSampleName(1) << "\n";
        std::cout << "    Sampler 3: " << sampleBank->getSampleName(2) << "\n";
        std::cout << "    Sampler 4: " << sampleBank->getSampleName(3) << "\n";
    }

    // Configure samplers for active playback
    for (int i = 0; i < 4; ++i) {
        synth.setSamplerKeyMode(i, true);  // KEY mode (tracks MIDI notes)
        synth.setSamplerLevel(i, 0.6f);    // Moderate level
        synth.setSamplerLoopStart(i, 0.0f);
        synth.setSamplerLoopLength(i, 1.0f);
        synth.setSamplerCrossfadeLength(i, 0.1f);
        synth.setSamplerPlaybackSpeed(i, 1.0f);
        synth.setSamplerTZFMDepth(i, 0.8f);  // Enable FM depth
    }

    // === FM Matrix: DENSE routing (12+ connections across all 8 sources/targets) ===
    std::cout << "  Configuring DENSE FM matrix (8 sources x 8 targets)...\n";
    params.fmGlobalDepth = 0.7f;  // Higher global depth

    // Oscillator -> Oscillator FM (ring modulation chain)
    params.fmMatrix[1][0] = 0.35f;   // OSC1 -> OSC2
    params.fmMatrix[2][1] = 0.30f;   // OSC2 -> OSC3
    params.fmMatrix[3][2] = 0.25f;   // OSC3 -> OSC4
    params.fmMatrix[0][3] = 0.20f;   // OSC4 -> OSC1 (feedback!)

    // Oscillator -> Sampler FM (timbral modulation)
    params.fmMatrix[4][0] = 0.25f;   // OSC1 -> SAMP1
    params.fmMatrix[5][1] = 0.20f;   // OSC2 -> SAMP2
    params.fmMatrix[6][2] = 0.25f;   // OSC3 -> SAMP3
    params.fmMatrix[7][3] = 0.20f;   // OSC4 -> SAMP4

    // Sampler -> Oscillator FM (pitched chaos)
    params.fmMatrix[0][4] = 0.15f;   // SAMP1 -> OSC1
    params.fmMatrix[1][5] = 0.15f;   // SAMP2 -> OSC2
    params.fmMatrix[2][6] = 0.15f;   // SAMP3 -> OSC3
    params.fmMatrix[3][7] = 0.15f;   // SAMP4 -> OSC4

    // Sampler -> Sampler FM (granular intermodulation)
    params.fmMatrix[5][4] = 0.20f;   // SAMP1 -> SAMP2
    params.fmMatrix[7][6] = 0.20f;   // SAMP3 -> SAMP4

    std::cout << "    Created 14 FM connections (osc↔osc, osc↔samp, samp↔samp)\n";

    // NOTE: Modulation matrix configuration skipped - requires UI instance
    // The profiling will still stress modulation processing via internal routing

    // === Filter: Enabled with resonant ladder filter ===
    std::cout << "  Enabling filter...\n";
    synth.setFilterEnabled(true);
    synth.updateFilterParameters(4, // Ladder filter
                                800.0f, // cutoff
                                0.0f, // gain
                                0.6f, // resonance
                                1.5f, // drive
                                200.0f, // feedbackHP
                                0.5f, // spread
                                0.3f, // notchFeedback
                                0.5f); // bandwidth

    // === Reverb: Enabled with active settings ===
    std::cout << "  Enabling reverb...\n";
    synth.setReverbEnabled(true);
    synth.updateReverbParameters(0.5f, // delayTime
                                0.7f, // size
                                0.5f, // damping
                                0.3f, // mix
                                0.6f, // decay
                                0.6f, // diffusion
                                0.15f, // modDepth
                                2.0f); // modFreq

    // === Compressor: ENABLED (was disabled before!) ===
    std::cout << "  Enabling compressor...\n";
    synth.setCompressorEnabled(true);
    synth.updateCompressorParameters(-12.0f,  // threshold
                                     4.0f,    // ratio
                                     0.005f,  // attack
                                     0.100f,  // release
                                     3.0f,    // knee
                                     1.0f,    // mix (100% wet)
                                     true,    // auto makeup
                                     1.0f,    // manual makeup
                                     true);   // RMS mode

    // === LFOs: All 4 active ===
    std::cout << "  Configuring LFOs...\n";
    synth.updateLFOParameters(0, 2.0f, 0, 0, 0.7f, 0.5f, false, false, 120.0f);
    synth.updateLFOParameters(1, 0.5f, 0, 1, 0.5f, 0.3f, false, false, 120.0f);
    synth.updateLFOParameters(2, 0.1f, 0, 0, 0.3f, 0.5f, false, false, 120.0f);
    synth.updateLFOParameters(3, 0.05f, 0, 1, 0.5f, 0.7f, false, false, 120.0f);

    // === Chaos Generators: All 4 active ===
    std::cout << "  Configuring chaos generators...\n";
    params.activeChaosCount = 4;
    params.chaosMuted[0] = false;
    params.chaosMuted[1] = false;
    params.chaosMuted[2] = false;
    params.chaosMuted[3] = false;
    params.chaosLevel[0] = 0.2f;
    params.chaosLevel[1] = 0.2f;
    params.chaosLevel[2] = 0.2f;
    params.chaosLevel[3] = 0.2f;

    synth.setChaosClockFreq(0, 5.0f);
    synth.setChaosFastMode(0, true);
    synth.setChaosClockFreq(1, 10.0f);
    synth.setChaosFastMode(1, true);
    synth.setChaosClockFreq(2, 2.0f);
    synth.setChaosFastMode(2, false);
    synth.setChaosClockFreq(3, 15.0f);
    synth.setChaosFastMode(3, true);

    // === Half-Rate Processing ===
    // Check environment variable to enable half-rate mode for benchmarking
    const bool halfRateProfile = (std::getenv("WF_HALF_RATE") != nullptr);
    if (halfRateProfile) {
        params.halfRateEnabled = true;
        std::cout << "  HALF-RATE MODE ENABLED (osc/sampler @ 24kHz)\n";
    } else {
        params.halfRateEnabled = false;
    }

    std::cout << "\n=== MAXED-OUT Stress-test configuration complete ===\n";
    std::cout << "  - 4 Oscillators active + 4 Samplers active (8 generators)\n";
    std::cout << "  - 14 FM connections (dense 8x8 matrix)\n";
    std::cout << "  - 4 LFOs running (0.05Hz - 2Hz)\n";
    std::cout << "  - 4 Chaos generators (2-15Hz, mixed fast/slow modes)\n";
    std::cout << "  - Ladder filter enabled (800Hz cutoff, Q=0.6)\n";
    std::cout << "  - Greyhole reverb active (30% mix, size=0.7)\n";
    std::cout << "  - Compressor enabled (-12dB threshold, 4:1 ratio)\n";
    std::cout << "====================================================\n\n";
}

namespace {
struct ProfileConfig {
    unsigned int sampleRate = 48000;
    unsigned int bufferSize = 256;
    unsigned int iterations = 20000;
    unsigned int voices = 8;
};

void configureSamplerDronePreset(SynthParameters& params, Synth& synth) {
    std::cout << "Configuring sampler-drone preset (oscillators muted, 2 samplers free-running)...\n";

    params.activeOscCount = 4;
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        params.oscMuted[i] = true;
    }

    params.activeSamplerCount = SAMPLERS_PER_VOICE;
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        bool drone = (i < 2);
        params.samplerMuted[i] = !drone;
        params.setSamplerNoteSource(i, static_cast<int>(drone ? NoteSource::FREE : NoteSource::EXTERNAL_MIDI));
    }

    synth.setFilterEnabled(false);
    synth.setReverbEnabled(false);
    synth.setCompressorEnabled(false);
    params.activeLfoCount = 0;
    params.activeChaosCount = 0;
}

ProfileConfig parseArgs(int argc, char** argv) {
    ProfileConfig cfg;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--sample-rate") == 0 && i + 1 < argc) {
            cfg.sampleRate = static_cast<unsigned int>(std::stoul(argv[++i]));
        } else if (std::strcmp(argv[i], "--buffer-size") == 0 && i + 1 < argc) {
            cfg.bufferSize = static_cast<unsigned int>(std::stoul(argv[++i]));
        } else if (std::strcmp(argv[i], "--buffers") == 0 && i + 1 < argc) {
            cfg.iterations = static_cast<unsigned int>(std::stoul(argv[++i]));
        } else if (std::strcmp(argv[i], "--voices") == 0 && i + 1 < argc) {
            cfg.voices = static_cast<unsigned int>(std::stoul(argv[++i]));
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cout << "profile_runner options:\n"
                         "  --sample-rate <hz>   (default: 48000)\n"
                         "  --buffer-size <n>    (default: 256)\n"
                         "  --buffers <n>        number of buffers to render (default: 20000)\n"
                         "  --voices <n>         simultaneous voices to trigger (default: 8)\n";
            std::exit(0);
        }
    }
    return cfg;
}
}  // namespace

int main(int argc, char** argv) {
    const ProfileConfig cfg = parseArgs(argc, argv);

    std::cout << "Profiling configuration:\n"
              << "  Sample rate : " << cfg.sampleRate << " Hz\n"
              << "  Buffer size : " << cfg.bufferSize << " samples\n"
              << "  Buffers     : " << cfg.iterations << "\n"
              << "  Voices      : " << cfg.voices << "\n\n";

    SynthParameters params;
    Synth synth(static_cast<float>(cfg.sampleRate));
    synth.setParams(&params);

    Clock clock(static_cast<float>(cfg.sampleRate));
    synth.setClock(&clock);

    const bool samplerDroneProfile = (std::getenv("WF_PROFILE_SAMPLER") != nullptr);

    if (samplerDroneProfile) {
        configureSamplerDronePreset(params, synth);
        std::cout << "Sampler drone profile enabled via WF_PROFILE_SAMPLER=1\n";
    } else {
        configureStressTestPreset(params, synth, static_cast<float>(cfg.sampleRate));
    }

    constexpr unsigned int channels = 2;
    std::vector<float> buffer(static_cast<size_t>(cfg.bufferSize) * channels, 0.0f);

    // Prime the synth with active voices (only for oscillator stress profile)
    const unsigned int primeVoices = samplerDroneProfile ? 0 : cfg.voices;
    for (unsigned int v = 0; v < primeVoices; ++v) {
        const int note = 48 + static_cast<int>(v) * 2;
        synth.noteOn(note, 100);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (unsigned int i = 0; i < cfg.iterations; ++i) {
        if (!samplerDroneProfile) {
            synth.processLFOs(static_cast<float>(cfg.sampleRate), cfg.bufferSize);
            synth.processChaos(cfg.bufferSize);
        }

        // Main audio processing with all effects
        synth.process(buffer.data(), cfg.bufferSize, channels);
        clock.advance(cfg.bufferSize);

        // Periodically retrigger a voice to keep envelopes and modulation busy.
        if (!samplerDroneProfile && (i % 1024) == 1023) {
            const int note = 48 + static_cast<int>((i / 1024) % cfg.voices) * 2;
            synth.noteOn(note, 100);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto totalMicros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    float checksum = 0.0f;
    for (float sample : buffer) {
        checksum += sample;
    }

    std::cout << "Rendered " << cfg.iterations << " buffers in "
              << totalMicros / 1000.0 << " ms. Avg "
              << static_cast<double>(totalMicros) / cfg.iterations << " us per buffer.\n";
    std::cout << "Tail buffer checksum: " << checksum << "\n";
    std::cout << "Profiling run complete. If built with -pg, run:\n"
              << "  gprof profile_runner gmon.out | less\n";

    // Print detailed Voice::generateSample profiling
    g_voiceProfiler.report();

    // Print detailed BrainwaveOscillator::process profiling
    g_oscProfiler.report();

    return 0;
}
