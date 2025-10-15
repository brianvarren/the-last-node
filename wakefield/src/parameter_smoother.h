#ifndef PARAMETER_SMOOTHER_H
#define PARAMETER_SMOOTHER_H

#include <cmath>

// Simple one-pole lowpass filter for parameter smoothing
// Provides smooth transitions without zipper noise
class ParameterSmoother {
public:
    ParameterSmoother(float smoothTime = 0.01f, float sampleRate = 100.0f)
        : currentValue(0.0f)
        , targetValue(0.0f) {
        setSmoothTime(smoothTime, sampleRate);
    }

    // Set the smoothing time constant (in seconds)
    // Typical values: 0.005 - 0.02 seconds (5-20ms)
    void setSmoothTime(float timeSeconds, float sampleRate) {
        // Calculate one-pole filter coefficient
        // coefficient = 1 - exp(-2π * cutoff_freq * dt)
        // For smoothTime, we use cutoff = 1/(2π * smoothTime)
        float dt = 1.0f / sampleRate;
        coefficient = 1.0f - std::exp(-1.0f / (timeSeconds * sampleRate));
    }

    // Set target value (what the parameter should move towards)
    void setTarget(float target) {
        targetValue = target;
    }

    // Reset to a specific value immediately (no smoothing)
    void reset(float value) {
        currentValue = value;
        targetValue = value;
    }

    // Process one sample - returns smoothed value
    float process() {
        currentValue += coefficient * (targetValue - currentValue);
        return currentValue;
    }

    // Get current smoothed value without advancing
    float getCurrentValue() const {
        return currentValue;
    }

    // Check if we're close enough to target (within 0.1%)
    bool isSettled() const {
        return std::abs(targetValue - currentValue) < 0.001f * std::abs(targetValue);
    }

private:
    float currentValue;
    float targetValue;
    float coefficient;
};

#endif // PARAMETER_SMOOTHER_H
