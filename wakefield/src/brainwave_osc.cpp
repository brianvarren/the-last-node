#include "brainwave_osc.h"
#include <algorithm>
#include <cmath>

// Classic Casio CZ style phase distortion for sawtooth
// morph: 0.0 (saw) to 0.5 (sine)
static float generatePhaseDistorted(float phase, float morph) {
    // phase is normalized [0, 1)
    
    // map morph to alpha
    // morph=0   -> alpha=1 (saw)
    // morph=0.5 -> alpha=0.5 (sine)
    float alpha = 1.0f - morph;
    alpha = std::min(std::max(alpha, 0.001f), 0.999f);

    float warped_phase;
    if (phase < alpha) {
        warped_phase = phase * 0.5f / alpha;
    } else {
        warped_phase = 0.5f + (phase - alpha) * 0.5f / (1.0f - alpha);
    }
    
    return std::sin(warped_phase * 2.0f * M_PI);
}

// Generate waveform with tanh-shaped pulse (morph 0.5 to 1.0)
static float generateTanhShaped(float phase, float morph, float duty) {
    // phase is normalized [0, 1)
    const float twoPi = 2.0f * M_PI;
    float sine = std::sin(twoPi * phase);

    // Map morph to edge parameter
    // morph = 0.5 → edge = 0.0 (pure sine)
    // morph = 1.0 → edge = 1.0 (hard square)
    float edge = (morph - 0.5f) * 2.0f;
    edge = std::min(std::max(edge, 0.0f), 1.0f);

    // When edge is very small, just return a pure sine wave.
    if (edge < 1e-3f) {
        return sine;
    }

    // Map duty to comparator bias
    float theta = twoPi * (duty - 0.5f);
    float x = sine - std::sin(theta);
    
    // Edge hardness control
    float beta = 1.0f + 40.0f * edge;
    
    float tanh_pulse = std::tanh(beta * x);

    // Crossfade from sine to tanh pulse based on edge to ensure smooth transition
    return (1.0f - edge) * sine + edge * tanh_pulse;
}

BrainwaveOscillator::BrainwaveOscillator()
    : mode_(BrainwaveMode::FREE)
    , baseFrequency_(440.0f)
    , noteFrequency_(440.0f)
    , morphPosition_(0.5f)
    , duty_(0.5f)  // Default to 50% duty (symmetric square)
    , ratio_(1.0f)
    , offsetHz_(0.0f)
    , velocityWeight_(0.0f)
    , level_(1.0f)
    , flipPolarity_(false)
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
    
    // Generate waveform based on morph position
    if (morphPos < 0.5f) {
        // Left side: Phase distortion (saw)
        return generatePhaseDistorted(normalizedPhase, morphPos);
    } else {
        // Right side: Tanh-shaped pulse/square with duty control
        return generateTanhShaped(normalizedPhase, morphPos, duty_);
    }
}

float BrainwaveOscillator::process(float sampleRate) {
    // Calculate effective frequency with all modulations
    float freq = calculateEffectiveFrequency(sampleRate);
    
    // Calculate phase increment using double precision to avoid overflow
    // phase_increment = (frequency * 2^32) / sample_rate
    // Use double to maintain precision for large intermediate values
    double phaseIncrementDouble = (static_cast<double>(freq) * 4294967296.0) / static_cast<double>(sampleRate);
    uint32_t phaseIncrement = static_cast<uint32_t>(phaseIncrementDouble);
    
    // Generate current sample using real-time algorithm
    float sample = generateSample(phaseAccumulator_, morphPosition_);
    if (flipPolarity_) {
        sample = -sample;
    }
    
    // Advance phase
    phaseAccumulator_ += phaseIncrement;
    
    return sample;
}
