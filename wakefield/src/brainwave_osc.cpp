#include "brainwave_osc.h"
#include <algorithm>
#include <cmath>

// Phase distortion phaseshaper (Casio CZ-style)
static float phaseshaper(float x, float d) {
    // Classic phase distortion synthesis
    // x: normalized phase (0.0 to 1.0)
    // d: point of inflection (0.0 to 1.0)
    if (x <= d) {
        return (1.0f / M_PI) * x;
    } else {
        return (1.0f / M_PI) * (1.0f + ((x - d) / (1.0f - d)));
    }
}

// Generate waveform with phase distortion (morph 0.0 to 0.5)
static float generatePhaseDistorted(float phase, float morph) {
    // Map morph to d parameter
    // morph = 0.0 → d = 1.0 (maximum distortion)
    // morph = 0.5 → d = 0.5 (pure sine)
    float d = 1.0f - (morph * 2.0f);
    
    // Apply phaseshaper and generate output
    float shapedPhase = phaseshaper(phase, d);
    return -std::cos(2.0f * M_PI * shapedPhase);
}

// Generate waveform with tanh waveshaping (morph 0.5 to 1.0)
static float generateTanhShaped(float phase, float morph) {
    // Map morph to amount
    // morph = 0.5 → amount = 0.0 (pure sine)
    // morph = 1.0 → amount = 1.0 (maximum saturation)
    float amount = (morph - 0.5f) * 2.0f;
    
    // Start with pure sine
    float sine = std::sin(2.0f * M_PI * phase);
    
    // Apply increasing gain before tanh
    // Scale factor chosen to create square-like wave at amount=1.0
    float gain = 1.0f + (amount * 9.0f);  // 1.0 to 10.0
    float shaped = std::tanh(sine * gain);
    
    // Normalize to maintain amplitude
    float normalizer = std::tanh(gain);
    return shaped / normalizer;
}

BrainwaveOscillator::BrainwaveOscillator()
    : mode_(BrainwaveMode::FREE)
    , baseFrequency_(440.0f)
    , noteFrequency_(440.0f)
    , morphPosition_(0.5f)
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
        // Left side: Phase distortion
        return generatePhaseDistorted(normalizedPhase, morphPos);
    } else {
        // Right side: Tanh waveshaping
        return generateTanhShaped(normalizedPhase, morphPos);
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

