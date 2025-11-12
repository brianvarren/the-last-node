#ifndef ENVELOPE_H
#define ENVELOPE_H

#include <array>

enum class EnvelopeStage {
    OFF,
    ATTACK,
    DECAY,
    SUSTAIN,
    RELEASE
};

class Envelope {
public:
    Envelope(float sampleRate);
    
    // Set ADSR parameters (in seconds for A, D, R; 0-1 for S)
    void setAttack(float seconds);
    void setDecay(float seconds);
    void setSustain(float level);     // 0.0 to 1.0
    void setRelease(float seconds);

    // Set envelope bend/curve parameters (0.0-1.0, where 0.5 = linear)
    void setAttackBend(float bend);   // <0.5 = concave, >0.5 = convex
    void setReleaseBend(float bend);  // Affects both decay and release curves
    
    // Trigger envelope stages
    void noteOn(bool fromCurrentLevel = false);  // If true, starts attack from current level (for smooth retriggering)
    void noteOff();
    void reset();
    
    // Advance envelope by one sample and return current level
    float process();
    // Advance envelope by a block of samples and return level at block end
    float processBlock(unsigned int samples);
    
    // Get current state
    EnvelopeStage getStage() const { return stage; }
    float getLevel() const { return level; }
    bool isActive() const { return stage != EnvelopeStage::OFF; }
    
private:
    float sampleRate;
    EnvelopeStage stage;
    float level;
    // 0..1 linear time progress through current stage
    float stageProgress;

    // ADSR parameters
    float attackTime;
    float decayTime;
    float sustainLevel;
    float releaseTime;

    // Bend/curve parameters (0.5 = linear, <0.5 = concave, >0.5 = convex)
    float attackBend;
    float releaseBend;
    // Precomputed per-stage rates (progress per sample)
    float attackRate;
    float decayRate;
    float releaseRate;
    // Bend lookup tables (attack uses attackBend, decay/release use releaseBend)
    static constexpr int kBendLUTSize = 1024;
    std::array<float, kBendLUTSize> attackLUT;
    std::array<float, kBendLUTSize> releaseLUT;
    float attackStartLevel;   // Level to start attack from (for smooth retriggering)
    float releaseStartLevel;

    // Calculate rates from times
    void calculateRates();
    // Rebuild bend LUTs when bends change
    void rebuildBendTables();
    inline float lookupAttack(float t) const {
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        float x = t * (kBendLUTSize - 1);
        int i = static_cast<int>(x);
        float f = x - i;
        if (i >= kBendLUTSize - 1) return attackLUT[kBendLUTSize - 1];
        return attackLUT[i] + (attackLUT[i + 1] - attackLUT[i]) * f;
    }
    inline float lookupRelease(float t) const {
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        float x = t * (kBendLUTSize - 1);
        int i = static_cast<int>(x);
        float f = x - i;
        if (i >= kBendLUTSize - 1) return releaseLUT[kBendLUTSize - 1];
        return releaseLUT[i] + (releaseLUT[i + 1] - releaseLUT[i]) * f;
    }
};

#endif // ENVELOPE_H
