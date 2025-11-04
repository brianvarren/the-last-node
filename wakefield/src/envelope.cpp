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
    , attackRate(0.0)
    , decayRate(0.0)
    , releaseRate(0.0)
    , attackStartLevel(0.0f)
    , releaseStartLevel(0.0f) {

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
    auto timeToRate = [&](float timeSeconds) -> double {
        if (timeSeconds <= 0.0f) {
            return 1.0;  // Instant stage
        }
        return 1.0 / (static_cast<double>(timeSeconds) * static_cast<double>(sampleRate));
    };

    attackRate = timeToRate(attackTime);
    decayRate = timeToRate(decayTime);
    releaseRate = timeToRate(releaseTime);
}

void Envelope::noteOn(bool fromCurrentLevel) {
    stage = EnvelopeStage::ATTACK;
    if (fromCurrentLevel) {
        // Smooth retriggering: start attack from current level (for voice stealing)
        attackStartLevel = level;
    } else {
        // Normal retrigger: start from zero
        attackStartLevel = 0.0f;
        level = 0.0f;
    }
    stageProgress = 0.0f;
}

void Envelope::noteOff() {
    // Enter release stage
    stage = EnvelopeStage::RELEASE;
    stageProgress = 0.0f;
    releaseStartLevel = level;
    if (releaseRate >= 1.0f) {
        // Instant release
        level = 0.0f;
        stage = EnvelopeStage::OFF;
    }
}

void Envelope::reset() {
    stage = EnvelopeStage::OFF;
    level = 0.0f;
    stageProgress = 0.0f;
    attackStartLevel = 0.0f;
    releaseStartLevel = 0.0f;
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
                level = 1.0f;
                stage = EnvelopeStage::DECAY;
                stageProgress = 0.0f;
            } else {
                float progress = static_cast<float>(std::clamp(stageProgress, 0.0, 1.0));
                float bentProgress = applyBend(progress, attackBend);
                // Interpolate from attackStartLevel to 1.0 (supports smooth retriggering)
                level = attackStartLevel + (1.0f - attackStartLevel) * bentProgress;
            }
            break;

        case EnvelopeStage::DECAY:
            stageProgress += decayRate;
            if (stageProgress >= 1.0f) {
                level = sustainLevel;
                stage = EnvelopeStage::SUSTAIN;
                stageProgress = 0.0f;
            } else {
                float progress = static_cast<float>(std::clamp(stageProgress, 0.0, 1.0));
                float bentProgress = applyBend(progress, releaseBend);
                level = 1.0f + (sustainLevel - 1.0f) * bentProgress;
            }
            break;

        case EnvelopeStage::SUSTAIN:
            level = sustainLevel;
            // Stay here until noteOff is called
            break;

        case EnvelopeStage::RELEASE: {
            stageProgress += releaseRate;
            if (stageProgress >= 1.0f) {
                level = 0.0f;
                stage = EnvelopeStage::OFF;
                stageProgress = 0.0f;
            } else {
                float progress = static_cast<float>(std::clamp(stageProgress, 0.0, 1.0));
                float bentProgress = applyBend(progress, releaseBend);
                level = releaseStartLevel * (1.0f - bentProgress);
                if (level <= 0.0001f) {
                    level = 0.0f;
                    stage = EnvelopeStage::OFF;
                    stageProgress = 0.0f;
                }
            }
            break;
        }
    }

    return level;
}
