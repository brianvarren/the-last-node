#include "envelope.h"
#include <algorithm>
#include <cmath>

Envelope::Envelope(float sampleRate)
    : sampleRate(sampleRate)
    , stage(EnvelopeStage::OFF)
    , level(0.0f)
    , stageProgress(0.0f)
    , attackTime(0.01f)      // 10ms default attack
    , decayTime(0.1f)        // 100ms default decay
    , sustainLevel(0.7f)     // 70% sustain level
    , releaseTime(0.2f)      // 200ms default release
    , attackBend(0.5f)       // Linear by default
    , releaseBend(0.5f)      // Linear by default
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

void Envelope::setAttackBend(float bend) {
    attackBend = std::clamp(bend, 0.0f, 1.0f);
}

void Envelope::setReleaseBend(float bend) {
    releaseBend = std::clamp(bend, 0.0f, 1.0f);
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
    stageProgress = 0.0f;
}

void Envelope::noteOff() {
    // Enter release stage
    stage = EnvelopeStage::RELEASE;
    stageProgress = 0.0f;

    // Calculate release rate from current level
    if (releaseTime > 0.0f) {
        releaseRate = level / (releaseTime * sampleRate);
    } else {
        releaseRate = level;  // Instant release
    }
}

float Envelope::applyBend(float progress, float bend) const {
    // progress: 0 to 1 (linear time progress)
    // bend: 0 to 1, where 0.5 = linear, <0.5 = concave (slow start), >0.5 = convex (fast start)

    if (bend == 0.5f) {
        return progress;  // Linear, no bend
    }

    // Map bend from [0, 1] to an exponent
    // bend = 0.0 -> exp = 0.1 (very concave)
    // bend = 0.5 -> exp = 1.0 (linear)
    // bend = 1.0 -> exp = 10.0 (very convex)
    float exponent = std::pow(10.0f, (bend - 0.5f) * 2.0f);

    return std::pow(progress, exponent);
}

float Envelope::process() {
    switch (stage) {
        case EnvelopeStage::OFF:
            level = 0.0f;
            break;

        case EnvelopeStage::ATTACK:
            stageProgress += attackRate;
            if (stageProgress >= 1.0f) {
                stageProgress = 1.0f;
                level = 1.0f;
                stage = EnvelopeStage::DECAY;
                stageProgress = 0.0f;
            } else {
                // Apply attack bend curve
                level = applyBend(stageProgress, attackBend);
            }
            break;

        case EnvelopeStage::DECAY:
            stageProgress += decayRate;
            if (stageProgress >= 1.0f) {
                stageProgress = 1.0f;
                level = sustainLevel;
                stage = EnvelopeStage::SUSTAIN;
            } else {
                // Apply release bend curve (also affects decay)
                // Decay goes from 1.0 to sustainLevel
                float bentProgress = applyBend(stageProgress, releaseBend);
                level = 1.0f - bentProgress * (1.0f - sustainLevel);
            }
            break;

        case EnvelopeStage::SUSTAIN:
            level = sustainLevel;
            // Stay here until noteOff is called
            break;

        case EnvelopeStage::RELEASE: {
            // Store the level we started release from for calculation
            static float releaseStartLevel = level;
            if (stageProgress == 0.0f) {
                releaseStartLevel = level;
            }

            stageProgress += releaseRate;
            if (stageProgress >= 1.0f || level <= 0.0f) {
                stageProgress = 1.0f;
                level = 0.0f;
                stage = EnvelopeStage::OFF;
            } else {
                // Apply release bend curve
                // Release goes from releaseStartLevel to 0
                float bentProgress = applyBend(stageProgress, releaseBend);
                level = releaseStartLevel * (1.0f - bentProgress);
            }
            break;
        }
    }

    return level;
}