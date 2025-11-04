#include "brainwave_osc.h"
#include "fast_math.h"
#include <algorithm>
#include <cmath>

#if USE_WAVETABLE_OSCILLATORS
#include "wavetable.h"
#endif

using namespace FastMath;

// Classic Casio CZ style phase distortion for sawtooth with extended morph range
static inline __attribute__((always_inline)) float generatePhaseDistorted(float phase, float amount) {
    // phase is normalized [0, 1)
    // amount: 0.0 = sawtooth pointing top-left (peak at left)
    // amount: 0.5 = sine wave (centered peak)
    // amount: 1.0 = sawtooth pointing top-right (peak at right)
    // Creates visually symmetrical morphing

    float clamped = std::min(std::max(amount, 0.0f), 1.0f);
    bool mirror = false;
    float morphAmount = clamped;

    // For morph 0.0-0.5, we mirror the phase and remap to 0.5-1.0 range
    if (clamped < 0.5f) {
        mirror = true;
        morphAmount = 1.0f - clamped * 2.0f;  // 0.0 -> 1.0, 0.5 -> 0.0
        morphAmount = 0.5f + morphAmount * 0.5f;  // Map to 0.5-1.0
    }

    // After remapping, morphAmount is always in range [0.5, 1.0]
    // morphAmount 0.5 -> pivot 0.5 (sine wave)
    // morphAmount 1.0 -> pivot 0.9999 (sharp sawtooth)
    float t = (morphAmount - 0.5f) * 2.0f;  // 0 at morphAmount=0.5, 1 at morphAmount=1.0
    float pivot = 0.5f + 0.4999f * t;
    pivot = std::min(std::max(pivot, 0.0001f), 0.9999f);

    // Mirror phase for left-side morphing (flip horizontally)
    float workingPhase = mirror ? (1.0f - phase) : phase;

    float shapedPhase;
    if (workingPhase <= pivot) {
        float denom = std::max(1e-6f, 2.0f * pivot);
        shapedPhase = workingPhase / denom;
    } else {
        float denom = std::max(1e-6f, 1.0f - pivot);
        shapedPhase = 0.5f * (1.0f + ((workingPhase - pivot) / denom));
    }

    // No vertical inversion - just horizontal mirroring via phase
    float output = -fastcos(shapedPhase * kTwoPi);
    return output;
}

