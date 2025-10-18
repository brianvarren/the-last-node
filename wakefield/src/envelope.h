#ifndef ENVELOPE_H
#define ENVELOPE_H

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
    void noteOn();
    void noteOff();
    
    // Advance envelope by one sample and return current level
    float process();
    
    // Get current state
    EnvelopeStage getStage() const { return stage; }
    float getLevel() const { return level; }
    bool isActive() const { return stage != EnvelopeStage::OFF; }
    
private:
    float sampleRate;
    EnvelopeStage stage;
    float level;
    float stageProgress;  // 0-1 progress through current stage (for bend calculation)
    
    // ADSR parameters
    float attackTime;
    float decayTime;
    float sustainLevel;
    float releaseTime;

    // Bend/curve parameters (0.5 = linear, <0.5 = concave, >0.5 = convex)
    float attackBend;
    float releaseBend;

    // Rates (increment per sample)
    float attackRate;
    float decayRate;
    float releaseRate;

    // Calculate rates from times
    void calculateRates();

    // Apply bend curve to a linear 0-1 progress value
    float applyBend(float progress, float bend) const;
};

#endif // ENVELOPE_H