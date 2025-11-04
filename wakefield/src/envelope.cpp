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
    , releaseRate(0.0f)
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
    rebuildBendTables();
}

void Envelope::setReleaseBend(float bend) {
    releaseBend = std::clamp(bend, 0.0f, 1.0f);
    rebuildBendTables();
}

void Envelope::calculateRates() {
    auto timeToRate = [&](float timeSeconds) -> float {
        if (timeSeconds <= 0.0f) return 1.0f;
        return 1.0f / (timeSeconds * sampleRate);
    };
    attackRate = timeToRate(attackTime);
    decayRate = timeToRate(decayTime);
    releaseRate = timeToRate(releaseTime);
    rebuildBendTables();
}

void Envelope::rebuildBendTables() {
    // Precompute pow curve for current bends to preserve exact sound with a LUT
    auto build = [](float bend, std::array<float, kBendLUTSize>& table) {
        // Map bend to exponent exactly like original applyBend
        float exponent = std::pow(10.0f, (bend - 0.5f) * 2.0f);
        for (int i = 0; i < kBendLUTSize; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(kBendLUTSize - 1);
            if (bend == 0.5f) {
                table[i] = t;
            } else {
                table[i] = std::pow(t, exponent);
            }
        }
    };
    build(attackBend, attackLUT);
    build(releaseBend, releaseLUT);
}

void Envelope::noteOn(bool fromCurrentLevel) {
    stage = EnvelopeStage::ATTACK;
    if (fromCurrentLevel) {
        attackStartLevel = level; // smooth retrigger from current level
    } else {
        attackStartLevel = 0.0f;
        level = 0.0f;
    }
    stageProgress = 0.0f;
}

void Envelope::noteOff() {
    // Enter release stage
    stage = EnvelopeStage::RELEASE;
    releaseStartLevel = level;
    stageProgress = 0.0f;
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
                float bent = lookupAttack(stageProgress);
                level = attackStartLevel + (1.0f - attackStartLevel) * bent;
            }
            break;

        case EnvelopeStage::DECAY:
            stageProgress += decayRate;
            if (stageProgress >= 1.0f) {
                level = sustainLevel;
                stage = EnvelopeStage::SUSTAIN;
                stageProgress = 0.0f;
            } else {
                float bent = lookupRelease(stageProgress);
                level = 1.0f + (sustainLevel - 1.0f) * bent;
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
                float bent = lookupRelease(stageProgress);
                level = releaseStartLevel * (1.0f - bent);
                if (level < 1e-6f) { level = 0.0f; stage = EnvelopeStage::OFF; stageProgress = 0.0f; }
            }
            break;
        }
    }

    // Denormal guard
    if (std::fabs(level) < 1e-12f) level = 0.0f;
    return level;
}
