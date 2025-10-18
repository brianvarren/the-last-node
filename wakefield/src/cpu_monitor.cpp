#include "cpu_monitor.h"
#include <fstream>
#include <sstream>

CPUMonitor::CPUMonitor()
    : running(false)
    , enabled(true)  // Enabled by default
    , currentUsage(0.0f) {
}

CPUMonitor::~CPUMonitor() {
    stop();
}

void CPUMonitor::start() {
    if (running.load()) {
        return;  // Already running
    }

    running = true;
    monitorThread = std::thread(&CPUMonitor::monitorLoop, this);
}

void CPUMonitor::stop() {
    if (!running.load()) {
        return;  // Not running
    }

    running = false;
    if (monitorThread.joinable()) {
        monitorThread.join();
    }
}

float CPUMonitor::getCPUUsage() const {
    return currentUsage.load();
}

void CPUMonitor::setEnabled(bool enable) {
    enabled = enable;
}

bool CPUMonitor::readCPUStats(CPUStats& stats) {
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    if (!std::getline(file, line)) {
        return false;
    }

    // Parse first line which should be "cpu  user nice system idle iowait irq softirq steal ..."
    std::istringstream ss(line);
    std::string cpu_label;
    ss >> cpu_label;

    if (cpu_label != "cpu") {
        return false;
    }

    stats.user = 0;
    stats.nice = 0;
    stats.system = 0;
    stats.idle = 0;
    stats.iowait = 0;
    stats.irq = 0;
    stats.softirq = 0;
    stats.steal = 0;

    ss >> stats.user >> stats.nice >> stats.system >> stats.idle
       >> stats.iowait >> stats.irq >> stats.softirq >> stats.steal;

    return true;
}

float CPUMonitor::calculateUsage(const CPUStats& prev, const CPUStats& current) {
    // Calculate total time difference
    unsigned long long prevIdle = prev.idle + prev.iowait;
    unsigned long long currIdle = current.idle + current.iowait;

    unsigned long long prevTotal = prev.user + prev.nice + prev.system + prev.idle +
                                   prev.iowait + prev.irq + prev.softirq + prev.steal;
    unsigned long long currTotal = current.user + current.nice + current.system + current.idle +
                                   current.iowait + current.irq + current.softirq + current.steal;

    unsigned long long totalDiff = currTotal - prevTotal;
    unsigned long long idleDiff = currIdle - prevIdle;

    if (totalDiff == 0) {
        return 0.0f;
    }

    // Calculate usage percentage
    float usage = 100.0f * (1.0f - static_cast<float>(idleDiff) / static_cast<float>(totalDiff));

    // Clamp to valid range
    if (usage < 0.0f) usage = 0.0f;
    if (usage > 100.0f) usage = 100.0f;

    return usage;
}

void CPUMonitor::monitorLoop() {
    CPUStats prevStats;
    if (!readCPUStats(prevStats)) {
        return;  // Failed to read initial stats
    }

    while (running.load()) {
        // Wait for 500ms between samples
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        if (!enabled.load()) {
            currentUsage = 0.0f;
            continue;
        }

        CPUStats currentStats;
        if (readCPUStats(currentStats)) {
            float usage = calculateUsage(prevStats, currentStats);
            currentUsage = usage;
            prevStats = currentStats;
        }
    }
}
