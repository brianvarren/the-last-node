#include "../../ui.h"
#include <algorithm>
#include <limits>
#include <cstdio>

void UI::drawDebugPage() {
    int row = 3;

    attron(A_BOLD);
    mvprintw(row++, 1, "AUDIO DEBUG MONITOR");
    attroff(A_BOLD);
    row++;

    uint32_t currentMicros = audioDebugState.currentMicros.load(std::memory_order_relaxed);
    uint32_t minMicros = audioDebugState.minMicros.load(std::memory_order_relaxed);
    uint32_t maxMicros = audioDebugState.maxMicros.load(std::memory_order_relaxed);
    float currentPeak = audioDebugState.currentPeak.load(std::memory_order_relaxed);
    float maxPeak = audioDebugState.maxPeak.load(std::memory_order_relaxed);
    uint32_t currentVoices = audioDebugState.currentVoices.load(std::memory_order_relaxed);
    uint32_t maxVoices = audioDebugState.maxVoices.load(std::memory_order_relaxed);
    uint64_t callbackCount = audioDebugState.callbackCount.load(std::memory_order_relaxed);
    uint64_t totalMicros = audioDebugState.totalMicros.load(std::memory_order_relaxed);
    uint32_t currentInterval = audioDebugState.currentIntervalMicros.load(std::memory_order_relaxed);
    uint32_t minInterval = audioDebugState.minIntervalMicros.load(std::memory_order_relaxed);
    uint32_t maxInterval = audioDebugState.maxIntervalMicros.load(std::memory_order_relaxed);
    uint64_t totalInterval = audioDebugState.totalIntervalMicros.load(std::memory_order_relaxed);
    int schedulerPolicy = audioDebugState.schedulerPolicy.load(std::memory_order_relaxed);
    int schedulerPriority = audioDebugState.schedulerPriority.load(std::memory_order_relaxed);
    int callbackCpu = audioDebugState.callbackCpu.load(std::memory_order_relaxed);

    attron(COLOR_SECTION_HEADER);
    mvprintw(row++, 2, "Audio Callback");
    attroff(COLOR_SECTION_HEADER);
    std::string minMicrosStr = (minMicros == std::numeric_limits<uint32_t>::max())
                                   ? "n/a"
                                   : std::to_string(minMicros);
    std::string maxMicrosStr = (maxMicros == 0)
                                   ? "n/a"
                                   : std::to_string(maxMicros);

    auto formatFloat = [](double value, int precision = 1, bool forceSign = false) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), forceSign ? "%+.*f" : "%.*f", precision, value);
        return std::string(buffer);
    };

    double expectedIntervalMicros = 0.0;
    if (audioSampleRate > 0 && audioBufferSize > 0) {
        expectedIntervalMicros = (static_cast<double>(audioBufferSize) * 1'000'000.0) /
                                 static_cast<double>(audioSampleRate);
    }
    std::string targetMs = expectedIntervalMicros > 0.0
                               ? formatFloat(expectedIntervalMicros / 1000.0, 2, false)
                               : "n/a";

    double avgMicros = (callbackCount > 0)
                           ? static_cast<double>(totalMicros) / static_cast<double>(callbackCount)
                           : 0.0;
    std::string avgMicrosStr = (callbackCount > 0)
                                   ? formatFloat(avgMicros, 1, false)
                                   : "n/a";

    double currentLoad = (expectedIntervalMicros > 0.0 && currentMicros > 0)
                             ? (static_cast<double>(currentMicros) / expectedIntervalMicros) * 100.0
                             : 0.0;
    double avgLoad = (expectedIntervalMicros > 0.0 && callbackCount > 0)
                         ? (avgMicros / expectedIntervalMicros) * 100.0
                         : 0.0;
    double worstLoad = (expectedIntervalMicros > 0.0 && maxMicros > 0)
                           ? (static_cast<double>(maxMicros) / expectedIntervalMicros) * 100.0
                           : 0.0;

    double currentSlack = (expectedIntervalMicros > 0.0)
                              ? expectedIntervalMicros - static_cast<double>(currentMicros)
                              : 0.0;
    double worstSlack = (expectedIntervalMicros > 0.0 && maxMicros > 0)
                            ? expectedIntervalMicros - static_cast<double>(maxMicros)
                            : 0.0;

    mvprintw(row++, 4, "Buffer: %d frames @ %d Hz (target %s ms)",
             audioBufferSize,
             audioSampleRate,
             targetMs.c_str());
    mvprintw(row++, 4, "Duration: %u us (avg %s, min %s, max %s)",
             currentMicros,
             avgMicrosStr.c_str(),
             minMicrosStr.c_str(),
             maxMicrosStr.c_str());

    if (expectedIntervalMicros > 0.0) {
        mvprintw(row++, 4, "Load vs budget: %.1f%% current (avg %.1f%%, worst %.1f%%)",
                 currentLoad,
                 avgLoad,
                 worstLoad);
        mvprintw(row++, 4, "Slack to deadline: %s us current (worst %s us)",
                 formatFloat(currentSlack, 1, true).c_str(),
                 (maxMicros > 0)
                     ? formatFloat(worstSlack, 1, true).c_str()
                     : "n/a");
    }

    mvprintw(row++, 4, "Peak amplitude: %.4f (max %.4f)", currentPeak, maxPeak);
    mvprintw(row++, 4, "Active voices: %u (peak %u)", currentVoices, maxVoices);
    mvprintw(row++, 4, "Callbacks processed: %llu", static_cast<unsigned long long>(callbackCount));

    attron(COLOR_SECTION_HEADER);
    mvprintw(row++, 2, "Timing & Jitter");
    attroff(COLOR_SECTION_HEADER);

    std::string minIntervalStr = (minInterval == std::numeric_limits<uint32_t>::max())
                                     ? "n/a"
                                     : std::to_string(minInterval);
    std::string maxIntervalStr = (maxInterval == 0)
                                     ? "n/a"
                                     : std::to_string(maxInterval);
    double avgIntervalMicros = (callbackCount > 1 && totalInterval > 0)
                                   ? static_cast<double>(totalInterval) / static_cast<double>(callbackCount - 1)
                                   : 0.0;
    std::string avgIntervalStr = (callbackCount > 1 && totalInterval > 0)
                                     ? formatFloat(avgIntervalMicros, 1, false)
                                     : "n/a";

    mvprintw(row++, 4, "Interval: %u us (avg %s, min %s, max %s)",
             currentInterval,
             avgIntervalStr.c_str(),
             minIntervalStr.c_str(),
             maxIntervalStr.c_str());

    if (expectedIntervalMicros > 0.0) {
        double currentJitter = (currentInterval > 0)
                                   ? static_cast<double>(currentInterval) - expectedIntervalMicros
                                   : 0.0;
        double worstJitter = (maxInterval > 0)
                                 ? static_cast<double>(maxInterval) - expectedIntervalMicros
                                 : 0.0;

        mvprintw(row++, 4, "Jitter vs target: %s us current (worst %s us)",
                 (currentInterval > 0)
                     ? formatFloat(currentJitter, 1, true).c_str()
                     : "n/a",
                 (maxInterval > 0)
                     ? formatFloat(worstJitter, 1, true).c_str()
                     : "n/a");
    }

    attron(COLOR_SECTION_HEADER);
    mvprintw(row++, 2, "Scheduler & System");
    attroff(COLOR_SECTION_HEADER);

    auto schedulerPolicyName = [](int policy) -> const char* {
#ifdef SCHED_FIFO
        if (policy == SCHED_FIFO) return "SCHED_FIFO";
#endif
#ifdef SCHED_RR
        if (policy == SCHED_RR) return "SCHED_RR";
#endif
#ifdef SCHED_OTHER
        if (policy == SCHED_OTHER) return "SCHED_OTHER";
#endif
#ifdef SCHED_IDLE
        if (policy == SCHED_IDLE) return "SCHED_IDLE";
#endif
#ifdef SCHED_BATCH
        if (policy == SCHED_BATCH) return "SCHED_BATCH";
#endif
#ifdef SCHED_DEADLINE
        if (policy == SCHED_DEADLINE) return "SCHED_DEADLINE";
#endif
        return "UNKNOWN";
    };

    std::string policyStr = (schedulerPolicy >= 0)
                                ? schedulerPolicyName(schedulerPolicy)
                                : "UNKNOWN";
    bool isRealtime = false;
