#include "brainwave_osc.h"
#include "brainwave_wavetable.h"
#include <algorithm>
#include <cmath>

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

float BrainwaveOscillator::interpolateSample(uint32_t phase, float morphPos) {
    // Map morph position (0.0-1.0) to frame number (0-255)
    float framePos = morphPos * (BRAINWAVE_NUM_FRAMES - 1);
    int frameA = static_cast<int>(std::floor(framePos));
    int frameB = std::min(frameA + 1, BRAINWAVE_NUM_FRAMES - 1);
    float frameFrac = framePos - frameA;
    
    // Extract wavetable index and interpolation fraction from phase
    // Upper 11 bits = index (0-2047), remaining bits for interpolation
    uint16_t index = phase >> 21;  // Get upper 11 bits (2048 samples)
    
    // Use remaining 21 bits for higher precision interpolation
    uint32_t fracBits = phase & 0x1FFFFF;  // Lower 21 bits
    float muScaled = static_cast<float>(fracBits) / 2097152.0f;  // 2^21 = 2097152
    
    // Get samples from frame A
    int offsetA = frameA * BRAINWAVE_FRAME_SIZE + index;
    int offsetA1 = frameA * BRAINWAVE_FRAME_SIZE + ((index + 1) % BRAINWAVE_FRAME_SIZE);
    float sampleA0 = brainwaveTable[offsetA] / 32768.0f;
    float sampleA1 = brainwaveTable[offsetA1] / 32768.0f;
    float sampleA = lerp(sampleA0, sampleA1, muScaled);
    
    // Get samples from frame B
    int offsetB = frameB * BRAINWAVE_FRAME_SIZE + index;
    int offsetB1 = frameB * BRAINWAVE_FRAME_SIZE + ((index + 1) % BRAINWAVE_FRAME_SIZE);
    float sampleB0 = brainwaveTable[offsetB] / 32768.0f;
    float sampleB1 = brainwaveTable[offsetB1] / 32768.0f;
    float sampleB = lerp(sampleB0, sampleB1, muScaled);
    
    // Interpolate between frames
    return lerp(sampleA, sampleB, frameFrac);
}

float BrainwaveOscillator::process(float sampleRate) {
    // Calculate effective frequency with all modulations
    float freq = calculateEffectiveFrequency(sampleRate);
    
    // Calculate phase increment using double precision to avoid overflow
    // phase_increment = (frequency * 2^32) / sample_rate
    // Use double to maintain precision for large intermediate values
    double phaseIncrementDouble = (static_cast<double>(freq) * 4294967296.0) / static_cast<double>(sampleRate);
    uint32_t phaseIncrement = static_cast<uint32_t>(phaseIncrementDouble);
    
    // Get current sample with morphing
    float sample = interpolateSample(phaseAccumulator_, morphPosition_);
    
    // Advance phase
    phaseAccumulator_ += phaseIncrement;
    
    return sample;
}

