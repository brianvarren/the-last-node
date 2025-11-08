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
    std::cout << "Configuring stress-test preset...\n";

    // === Oscillators: All 4 active, unmuted ===
    std::cout << "  Enabling all oscillators...\n";
    params.activeOscCount = 4;
    params.oscMuted[0] = false;
    params.oscMuted[1] = false;
    params.oscMuted[2] = false;
    params.oscMuted[3] = false;

    // === FM Matrix: Create complex FM routing ===
    std::cout << "  Configuring FM matrix...\n";
    params.fmGlobalDepth = 0.6f;
    params.fmMatrix[1][0] = 0.3f;  // OSC1 -> OSC2
    params.fmMatrix[2][1] = 0.25f; // OSC2 -> OSC3
    params.fmMatrix[3][2] = 0.2f;  // OSC3 -> OSC4

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

    std::cout << "Stress-test configuration complete:\n";
    std::cout << "  - 4 Oscillators active with FM matrix (3 connections)\n";
    std::cout << "  - 4 LFOs running (0.5Hz - 20Hz)\n";
    std::cout << "  - 4 Chaos generators (2-15Hz, mixed fast/slow modes)\n";
    std::cout << "  - Ladder filter enabled (800Hz cutoff, Q=0.6)\n";
    std::cout << "  - Greyhole reverb active (30% mix, size=0.7)\n";
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
        std::cout << "Enabling stress-test features...\n";

        // All 4 oscillators active
        params.oscMuted[0] = false;
        params.oscMuted[1] = false;
        params.oscMuted[2] = false;
        params.oscMuted[3] = false;

        // FM Matrix routing
        params.fmGlobalDepth = 0.6f;
        params.fmMatrix[1][0] = 0.3f;  // OSC1 -> OSC2
        params.fmMatrix[2][1] = 0.25f; // OSC2 -> OSC3
        params.fmMatrix[3][2] = 0.2f;  // OSC3 -> OSC4

        // Filter enabled
        synth.setFilterEnabled(true);
        synth.updateFilterParameters(4, 800.0f, 0.0f, 0.6f, 1.5f, 200.0f, 0.5f, 0.3f, 0.5f);

        // Reverb enabled
        synth.setReverbEnabled(true);
        synth.updateReverbParameters(0.5f, 0.7f, 0.5f, 0.3f, 0.6f, 0.6f, 0.15f, 2.0f);

        // All 4 LFOs running
        synth.updateLFOParameters(0, 2.0f, 0, 0, 0.7f, 0.5f, false, false, 120.0f);
        synth.updateLFOParameters(1, 0.5f, 0, 1, 0.5f, 0.3f, false, false, 120.0f);
        synth.updateLFOParameters(2, 0.1f, 0, 0, 0.3f, 0.5f, false, false, 120.0f);
        synth.updateLFOParameters(3, 0.05f, 0, 1, 0.5f, 0.7f, false, false, 120.0f);

        std::cout << "  - 4 Oscillators unmuted + FM matrix\n";
        std::cout << "  - 4 LFOs active\n";
        std::cout << "  - Ladder filter enabled\n";
        std::cout << "  - Greyhole reverb enabled\n\n";
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
