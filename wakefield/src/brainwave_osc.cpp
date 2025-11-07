#include "brainwave_osc.h"
#include "fast_math.h"
#include "osc_profiler.h"
#include "tanh_gain_lut.h"
#include <algorithm>
#include <cmath>

using namespace FastMath;

// Global profiler instance
OscProfiler g_oscProfiler;

namespace {

constexpr float kPhaseToFloat = 1.0f / 4294967296.0f;
constexpr float kMinPivot = 0.0001f;
constexpr float kMaxPivot = 0.9999f;
constexpr float kMaxBeta = 80.0f;
constexpr float kPulseMorphExponent = 2.5f;
constexpr float kPulseMorphSmoothTime = 0.003f; // ~3ms

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

inline float generatePhaseDistorted(float phase, float morph) {
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
    pivot = fastclamp(pivot, kMinPivot, kMaxPivot);

    float workingPhase = mirror ? (1.0f - phase) : phase;
    float shapedPhase;
    if (workingPhase <= pivot) {
        float denom = std::max(2.0f * pivot, kMinPivot);
        shapedPhase = workingPhase / denom;
    } else {
        float denom = std::max(1.0f - pivot, kMinPivot);
        shapedPhase = 0.5f * (1.0f + ((workingPhase - pivot) / denom));
    }

    return -fastcos(shapedPhase * kTwoPi);
}

inline float generateTanhSquare(float phase, float morph) {
    float edge = fastclamp(morph, 0.0f, 1.0f);
    if (edge <= 1e-6f) {
        return fastsin(kTwoPi * phase);
    }

    float sine = fastsin(kTwoPi * phase);
    float beta = 1.0f + edge * kMaxBeta;
    float saturated = fasttanh(beta * sine);
    return saturated * lookupTanhGain(edge);
}

} // namespace

static inline float mapPulseMorph(float morph) {
    float clamped = fastclamp(morph, 0.0f, 1.0f);
    return std::pow(clamped, kPulseMorphExponent);
}

BrainwaveOscillator::BrainwaveOscillator()
    : mode_(BrainwaveMode::KEY)
    , baseFrequency_(440.0f)
    , noteFrequency_(440.0f)
    , shape_(BrainwaveShape::SAW)
    , morph_(0.5f)
    , morphState_(0.5f)
    , ratio_(1.0f)
    , offsetHz_(0.0f)
    , fmSensitivity_(0.5f)
    , phaseAccumulator_(0) {
}

void BrainwaveOscillator::setShape(int shapeIndex) {
    int clamped = std::clamp(shapeIndex, 0, 1);
    shape_ = static_cast<BrainwaveShape>(clamped);
}

void BrainwaveOscillator::setMorph(float morph) {
    morph_ = fastclamp(morph, 0.0f, 1.0f);
    morphState_ = morph_;
}

float BrainwaveOscillator::process(float sampleRate, float fmInput,
                                   float pitchMod, float morphMod,
                                   float ratioMod, float offsetMod) {
    constexpr float kPhaseScale = 4294967296.0f;
    const float nyquistLimit = sampleRate * 0.45f;

    float freq = (mode_ == BrainwaveMode::KEY) ? noteFrequency_ : baseFrequency_;
    freq *= std::pow(2.0f, pitchMod);

    float modulatedRatio = ratio_ + ratioMod;
    float modulatedOffset = offsetHz_ + offsetMod;
    freq = std::fma(freq, modulatedRatio, modulatedOffset);
    freq = fastmax(freq, 0.01f);

    float modulatedFreq = std::fma(fmInput * fmSensitivity_, freq, freq);
    int32_t freqBits = *reinterpret_cast<int32_t*>(&modulatedFreq);
    int32_t signBit = freqBits >> 31;
    float absFreq = fastmin(fastabs(modulatedFreq), nyquistLimit);

    float phaseIncrementFloat = (absFreq * kPhaseScale) / sampleRate;
    uint32_t phaseIncrement = static_cast<uint32_t>(phaseIncrementFloat);

    float normalizedPhase = static_cast<float>(phaseAccumulator_) * kPhaseToFloat;
    float morphTarget = fastclamp(morph_ + morphMod, 0.0f, 1.0f);
    float alpha = 1.0f - std::exp(-1.0f / (std::max(sampleRate, 1.0f) * kPulseMorphSmoothTime));
    morphState_ += alpha * (morphTarget - morphState_);

    float sample;
    if (shape_ == BrainwaveShape::SAW) {
        sample = generatePhaseDistorted(normalizedPhase, morphState_);
    } else {
        float pulseControl = mapPulseMorph(morphState_);
        sample = generateTanhSquare(normalizedPhase, pulseControl);
    }

    int32_t signedIncrement = static_cast<int32_t>(phaseIncrement) ^ signBit;
    signedIncrement -= signBit;
    phaseAccumulator_ = static_cast<uint32_t>(static_cast<int32_t>(phaseAccumulator_) + signedIncrement);

    return sample;
}
