#include "envelope.h"
#include <algorithm>
#include <cmath>

Envelope::Envelope(float sampleRate)
    : sampleRate(sampleRate)
    , stage(EnvelopeStage::OFF)
    , level(0.0f)
    , attackTime(0.01f)      // 10ms default attack
    , decayTime(0.1f)        // 100ms default decay
    , sustainLevel(0.7f)     // 70% sustain level
    , releaseTime(0.2f)      // 200ms default release
    , attackBend(0.5f)       // Linear by default
    , releaseBend(0.5f)      // Linear by default
    , alphaAttack(1.0f)
    , alphaDecay(1.0f)
    , alphaRelease(1.0f)
    , attackSamples(0)
    , decaySamples(0)
    , releaseSamples(0)
    , stageSamplesLeft(0)
    , attackStartLevel(0.0f)
    , releaseStartLevel(0.0f) {
    calculateRates();
}

void Envelope::setAttack(float seconds) {
    attackTime = std::max(0.0001f, seconds);  // Minimum 0.1ms
    calculateRates();
}

void Envelope::setDecay(float seconds) {
    decayTime = std::max(0.0001f, seconds);
    calculateRates();
}

void Envelope::setSustain(float level) {
    sustainLevel = std::clamp(level, 0.0f, 1.0f);
}

void Envelope::setRelease(float seconds) {
    releaseTime = std::max(0.0001f, seconds);
    calculateRates();
}

void Envelope::setAttackBend(float bend) {
    attackBend = std::clamp(bend, 0.0f, 1.0f);
    calculateRates();
}

void Envelope::setReleaseBend(float bend) {
    releaseBend = std::clamp(bend, 0.0f, 1.0f);
    calculateRates();
}

void Envelope::calculateRates() {
    // Map time (seconds) to a per-sample alpha so that we reach ~99% of the target at the given time.
    // alpha = 1 - exp(-ln(100)/(time*fs)) with alpha clamped to [0,1].
    auto timeToAlpha = [&](float timeSeconds) -> float {
        if (timeSeconds <= 0.0f) return 1.0f;  // instant
        float denom = timeSeconds * sampleRate;
        float k = 4.605170186f; // ln(100)
        float a = 1.0f - std::exp(-k / denom);
        if (a < 0.0f) a = 0.0f; if (a > 1.0f) a = 1.0f;
        return a;
    };

    auto bendScale = [&](float bend) -> float { return bendToScale(bend); };

    float baseAttack = timeToAlpha(attackTime);
    float baseDecay  = timeToAlpha(decayTime);
    float baseRelease= timeToAlpha(releaseTime);

    float atkScale = bendScale(attackBend);
    float relScale = bendScale(releaseBend);

    // Adjust alpha by shaping: alpha_eff = 1 - (1 - alpha)^(scale)
    auto shapeAlpha = [](float a, float s) -> float {
        a = std::clamp(a, 0.0f, 1.0f);
        s = std::max(0.05f, s);
        float oneMinus = 1.0f - a;
        float shaped = 1.0f - std::pow(oneMinus, s);
        return std::clamp(shaped, 0.0f, 1.0f);
    };

    alphaAttack  = shapeAlpha(baseAttack, atkScale);
    alphaDecay   = shapeAlpha(baseDecay,  relScale);
    alphaRelease = shapeAlpha(baseRelease,relScale);

    attackSamples  = std::max(0, static_cast<int>(std::round(attackTime  * sampleRate)));
    decaySamples   = std::max(0, static_cast<int>(std::round(decayTime   * sampleRate)));
    releaseSamples = std::max(0, static_cast<int>(std::round(releaseTime * sampleRate)));
}

float Envelope::bendToScale(float bend) {
    // 0.5 = neutral ~1x, <0.5 slows (0.25x), >0.5 speeds (4x). Smooth, monotonic mapping.
    float t = std::clamp(bend, 0.0f, 1.0f);
    return std::pow(2.0f, (t - 0.5f) * 4.0f); // 0.25 .. 4.0
}

void Envelope::noteOn(bool fromCurrentLevel) {
    stage = EnvelopeStage::ATTACK;
    if (fromCurrentLevel) {
        attackStartLevel = level; // smooth retrigger from current level
    } else {
        attackStartLevel = 0.0f;
        level = 0.0f;
    }
    stageSamplesLeft = attackSamples;
}

void Envelope::noteOff() {
    // Enter release stage
    stage = EnvelopeStage::RELEASE;
    releaseStartLevel = level;
    stageSamplesLeft = releaseSamples;
    if (alphaRelease >= 1.0f || releaseSamples == 0) {
        // Instant release
        level = 0.0f;
        stage = EnvelopeStage::OFF;
    }
}

void Envelope::reset() {
    stage = EnvelopeStage::OFF;
    level = 0.0f;
    attackStartLevel = 0.0f;
    releaseStartLevel = 0.0f;
    stageSamplesLeft = 0;
}

float Envelope::process() {
    switch (stage) {
        case EnvelopeStage::OFF:
            level = 0.0f;
            break;

        case EnvelopeStage::ATTACK:
            if (attackSamples == 0 || alphaAttack >= 1.0f) {
                level = 1.0f;
                stage = EnvelopeStage::DECAY;
                stageSamplesLeft = decaySamples;
            } else {
                // One-pole toward 1.0
                float target = 1.0f;
                // Ensure smooth retrigger start
                if (level < attackStartLevel) level = attackStartLevel;
                level += (target - level) * alphaAttack;
                if (--stageSamplesLeft <= 0 || (target - level) < 1e-6f) {
                    level = target;
                    stage = EnvelopeStage::DECAY;
                    stageSamplesLeft = decaySamples;
                }
            }
            break;

        case EnvelopeStage::DECAY:
            if (decaySamples == 0 || alphaDecay >= 1.0f) {
                level = sustainLevel;
                stage = EnvelopeStage::SUSTAIN;
            } else {
                float target = sustainLevel;
                level += (target - level) * alphaDecay;
                if (--stageSamplesLeft <= 0 || std::fabs(target - level) < 1e-6f) {
                    level = target;
                    stage = EnvelopeStage::SUSTAIN;
                }
            }
            break;

        case EnvelopeStage::SUSTAIN:
            level = sustainLevel;
            // Stay here until noteOff is called
            break;

        case EnvelopeStage::RELEASE: {
            if (releaseSamples == 0 || alphaRelease >= 1.0f) {
                level = 0.0f;
                stage = EnvelopeStage::OFF;
            } else {
                float target = 0.0f;
                level += (target - level) * alphaRelease;
                if (--stageSamplesLeft <= 0 || level < 1e-6f) {
                    level = 0.0f;
                    stage = EnvelopeStage::OFF;
                }
            }
            break;
        }
    }

    // Denormal guard
    if (std::fabs(level) < 1e-12f) level = 0.0f;
    return level;
}
