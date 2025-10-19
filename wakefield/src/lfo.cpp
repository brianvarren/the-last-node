#include "lfo.h"
#include <algorithm>
#include <cmath>

// Phase distortion saw (same as brainwave osc)
float LFO::generatePhaseDistorted(float phase, float morph) {
    // phase is normalized [0, 1)
    float clamped = std::min(std::max(morph, 0.0f), 1.0f);
    float pivot;
    if (clamped <= 0.5f) {
        float t = clamped * 2.0f;
        pivot = 0.5f - (0.5f - 0.0001f) * t;
    } else {
        float t = (clamped - 0.5f) * 2.0f;
        pivot = 0.5f + 0.4999f * t;
    }
    pivot = std::min(std::max(pivot, 0.0001f), 0.9999f);

    float shapedPhase;
    if (phase <= pivot) {
        float denom = std::max(1e-6f, 2.0f * pivot);
        shapedPhase = phase / denom;
    } else {
        float denom = std::max(1e-6f, 1.0f - pivot);
        shapedPhase = 0.5f * (1.0f + ((phase - pivot) / denom));
    }

    return -std::cos(shapedPhase * 2.0f * static_cast<float>(M_PI));
}

// Tanh-shaped pulse (same as brainwave osc)
float LFO::generateTanhShaped(float phase, float morph, float duty) {
    const float twoPi = 2.0f * static_cast<float>(M_PI);
    float sine = std::sin(twoPi * phase);

    float edge = std::min(std::max(morph, 0.0f), 1.0f);

    if (edge < 1e-3f) {
        return sine;
    }

    float theta = twoPi * (duty - 0.5f);
    float x = sine - std::sin(theta);
    float beta = 1.0f + 80.0f * edge;
    float tanhPulse = std::tanh(beta * x);

    return (1.0f - edge) * sine + edge * tanhPulse;
}

LFO::LFO()
    : period_(3.0f)
    , syncMode_(LFOSyncMode::OFF)
    , shape_(0)
    , tempo_(120.0f)
    , morphPosition_(0.5f)
    , duty_(0.5f)
    , flipPolarity_(false)
    , resetOnNote_(false)
    , phaseAccumulator_(0)
    , currentOutput_(0.0f) {
}

float LFO::calculateFrequency(float sampleRate) {
    float freq = 0.0f;

    if (syncMode_ == LFOSyncMode::OFF) {
        // Free-running mode: use period directly
        if (period_ > 0.0001f) {
            freq = 1.0f / period_;
        } else {
            freq = 1.0f;
        }
    } else {
        // Tempo-synced mode
        float beatsPerSecond = tempo_ / 60.0f;

        switch (syncMode_) {
            case LFOSyncMode::ON:
                // Straight sync (1 cycle per beat)
                freq = beatsPerSecond;
                break;
            case LFOSyncMode::TRIPLET:
                // Triplet sync (1.5x faster, 3 cycles per 2 beats)
                freq = beatsPerSecond * 1.5f;
                break;
            case LFOSyncMode::DOTTED:
                // Dotted sync (0.666x slower, 2 cycles per 3 beats)
                freq = beatsPerSecond * (2.0f / 3.0f);
                break;
            default:
                freq = beatsPerSecond;
                break;
        }
    }

    return std::max(freq, 0.001f);
}

float LFO::generateSample(uint32_t phase) {
    // Convert 32-bit phase to float 0.0-1.0
    float floatPhase = static_cast<float>(phase) / static_cast<float>(0xFFFFFFFF);

    float sample;
    if (shape_ == 0) {
        // Phase-distorted sine wave
        sample = generatePhaseDistorted(floatPhase, morphPosition_);
    } else {
        // Tanh-shaped pulse wave (with phase shift to match oscillator)
        float shiftedPhase = floatPhase + 0.5f;
        if (shiftedPhase >= 1.0f) {
            shiftedPhase -= 1.0f;
        }
        sample = generateTanhShaped(shiftedPhase, morphPosition_, duty_);
    }

    if (flipPolarity_) {
        sample = -sample;
    }

    return sample;
}

float LFO::process(float sampleRate) {
    // Calculate frequency based on mode
    float frequency = calculateFrequency(sampleRate);

    // Calculate phase increment (32-bit unsigned for high precision)
    double phaseIncrementDouble = (frequency / sampleRate) * 4294967296.0;
    uint32_t phaseIncrement = static_cast<uint32_t>(phaseIncrementDouble);

    // Generate sample at current phase
    currentOutput_ = generateSample(phaseAccumulator_);

    // Advance phase
    phaseAccumulator_ += phaseIncrement;

    return currentOutput_;
}

void LFO::reset() {
    phaseAccumulator_ = 0;
    currentOutput_ = 0.0f;
}

float LFO::getPhase() const {
    return static_cast<double>(phaseAccumulator_) / 4294967296.0;
}
