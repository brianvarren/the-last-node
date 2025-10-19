#include "brainwave_osc.h"
#include <algorithm>
#include <cmath>

// Classic Casio CZ style phase distortion for sawtooth with extended morph range
static float generatePhaseDistorted(float phase, float amount) {
    // phase is normalized [0, 1)
    // amount: 0.5 = sine wave (centered peak)
    // amount > 0.5 = peak shifts right
    // amount < 0.5 = peak shifts left (by inverting the waveform)

    float clamped = std::min(std::max(amount, 0.0f), 1.0f);
    bool invert = false;
    float morphAmount = clamped;

    // For morph 0.0-0.5, we invert and remap to 0.5-1.0 range
    if (clamped < 0.5f) {
        invert = true;
        morphAmount = 1.0f - clamped * 2.0f;  // 0.0 -> 1.0, 0.5 -> 0.0
        morphAmount = 0.5f + morphAmount * 0.5f;  // Map to 0.5-1.0
    }

    float pivot;
    if (morphAmount <= 0.5f) {
        float t = morphAmount * 2.0f;
        pivot = 0.5f - (0.5f - 0.0001f) * t;
    } else {
        float t = (morphAmount - 0.5f) * 2.0f;
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

    float output = -std::cos(shapedPhase * 2.0f * static_cast<float>(M_PI));
    return invert ? -output : output;
}

// Generate waveform with tanh-shaped pulse (morph 0.5 to 1.0)
static float generateTanhShaped(float phase, float morph, float duty) {
    // phase is normalized [0, 1)
    const float twoPi = 2.0f * M_PI;
    float sine = std::sin(twoPi * phase);

    // Map morph to edge parameter (0 = sine, 1 = hard square)
    float edge = std::min(std::max(morph, 0.0f), 1.0f);
    edge = std::min(std::max(edge, 0.0f), 1.0f);

    // When edge is very small, just return a pure sine wave.
    if (edge < 1e-3f) {
        return sine;
    }

    // Map duty to comparator bias
    float theta = twoPi * (duty - 0.5f);
    float x = sine - std::sin(theta);
    
    // Edge hardness control
    float beta = 1.0f + 80.0f * edge;
    
    float tanh_pulse = std::tanh(beta * x);

    // Crossfade from sine to tanh pulse based on edge to ensure smooth transition
    return (1.0f - edge) * sine + edge * tanh_pulse;
}

BrainwaveOscillator::BrainwaveOscillator()
    : mode_(BrainwaveMode::KEY)
    , baseFrequency_(440.0f)
    , noteFrequency_(440.0f)
    , morphPosition_(0.5f)
    , duty_(0.5f)  // Default to 50% duty (symmetric square)
    , shape_(BrainwaveShape::SAW)
    , ratio_(1.0f)
    , offsetHz_(0.0f)
    , fmSensitivity_(0.5f)  // Default FM sensitivity
    , phaseAccumulator_(0) {
}

float BrainwaveOscillator::calculateEffectiveFrequency(float sampleRate) {
    float freq = 0.0f;
    
    if (mode_ == BrainwaveMode::FREE) {
        // FREE mode: use user-defined frequency
        freq = baseFrequency_;
    } else {
        // KEY mode: use note-derived frequency
        freq = noteFrequency_;
    }

    // Apply FM8-style ratio and offset
    freq = freq * ratio_ + offsetHz_;

    // Prevent negative or zero frequency
    freq = std::max(freq, 0.01f);
    
    return freq;
}

float BrainwaveOscillator::generateSample(uint32_t phase, float morphPos) {
    // Convert 32-bit phase accumulator to normalized phase (0.0 to 1.0)
    float normalizedPhase = static_cast<double>(phase) / 4294967296.0;
    
    float morphAmount = std::min(std::max(morphPos, 0.0f), 1.0f);

    if (shape_ == BrainwaveShape::SAW) {
        return generatePhaseDistorted(normalizedPhase, morphAmount);
    }

    float shiftedPhase = normalizedPhase + 0.5f;
    if (shiftedPhase >= 1.0f) {
        shiftedPhase -= 1.0f;
    }
    return generateTanhShaped(shiftedPhase, morphAmount, duty_);
}

float BrainwaveOscillator::process(float sampleRate, float fmInput,
                                   float pitchMod, float morphMod, float dutyMod,
                                   float ratioMod, float offsetMod) {
    // Calculate base frequency (FREE or KEY mode)
    float freq = 0.0f;
    if (mode_ == BrainwaveMode::FREE) {
        freq = baseFrequency_;
    } else {
        freq = noteFrequency_;
    }

    // Apply pitch modulation as frequency multiplier (pitchMod is in octaves)
    // This applies AFTER mode selection but BEFORE ratio/offset
    float pitchMultiplier = std::pow(2.0f, pitchMod);
    freq *= pitchMultiplier;

    // Apply modulated ratio and offset
    float modulatedRatio = ratio_ + ratioMod;
    float modulatedOffset = offsetHz_ + offsetMod;
    freq = freq * modulatedRatio + modulatedOffset;

    // Prevent negative or zero frequency
    freq = std::max(freq, 0.01f);

    // Apply Through-Zero FM: modulate frequency directly
    // FM input is bipolar (-1 to +1), scaled by sensitivity
    // TZFM allows frequency to go negative (phase reversal)
    float fmAmount = fmInput * fmSensitivity_ * freq;  // Scale FM by current frequency
    float modulatedFreq = freq + fmAmount;

    // TZFM: Allow negative frequencies (through-zero)
    // Negative frequency = phase runs backward
    bool isNegative = modulatedFreq < 0.0f;
    float absFreq = std::abs(modulatedFreq);

    // Prevent extremely high frequencies that could cause aliasing
    absFreq = std::min(absFreq, sampleRate * 0.45f);  // Nyquist limit with margin

    // Calculate phase increment using double precision
    double phaseIncrementDouble = (static_cast<double>(absFreq) * 4294967296.0) / static_cast<double>(sampleRate);
    uint32_t phaseIncrement = static_cast<uint32_t>(phaseIncrementDouble);

    // Apply morph and duty modulation
    float modulatedMorph = std::min(std::max(morphPosition_ + morphMod, 0.0f), 1.0f);
    float modulatedDuty = std::min(std::max(duty_ + dutyMod, 0.0f), 1.0f);

    // Temporarily override duty_ for this sample (generateSample uses member variable)
    float savedDuty = duty_;
    duty_ = modulatedDuty;
    float sample = generateSample(phaseAccumulator_, modulatedMorph);
    duty_ = savedDuty;

    // Advance or reverse phase depending on frequency sign (TZFM)
    if (isNegative) {
        phaseAccumulator_ -= phaseIncrement;  // Reverse phase direction
    } else {
        phaseAccumulator_ += phaseIncrement;  // Normal phase advancement
    }

    return sample;
}
