#include "brainwave_osc.h"
#include "fast_math.h"
#include "osc_profiler.h"
#include <algorithm>
#include <cmath>

using namespace FastMath;

// Global profiler instance
OscProfiler g_oscProfiler;

// PolyBLEP anti-aliasing for band-limited waveforms
// Applies correction near discontinuities to reduce aliasing
static inline __attribute__((always_inline)) float polyblep(float phase, float phaseInc) {
    // Only apply correction near discontinuities (within one sample of wraparound)
    if (phase < phaseInc) {
        float t = phase / phaseInc;
        return t + t - t * t - 1.0f;
    } else if (phase > 1.0f - phaseInc) {
        float t = (phase - 1.0f) / phaseInc;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// Generate sine wave (no aliasing, pure fundamental)
static inline __attribute__((always_inline)) float generateSine(float phase, float morph) {
    // Morph controls waveshaping amount (subtle harmonic coloring)
    float sine = fastsin(kTwoPi * phase);

    // Apply soft waveshaping when morph > 0
    // morph 0.0 = pure sine, morph 1.0 = shaped/saturated sine
    float shaped = sine + 0.3f * morph * sine * sine * sine;
    return fastclamp(shaped, -1.0f, 1.0f);
}

// Generate triangle wave with PolyBLEP
static inline __attribute__((always_inline)) float generateTriangle(float phase, float phaseInc, float morph) {
    // Morph controls slope asymmetry
    // morph 0.0 = ramp down, morph 0.5 = symmetric triangle, morph 1.0 = ramp up
    float pivot = 0.5f * (1.0f - morph) + morph * morph;  // Smooth transition

    float tri;
    if (phase < pivot) {
        tri = phase / pivot;
    } else {
        tri = 1.0f - (phase - pivot) / (1.0f - pivot);
    }
    tri = 2.0f * tri - 1.0f;

    // PolyBLEP correction at pivot point for anti-aliasing
    float correction = 0.0f;
    if (phase < phaseInc) {
        correction = polyblep(phase, phaseInc);
    } else if (phase > 1.0f - phaseInc) {
        correction = polyblep(phase, phaseInc);
    }

    return tri - 0.5f * correction;
}

// Generate sawtooth wave with PolyBLEP
static inline __attribute__((always_inline)) float generateSaw(float phase, float phaseInc, float morph) {
    // Basic sawtooth - upward ramp
    // Morph parameter unused for now (could control waveshaping later)
    (void)morph;  // Suppress unused parameter warning

    float saw = 2.0f * phase - 1.0f;
    saw -= polyblep(phase, phaseInc);

    return saw;
}

// Generate square/pulse wave with PolyBLEP
static inline __attribute__((always_inline)) float generateSquare(float phase, float phaseInc, float duty) {
    // duty controls pulse width (0.0 = narrow pulse, 0.5 = square, 1.0 = inverted narrow)
    float square = (phase < duty) ? 1.0f : -1.0f;

    // Apply PolyBLEP at both edges
    square += polyblep(phase, phaseInc);

    float phaseShifted = phase + (1.0f - duty);
    if (phaseShifted >= 1.0f) phaseShifted -= 1.0f;
    square -= polyblep(phaseShifted, phaseInc);

    return square;
}

BrainwaveOscillator::BrainwaveOscillator()
    : mode_(BrainwaveMode::KEY)
    , baseFrequency_(440.0f)
    , noteFrequency_(440.0f)
    , shape_(0.5f)  // Default to Saw (0.4-0.6 range)
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

float BrainwaveOscillator::generateSample(uint32_t phase, float phaseInc, float shapePos) {
    constexpr float kPhaseToFloat = 1.0f / 4294967296.0f;
    float normalizedPhase = static_cast<float>(phase) * kPhaseToFloat;

    // 5-shape continuum: Sine → Triangle → Saw → Square → Pulse
    // Shape regions: 0.0-0.2 (Sine), 0.2-0.4 (Tri), 0.4-0.6 (Saw), 0.6-0.8 (Square), 0.8-1.0 (Pulse)

    if (shapePos < 0.2f) {
        // Sine wave - use shapePos/0.2 for subtle waveshaping
        float waveshape = shapePos / 0.2f;
        return generateSine(normalizedPhase, waveshape);
    } else if (shapePos < 0.4f) {
        // Triangle - use normalized position for slope asymmetry
        float asymmetry = (shapePos - 0.2f) / 0.2f;
        return generateTriangle(normalizedPhase, phaseInc, asymmetry);
    } else if (shapePos < 0.6f) {
        // Sawtooth (no morph variation)
        return generateSaw(normalizedPhase, phaseInc, 0.0f);
    } else if (shapePos < 0.8f) {
        // Square wave (50% duty)
        return generateSquare(normalizedPhase, phaseInc, 0.5f);
    } else {
        // Pulse - duty narrows from 50% to ~5% as shapePos goes 0.8 → 1.0
        float pulseAmount = (shapePos - 0.8f) / 0.2f;  // 0.0 at 0.8, 1.0 at 1.0
        float duty = 0.5f - 0.45f * pulseAmount;  // 0.5 at shape=0.8, 0.05 at shape=1.0
        return generateSquare(normalizedPhase, phaseInc, duty);
    }
}

float BrainwaveOscillator::process(float sampleRate, float fmInput,
                                   float pitchMod, float morphMod, float dutyMod,
                                   float ratioMod, float offsetMod) {
    // Pre-computed constants
    constexpr float kPhaseScale = 4294967296.0f;
    const float nyquistLimit = sampleRate * 0.45f;

    // Calculate base frequency
    // KEY mode: Use MIDI note frequency directly
    // FREE mode: Use MIDI note frequency + user-defined frequency offset
    float freq = noteFrequency_;
    if (mode_ == BrainwaveMode::FREE) {
        freq += baseFrequency_;
    }

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

    // Morph and duty parameters no longer used (shape is now the single control)
    (void)morphMod;
    (void)dutyMod;

    // Calculate normalized phase increment for PolyBLEP (0.0 to 1.0 range)
    float normalizedPhaseInc = phaseIncrementFloat / kPhaseScale;

    // Generate sample with 5-shape continuum (shape is the only control)
    float sample = generateSample(phaseAccumulator_, normalizedPhaseInc, shape_);

    // Branchless phase update for TZFM
    // If negative (signBit = 0xFFFFFFFF), negate the increment
    // If positive (signBit = 0), keep increment as-is
    int32_t signedIncrement = (int32_t)phaseIncrement ^ signBit;
    signedIncrement = signedIncrement - signBit;  // Two's complement if negative
    phaseAccumulator_ = (uint32_t)((int32_t)phaseAccumulator_ + signedIncrement);

    return sample;
}
