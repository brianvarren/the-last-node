#ifndef VOICE_PROFILER_H
#define VOICE_PROFILER_H

#include <chrono>
#include <cstdint>

// Detailed profiling for Voice::generateSample
struct VoiceProfiler {
    // Timing accumulators (nanoseconds)
    uint64_t totalSamples = 0;
    uint64_t timeSmoothing = 0;
    uint64_t timeOscFM = 0;
    uint64_t timeOscProcess = 0;
    uint64_t timeOscGainCalc = 0;
    uint64_t timeSamplerFM = 0;
    uint64_t timeSamplerProcess = 0;
    uint64_t timeMixing = 0;
    uint64_t timeHistoryUpdate = 0;

    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    inline TimePoint now() const { return Clock::now(); }

    inline uint64_t elapsed_ns(TimePoint start) const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(now() - start).count();
    }

    void reset() {
        totalSamples = 0;
        timeSmoothing = 0;
        timeOscFM = 0;
        timeOscProcess = 0;
        timeOscGainCalc = 0;
        timeSamplerFM = 0;
        timeSamplerProcess = 0;
        timeMixing = 0;
        timeHistoryUpdate = 0;
    }

    void report() const {
        if (totalSamples == 0) return;

        uint64_t totalTime = timeSmoothing + timeOscFM + timeOscProcess +
                            timeOscGainCalc + timeSamplerFM + timeSamplerProcess +
                            timeMixing + timeHistoryUpdate;

        printf("\n=== Voice::generateSample Profiling (%lu samples) ===\n", totalSamples);
        printf("%-20s %12s %12s %8s\n", "Section", "Total (ms)", "Per-sample", "% CPU");
        printf("%-20s %12.3f %9.1f ns %7.1f%%\n",
               "Smoothing", timeSmoothing / 1e6,
               (double)timeSmoothing / totalSamples,
               100.0 * timeSmoothing / totalTime);
        printf("%-20s %12.3f %9.1f ns %7.1f%%\n",
               "Osc FM", timeOscFM / 1e6,
               (double)timeOscFM / totalSamples,
               100.0 * timeOscFM / totalTime);
        printf("%-20s %12.3f %9.1f ns %7.1f%%\n",
               "Osc Process", timeOscProcess / 1e6,
               (double)timeOscProcess / totalSamples,
               100.0 * timeOscProcess / totalTime);
        printf("%-20s %12.3f %9.1f ns %7.1f%%\n",
               "Osc Gain Calc", timeOscGainCalc / 1e6,
               (double)timeOscGainCalc / totalSamples,
               100.0 * timeOscGainCalc / totalTime);
        printf("%-20s %12.3f %9.1f ns %7.1f%%\n",
               "Sampler FM", timeSamplerFM / 1e6,
               (double)timeSamplerFM / totalSamples,
               100.0 * timeSamplerFM / totalTime);
        printf("%-20s %12.3f %9.1f ns %7.1f%%\n",
               "Sampler Process", timeSamplerProcess / 1e6,
               (double)timeSamplerProcess / totalSamples,
               100.0 * timeSamplerProcess / totalTime);
        printf("%-20s %12.3f %9.1f ns %7.1f%%\n",
               "Mixing", timeMixing / 1e6,
               (double)timeMixing / totalSamples,
               100.0 * timeMixing / totalTime);
        printf("%-20s %12.3f %9.1f ns %7.1f%%\n",
               "History Update", timeHistoryUpdate / 1e6,
               (double)timeHistoryUpdate / totalSamples,
               100.0 * timeHistoryUpdate / totalTime);
        printf("%-20s %12.3f %9.1f ns %7.1f%%\n",
               "TOTAL", totalTime / 1e6,
               (double)totalTime / totalSamples,
               100.0);
        printf("\n");
    }
};

// Global profiler (defined in voice.cpp)
extern VoiceProfiler g_voiceProfiler;

#endif // VOICE_PROFILER_H