#ifdef SCHED_FIFO
    if (schedulerPolicy == SCHED_FIFO) isRealtime = true;
#endif
#ifdef SCHED_RR
    if (schedulerPolicy == SCHED_RR) isRealtime = true;
#endif
#ifdef SCHED_DEADLINE
    if (schedulerPolicy == SCHED_DEADLINE) isRealtime = true;
#endif

    std::string priorityStr = (schedulerPriority >= 0)
                                  ? std::to_string(schedulerPriority)
                                  : std::string("n/a");
    mvprintw(row++, 4, "Policy: %s (priority %s, realtime %s)",
             policyStr.c_str(),
             priorityStr.c_str(),
             isRealtime ? "YES" : "no");

    std::string callbackCpuStr = (callbackCpu >= 0)
                                     ? std::to_string(callbackCpu)
                                     : std::string("n/a");
    mvprintw(row++, 4, "Last audio core: %s", callbackCpuStr.c_str());

    float systemCpu = cpuMonitor.getCPUUsage();
    mvprintw(row++, 4, "System CPU usage: %.1f%%", systemCpu);
    mvprintw(row++, 4, "Output underruns: %llu", static_cast<unsigned long long>(getAudioUnderrunCount()));

    row += 1;
    attron(COLOR_HINT);
    mvprintw(row++, 2, "Ctrl+D - jump to this page   R - reset min/max statistics");
    mvprintw(row++, 2, "Use Tab/Shift+Tab to navigate pages (Debug follows Config).");
    attroff(COLOR_HINT);
}
