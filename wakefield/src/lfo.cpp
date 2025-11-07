#include "lfo.h"
#include "fast_math.h"
#include "tanh_gain_lut.h"
#include <algorithm>
#include <cmath>

using namespace FastMath;

namespace {

constexpr float kPhaseToFloat = 1.0f / 4294967296.0f;
constexpr float kMaxBeta = 80.0f;
constexpr float kPulseMorphCurve = 5.0f;
constexpr float kPulseMorphSmoothTime = 0.005f; // ~5 ms glide per buffer

inline float lookupTanhGain(float morph) {
    float clamped = fastclamp(morph, 0.0f, 1.0f);
    float position = clamped * (kTanhGainLUTSize - 1);
    int index = static_cast<int>(position);
    int next = std::min(index + 1, kTanhGainLUTSize - 1);
    float frac = position - static_cast<float>(index);
    float a = kTanhGainLUT[index];
    float b = kTanhGainLUT[next];
    return a + (b - a) * frac;
}

inline float mapPulseMorph(float morph) {
    float clamped = fastclamp(morph, 0.0f, 1.0f);
    const float denom = 1.0f - std::exp(-kPulseMorphCurve);
    if (denom <= 0.0f) return clamped;
    return (1.0f - std::exp(-kPulseMorphCurve * clamped)) / denom;
}

} // namespace

// Phase distortion saw (same as brainwave osc)
float LFO::generatePhaseDistorted(float phase, float morph) {
    float clamped = fastclamp(morph, 0.0f, 1.0f);
    bool mirror = false;
    float morphAmount = clamped;

    if (clamped < 0.5f) {
        mirror = true;
        morphAmount = 1.0f - clamped * 2.0f;
        morphAmount = 0.5f + morphAmount * 0.5f;
    }

    float t = (morphAmount - 0.5f) * 2.0f;
    float pivot = 0.5f + 0.4999f * t;
    pivot = fastclamp(pivot, 0.0001f, 0.9999f);

    float workingPhase = mirror ? (1.0f - phase) : phase;
    float shapedPhase;
    if (workingPhase <= pivot) {
        float denom = std::max(2.0f * pivot, 1e-6f);
        shapedPhase = workingPhase / denom;
    } else {
        float denom = std::max(1.0f - pivot, 1e-6f);
        shapedPhase = 0.5f * (1.0f + ((workingPhase - pivot) / denom));
    }

    return -fastcos(shapedPhase * kTwoPi);
}

// Tanh-shaped pulse (same as brainwave osc)
float LFO::generateTanhShaped(float phase, float morph, float duty) {
    float edge = fastclamp(morph, 0.0f, 1.0f);
    if (edge < 1e-4f) {
        return fastsin(kTwoPi * phase);
    }

    float sine = fastsin(kTwoPi * phase);
    float theta = kTwoPi * (duty - 0.5f);
    float x = sine - fastsin(theta);
    float beta = 1.0f + edge * kMaxBeta;
    float tanhPulse = fasttanh(beta * x);
    return lookupTanhGain(edge) * tanhPulse;
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
    , currentOutput_(0.0f)
    , pulseMorphState_(mapPulseMorph(0.5f)) {
}

float LFO::calculateFrequency(float sampleRate) {
    if (syncMode_ == LFOSyncMode::OFF) {
        return std::max(period_ > 0.0001f ? 1.0f / period_ : 1.0f, 0.001f);
    }

    float beatsPerSecond = tempo_ / 60.0f;
    switch (syncMode_) {
        case LFOSyncMode::ON:      return beatsPerSecond;
        case LFOSyncMode::TRIPLET: return beatsPerSecond * 1.5f;
        case LFOSyncMode::DOTTED:  return beatsPerSecond * (2.0f / 3.0f);
        default:                   return beatsPerSecond;
    }
}

float LFO::generateSample(uint32_t phase) {
    float floatPhase = static_cast<float>(phase) * kPhaseToFloat;

    float sample;
    if (shape_ == 0) {
        sample = generatePhaseDistorted(floatPhase, morphPosition_);
    } else {
        float shiftedPhase = floatPhase + 0.5f;
        if (shiftedPhase >= 1.0f) shiftedPhase -= 1.0f;
        sample = generateTanhShaped(shiftedPhase, pulseMorphState_, duty_);
    }

    if (flipPolarity_) sample = -sample;
    return sample;
}

float LFO::process(float sampleRate, unsigned int nFrames) {
    float frequency = calculateFrequency(sampleRate);
    double phaseIncrementDouble = (frequency / sampleRate) * 4294967296.0;
    uint32_t phaseIncrement = static_cast<uint32_t>(phaseIncrementDouble);

    if (shape_ == 1) {
        float dt = (sampleRate > 0.0f) ? (static_cast<float>(nFrames) / sampleRate) : 0.0f;
        float alpha = 1.0f - std::exp(-dt / kPulseMorphSmoothTime);
        alpha = std::clamp(alpha, 0.0f, 1.0f);
        float target = mapPulseMorph(morphPosition_);
        pulseMorphState_ += alpha * (target - pulseMorphState_);
    }

    currentOutput_ = generateSample(phaseAccumulator_);
    phaseAccumulator_ += phaseIncrement * nFrames;
    return currentOutput_;
}

void LFO::reset() {
    phaseAccumulator_ = 0;
    currentOutput_ = 0.0f;
    pulseMorphState_ = mapPulseMorph(morphPosition_);
}

float LFO::getPhase() const {
    return static_cast<double>(phaseAccumulator_) * kPhaseToFloat;
}

void LFO::setMorph(float morph) {
    morphPosition_ = std::clamp(morph, 0.0f, 1.0f);
    pulseMorphState_ = mapPulseMorph(morphPosition_);
}
