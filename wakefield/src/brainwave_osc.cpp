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
    , octaveOffset_(0)  // Default to 0 (no octave shift)
    , lfoEnabled_(false)
    , lfoSpeedIndex_(0)
    , lfoPhase_(0.0f)
    , phaseAccumulator_(0) {
    
    buildLFOLUT();
}

void BrainwaveOscillator::buildLFOLUT() {
    // Build logarithmic LFO frequency lookup table
    // Frequency range from fastest (LFO_MIN_PERIOD) to slowest (LFO_MAX_PERIOD)
    float minFreq = 1.0f / LFO_MAX_PERIOD;  // Slowest
    float maxFreq = 1.0f / LFO_MIN_PERIOD;  // Fastest
    
    for (int i = 0; i < LFO_STEPS; ++i) {
        float t = i / static_cast<float>(LFO_STEPS - 1);
        // Logarithmic interpolation (reversed so index 0 is slow, 9 is fast)
        lfoFreqLUT_[i] = minFreq * std::pow(maxFreq / minFreq, 1.0f - t);
    }
}

void BrainwaveOscillator::setLFOSpeed(int speed) {
    lfoSpeedIndex_ = std::clamp(speed, 0, LFO_STEPS - 1);
}

float BrainwaveOscillator::calculateEffectiveFrequency(float sampleRate) {
    float freq = 0.0f;
    
    if (mode_ == BrainwaveMode::FREE) {
        // FREE mode: use base frequency with octave offset
        freq = baseFrequency_;
        
        // Apply octave offset (bipolar: -3 to +3)
        if (octaveOffset_ > 0) {
            freq *= (1 << octaveOffset_);  // Multiply by 2^octave
        } else if (octaveOffset_ < 0) {
            freq /= (1 << (-octaveOffset_));  // Divide by 2^|octave|
        }
        // If octaveOffset_ == 0, freq stays unchanged
        
        // Apply LFO modulation
        if (lfoEnabled_) {
            float lfoFreq = lfoFreqLUT_[lfoSpeedIndex_];
            lfoPhase_ += lfoFreq / sampleRate;
            if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;
            
            // LFO creates +/- 50% modulation
            float lfoValue = std::sin(lfoPhase_ * 2.0f * M_PI);
            freq *= (1.0f + 0.5f * lfoValue);
        }
    } else {
        // KEY mode: use note frequency with base as offset
        freq = noteFrequency_;
        
        // Apply frequency offset (base frequency acts as detune in cents/Hz)
        float offset = (baseFrequency_ - 440.0f) * 0.1f;  // Scale down the offset
        freq += offset;
        
        // Apply octave offset (bipolar: -3 to +3)
        if (octaveOffset_ > 0) {
            freq *= (1 << octaveOffset_);  // Multiply by 2^octave
        } else if (octaveOffset_ < 0) {
            freq /= (1 << (-octaveOffset_));  // Divide by 2^|octave|
        }
        // If octaveOffset_ == 0, freq stays unchanged
        
        // Apply LFO modulation (LFO speed scales with note frequency)
        if (lfoEnabled_) {
            float baseLFOFreq = lfoFreqLUT_[lfoSpeedIndex_];
            // Scale LFO frequency based on note frequency (higher notes = faster LFO)
            float lfoFreq = baseLFOFreq * (noteFrequency_ / 440.0f);
            lfoPhase_ += lfoFreq / sampleRate;
            if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;
            
            float lfoValue = std::sin(lfoPhase_ * 2.0f * M_PI);
            freq *= (1.0f + 0.3f * lfoValue);  // Slightly less modulation in KEY mode
        }
    }
    
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
    
    // Advance phase
    phaseAccumulator_ += phaseIncrement;
    
    return sample;
}

