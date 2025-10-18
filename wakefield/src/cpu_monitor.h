#ifndef CPU_MONITOR_H
#define CPU_MONITOR_H

#include <atomic>
#include <thread>
#include <chrono>
#include <string>

// CPU usage monitoring for Linux systems
class CPUMonitor {
public:
    CPUMonitor();
    ~CPUMonitor();

    // Start monitoring in background thread
    void start();

    // Stop monitoring
    void stop();

    // Get current CPU usage percentage (0-100)
    float getCPUUsage() const;

    // Check if monitoring is enabled
    bool isEnabled() const { return enabled.load(); }

    // Enable/disable monitoring
    void setEnabled(bool enable);

private:
    std::atomic<bool> running;
    std::atomic<bool> enabled;
    std::atomic<float> currentUsage;
    std::thread monitorThread;

    // Read CPU stats from /proc/stat
    struct CPUStats {
        unsigned long long user;
        unsigned long long nice;
        unsigned long long system;
        unsigned long long idle;
        unsigned long long iowait;
        unsigned long long irq;
        unsigned long long softirq;
        unsigned long long steal;
    };

    bool readCPUStats(CPUStats& stats);
    float calculateUsage(const CPUStats& prev, const CPUStats& current);
    void monitorLoop();
};

#endif // CPU_MONITOR_H
