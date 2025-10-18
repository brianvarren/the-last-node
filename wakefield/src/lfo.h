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

    // Parameter control
    void setPeriod(float seconds) { period_ = seconds; }
    float getPeriod() const { return period_; }

    void setSyncMode(LFOSyncMode mode) { syncMode_ = mode; }
    LFOSyncMode getSyncMode() const { return syncMode_; }

    void setTempo(float bpm) { tempo_ = bpm; }
    float getTempo() const { return tempo_; }

    // Waveform shaping (same as oscillator)
    void setMorph(float morph) { morphPosition_ = morph; }
    float getMorph() const { return morphPosition_; }

    void setDuty(float duty) { duty_ = std::min(std::max(duty, 0.0f), 1.0f); }
    float getDuty() const { return duty_; }

    void setFlipPolarity(bool flip) { flipPolarity_ = flip; }
    bool getFlipPolarity() const { return flipPolarity_; }

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
    float period_;            // Period in seconds (for free-running mode)
    LFOSyncMode syncMode_;    // Sync mode
    float tempo_;             // Tempo in BPM (for synced mode)
    float morphPosition_;     // 0.0 to 1.0 (waveform shape)
    float duty_;              // 0.0 to 1.0 (pulse width)
    bool flipPolarity_;       // Invert output
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
