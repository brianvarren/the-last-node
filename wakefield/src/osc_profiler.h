#ifndef OSC_PROFILER_H
#define OSC_PROFILER_H

#include <chrono>
#include <cstdint>
#include <cstdio>

// Detailed profiling for BrainwaveOscillator::process
struct OscProfiler {
    // Timing accumulators (nanoseconds)
    uint64_t totalCalls = 0;
    uint64_t timeFreqCalc = 0;
    uint64_t timeFM = 0;
    uint64_t timePhaseInc = 0;
    uint64_t timeModulation = 0;
    uint64_t timeSampleGen = 0;
    uint64_t timePhaseUpdate = 0;

    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    inline TimePoint now() const { return Clock::now(); }

    inline uint64_t elapsed_ns(TimePoint start) const {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(now() - start).count();
    }

    void reset() {
        totalCalls = 0;
        timeFreqCalc = 0;
        timeFM = 0;
        timePhaseInc = 0;
        timeModulation = 0;
        timeSampleGen = 0;
        timePhaseUpdate = 0;
    }

    void report() const {
        if (totalCalls == 0) return;

        uint64_t totalTime = timeFreqCalc + timeFM + timePhaseInc +
                            timeModulation + timeSampleGen + timePhaseUpdate;

        printf("\n=== BrainwaveOscillator::process Profiling (%lu calls) ===\n", totalCalls);
        printf("%-20s %12s %12s %8s\n", "Section", "Total (ms)", "Per-call", "% CPU");
        printf("%-20s %12.3f %9.1f ns %7.1f%%\n",
               "Freq Calc", timeFreqCalc / 1e6,
               (double)timeFreqCalc / totalCalls,
               100.0 * timeFreqCalc / totalTime);
        printf("%-20s %12.3f %9.1f ns %7.1f%%\n",
               "FM Modulation", timeFM / 1e6,
               (double)timeFM / totalCalls,
               100.0 * timeFM / totalTime);
        printf("%-20s %12.3f %9.1f ns %7.1f%%\n",
               "Phase Increment", timePhaseInc / 1e6,
               (double)timePhaseInc / totalCalls,
               100.0 * timePhaseInc / totalTime);
        printf("%-20s %12.3f %9.1f ns %7.1f%%\n",
               "Morph/Duty Mod", timeModulation / 1e6,
               (double)timeModulation / totalCalls,
               100.0 * timeModulation / totalTime);
        printf("%-20s %12.3f %9.1f ns %7.1f%%\n",
               "Sample Generation", timeSampleGen / 1e6,
               (double)timeSampleGen / totalCalls,
               100.0 * timeSampleGen / totalTime);
        printf("%-20s %12.3f %9.1f ns %7.1f%%\n",
               "Phase Update", timePhaseUpdate / 1e6,
               (double)timePhaseUpdate / totalCalls,
               100.0 * timePhaseUpdate / totalTime);
        printf("%-20s %12.3f %9.1f ns %7.1f%%\n",
               "TOTAL", totalTime / 1e6,
               (double)totalTime / totalCalls,
               100.0);
        printf("\n");
    }
};

// Global profiler (defined in brainwave_osc.cpp)
extern OscProfiler g_oscProfiler;

#endif // OSC_PROFILER_H
