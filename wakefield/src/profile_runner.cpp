#include "synth.h"
#include "ui.h"
#include "clock.h"
#include "sequencer.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// Define sequencer global to satisfy UI code linkage (not used in profile_runner)
Sequencer* sequencer = nullptr;

namespace {
struct ProfileConfig {
    unsigned int sampleRate = 48000;
    unsigned int bufferSize = 256;
    unsigned int iterations = 20000;
    unsigned int voices = 8;
};

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

    constexpr unsigned int channels = 2;
    std::vector<float> buffer(static_cast<size_t>(cfg.bufferSize) * channels, 0.0f);

    // Prime the synth with active voices to simulate worst-case load.
    for (unsigned int v = 0; v < cfg.voices; ++v) {
        const int note = 48 + static_cast<int>(v) * 2;
        synth.noteOn(note, 100);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (unsigned int i = 0; i < cfg.iterations; ++i) {
        synth.process(buffer.data(), cfg.bufferSize, channels);
        clock.advance(cfg.bufferSize);

        // Periodically retrigger a voice to keep envelopes and modulation busy.
        if ((i % 1024) == 1023) {
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

    return 0;
}
