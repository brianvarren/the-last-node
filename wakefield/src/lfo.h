#ifndef LFO_H
#define LFO_H

#include <cstdint>
#include <cmath>

// LFO sync modes (tempo-synced or free-running)
enum class LFOSyncMode {
    OFF = 0,      // Free-running
    ON = 1,       // Straight sync
    TRIPLET = 2,  // Triplet sync
    DOTTED = 3    // Dotted sync
};

// Low Frequency Oscillator for control-rate modulation
// Uses same waveform generation as BrainwaveOscillator but optimized for LFO rates
class LFO {
public:
    LFO();

    // Setters
    void setPeriod(float period) { period_ = period; }
    void setSyncMode(LFOSyncMode mode) { syncMode_ = mode; }
    void setShape(int shape) { shape_ = shape; }
    void setMorph(float morph) { morphPosition_ = morph; }
    void setDuty(float duty) { duty_ = std::min(std::max(duty, 0.0f), 1.0f); }
    void setFlip(bool flip) { flipPolarity_ = flip; }

    // Getters
    float getPeriod() const { return period_; }
    LFOSyncMode getSyncMode() const { return syncMode_; }
    int getShape() const { return shape_; }
    float getMorph() const { return morphPosition_; }
    float getDuty() const { return duty_; }
    bool getFlipPolarity() const { return flipPolarity_; }

    void setTempo(float bpm) { tempo_ = bpm; }
    float getTempo() const { return tempo_; }

    void setResetOnNote(bool reset) { resetOnNote_ = reset; }
    bool getResetOnNote() const { return resetOnNote_; }

    // Processing
    float process(float sampleRate);

    // Reset phase (called on note-on if resetOnNote is true)
    void reset();

    // Get current output value without advancing (for modulation matrix)
    float getCurrentValue() const { return currentOutput_; }

    float getPhase() const;

private:
    float period_;            // Free-running period in seconds
    LFOSyncMode syncMode_;    // Sync mode
    int shape_;               // 0=phase-distorted, 1=tanh-shaped
    float tempo_;             // Tempo in BPM (for synced mode)
    float morphPosition_;     // 0.0 to 1.0 (waveform shape)
    float duty_;              // 0.0 to 1.0, pulse width control
    bool flipPolarity_;       // Invert waveform polarity
    bool resetOnNote_;        // Reset phase on note-on

    // Phase accumulator (32-bit for high precision)
    uint32_t phaseAccumulator_;

    // Current output value (cached for modulation matrix)
    float currentOutput_;

    // Helper functions
    float calculateFrequency(float sampleRate);
    float generateSample(uint32_t phase);
    float generatePhaseDistorted(float phase, float morph);
    float generateTanhShaped(float phase, float morph, float duty);
};

#endif // LFO_H
