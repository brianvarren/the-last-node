#include "envelope.h"
#include <algorithm>

Envelope::Envelope(float sampleRate)
    : sampleRate(sampleRate)
    , stage(EnvelopeStage::OFF)
    , level(0.0f)
    , attackTime(0.01f)      // 10ms default attack
    , decayTime(0.1f)        // 100ms default decay
    , sustainLevel(0.7f)     // 70% sustain level
    , releaseTime(0.2f)      // 200ms default release
    , attackRate(0.0f)
    , decayRate(0.0f)
    , releaseRate(0.0f) {
    
    calculateRates();
}

void Envelope::setAttack(float seconds) {
    attackTime = std::max(0.001f, seconds);  // Minimum 1ms
    calculateRates();
}

void Envelope::setDecay(float seconds) {
    decayTime = std::max(0.001f, seconds);
    calculateRates();
}

void Envelope::setSustain(float level) {
    sustainLevel = std::clamp(level, 0.0f, 1.0f);
}

void Envelope::setRelease(float seconds) {
    releaseTime = std::max(0.001f, seconds);
    calculateRates();
}

void Envelope::calculateRates() {
    // Attack: go from 0 to 1 in attackTime seconds
    attackRate = 1.0f / (attackTime * sampleRate);
    
    // Decay: go from 1 to sustainLevel in decayTime seconds
    decayRate = (1.0f - sustainLevel) / (decayTime * sampleRate);
    
    // Release: go from current level to 0 in releaseTime seconds
    // We'll calculate this dynamically in noteOff since we don't know current level yet
}

void Envelope::noteOn() {
    stage = EnvelopeStage::ATTACK;
    level = 0.0f;
}

void Envelope::noteOff() {
    // Enter release stage
    stage = EnvelopeStage::RELEASE;
    
    // Calculate release rate from current level
    if (releaseTime > 0.0f) {
        releaseRate = level / (releaseTime * sampleRate);
    } else {
        releaseRate = level;  // Instant release
    }
}

float Envelope::process() {
    switch (stage) {
        case EnvelopeStage::OFF:
            level = 0.0f;
            break;
            
        case EnvelopeStage::ATTACK:
            level += attackRate;
            if (level >= 1.0f) {
                level = 1.0f;
                stage = EnvelopeStage::DECAY;
            }
            break;
            
        case EnvelopeStage::DECAY:
            level -= decayRate;
            if (level <= sustainLevel) {
                level = sustainLevel;
                stage = EnvelopeStage::SUSTAIN;
            }
            break;
            
        case EnvelopeStage::SUSTAIN:
            level = sustainLevel;
            // Stay here until noteOff is called
            break;
            
        case EnvelopeStage::RELEASE:
            level -= releaseRate;
            if (level <= 0.0f) {
                level = 0.0f;
                stage = EnvelopeStage::OFF;
            }
            break;
    }
    
    return level;
}