// Generate waveform with tanh-shaped pulse (morph 0.5 to 1.0)
static inline __attribute__((always_inline)) float generateTanhShaped(float phase, float morph, float duty) {
    // phase is normalized [0, 1)
    float sine = fastsin(kTwoPi * phase);

    // Map morph to edge parameter (0 = sine, 1 = hard square)
    // Remove redundant double clamp
    float edge = fastclamp(morph, 0.0f, 1.0f);

    // Branchless early exit: blend instead of if-statement
    // When edge is very small, output ~= sine
    float theta = kTwoPi * (duty - 0.5f);
    float x = sine - fastsin(theta);

    // Edge hardness control
    float beta = 1.0f + 80.0f * edge;

    float tanh_pulse = fasttanh(beta * x);

    // Crossfade from sine to tanh pulse based on edge
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

float BrainwaveOscillator::generateSample(uint32_t phase, float morphPos, float duty, float frequency) {
#if USE_WAVETABLE_OSCILLATORS
    // Use wavetable lookup - much faster than computing trig functions
    auto& bank = Wavetable::WavetableBank::getInstance();

    if (bank.isInitialized()) {
        float morphAmount = fastclamp(morphPos, 0.0f, 1.0f);
        float dutyAmount = fastclamp(duty, 0.0f, 1.0f);

        if (shape_ == BrainwaveShape::SAW) {
            return bank.lookupSaw(phase, morphAmount, frequency);
        } else {
            // Pulse needs phase shift
            uint32_t shiftedPhase = phase + 0x80000000;  // Add 180 degrees
            return bank.lookupPulse(shiftedPhase, morphAmount, dutyAmount, frequency);
        }
    }
    // Fall through to computed version if not initialized
#endif

    // Computed version (fallback or when USE_WAVETABLE_OSCILLATORS=0)
    constexpr float kPhaseToFloat = 1.0f / 4294967296.0f;
    float normalizedPhase = static_cast<float>(phase) * kPhaseToFloat;

    float morphAmount = fastclamp(morphPos, 0.0f, 1.0f);

    if (shape_ == BrainwaveShape::SAW) {
        return generatePhaseDistorted(normalizedPhase, morphAmount);
    }

    // Phase shift with branchless wrap
    float shiftedPhase = normalizedPhase + 0.5f;
    shiftedPhase -= (shiftedPhase >= 1.0f) ? 1.0f : 0.0f;

    return generateTanhShaped(shiftedPhase, morphAmount, duty);
}

float BrainwaveOscillator::process(float sampleRate, float fmInput,
                                   float pitchMod, float morphMod, float dutyMod,
                                   float ratioMod, float offsetMod) {
    // Pre-computed constants
    constexpr float kPhaseScale = 4294967296.0f;
    const float nyquistLimit = sampleRate * 0.45f;

    // Calculate base frequency (FREE or KEY mode)
    // Reorder: KEY mode is more common, check it first
    float freq = (mode_ == BrainwaveMode::KEY) ? noteFrequency_ : baseFrequency_;

    // Apply pitch modulation - use std::pow for rock-solid pitch accuracy
    // pitchMod is in octaves: 1.0 = double frequency, -1.0 = half frequency
    // Pitch perception is extremely sensitive, no approximations allowed
    freq *= std::pow(2.0f, pitchMod);

    // Apply modulated ratio and offset with FMA (fused multiply-add)
    float modulatedRatio = ratio_ + ratioMod;
    float modulatedOffset = offsetHz_ + offsetMod;
    freq = std::fma(freq, modulatedRatio, modulatedOffset);

    // Prevent negative or zero frequency
    freq = fastmax(freq, 0.01f);

    // TZFM: Through-Zero FM (full implementation, optimized)
    // FM input is bipolar (-1 to +1), scaled by sensitivity
    // Combine operations: freq * (1 + fmInput * sensitivity)
    float modulatedFreq = std::fma(fmInput * fmSensitivity_, freq, freq);

    // Branchless abs and sign extraction for TZFM
    int32_t freqBits = *reinterpret_cast<int32_t*>(&modulatedFreq);
    int32_t signBit = freqBits >> 31;  // Extract sign: 0xFFFFFFFF if negative, 0 if positive
    float absFreq = fastabs(modulatedFreq);

    // Nyquist limiting
    absFreq = fastmin(absFreq, nyquistLimit);

    // Calculate phase increment (stay in float, sufficient precision)
    float phaseIncrementFloat = (absFreq * kPhaseScale) / sampleRate;
    uint32_t phaseIncrement = static_cast<uint32_t>(phaseIncrementFloat);

    // Apply morph and duty modulation (single clamp instead of min/max)
    float modulatedMorph = fastclamp(morphPosition_ + morphMod, 0.0f, 1.0f);
    float modulatedDuty = fastclamp(duty_ + dutyMod, 0.0f, 1.0f);

    // Generate sample (pass current frequency to support wavetable path)
    float sample = generateSample(phaseAccumulator_, modulatedMorph, modulatedDuty, absFreq);

    // Branchless phase update for TZFM
    // If negative (signBit = 0xFFFFFFFF), negate the increment
    // If positive (signBit = 0), keep increment as-is
    int32_t signedIncrement = (int32_t)phaseIncrement ^ signBit;
    signedIncrement = signedIncrement - signBit;  // Two's complement if negative
    phaseAccumulator_ = (uint32_t)((int32_t)phaseAccumulator_ + signedIncrement);

    return sample;
}
