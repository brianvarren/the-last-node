#ifndef BRAINWAVE_OSC_H
#define BRAINWAVE_OSC_H

#include <cstdint>
#include <cmath>

// Brainwave oscillator modes
enum class BrainwaveMode {
    FREE = 0,  // Free-running, user controls frequency
    KEY = 1    // Key-tracking, MIDI note controls frequency
};

// Oscillator waveshape families
enum class BrainwaveShape {
    SAW = 0,
    PULSE = 1
};

class BrainwaveOscillator {
public:
    BrainwaveOscillator();
    
    // Mode control
    void setMode(BrainwaveMode mode) { mode_ = mode; }
    BrainwaveMode getMode() const { return mode_; }
    
    // Frequency control
    void setFrequency(float freq) { baseFrequency_ = freq; }
    float getFrequency() const { return baseFrequency_; }
    void setRatio(float ratio) { ratio_ = ratio; }
    float getRatio() const { return ratio_; }
    void setOffset(float offsetHz) { offsetHz_ = offsetHz; }
    float getOffset() const { return offsetHz_; }
    
    // Morph control (0.0 to 1.0, maps to frames 0-255)
    void setMorph(float morph) { morphPosition_ = morph; }
    float getMorph() const { return morphPosition_; }
    
    // Duty control (0.0 to 1.0, pulse width for square/pulse waves)
    void setDuty(float duty) { duty_ = std::min(std::max(duty, 0.0f), 1.0f); }
    float getDuty() const { return duty_; }

    void setShape(BrainwaveShape shape) { shape_ = shape; }
    BrainwaveShape getShape() const { return shape_; }

    void setLevel(float level) { level_ = std::min(std::max(level, 0.0f), 1.0f); }
    float getLevel() const { return level_; }
    void setFlipPolarity(bool flip) { flipPolarity_ = flip; }
    bool getFlipPolarity() const { return flipPolarity_; }
    
    // Note control (for KEY mode)
    void setNoteFrequency(float freq) { noteFrequency_ = freq; }
    float getNoteFrequency() const { return noteFrequency_; }

    // FM control (Through-Zero FM support)
    void setFMSensitivity(float sensitivity) { fmSensitivity_ = sensitivity; }
    float getFMSensitivity() const { return fmSensitivity_; }

    // Generate one sample and advance phase (with optional FM input)
    float process(float sampleRate, float fmInput = 0.0f);

    // Reset phase
    void reset() { phaseAccumulator_ = 0; }
    
private:
    BrainwaveMode mode_;
    float baseFrequency_;      // User-controlled frequency or offset
    float noteFrequency_;      // MIDI note frequency (KEY mode)
    float morphPosition_;      // 0.0 to 1.0
    float duty_;               // 0.0 to 1.0, pulse width control
    BrainwaveShape shape_;
    float ratio_;              // Frequency multiplier
    float offsetHz_;           // Frequency offset in Hz
    float level_;              // Output level (0-1)
    bool flipPolarity_;        // Invert waveform polarity
    float fmSensitivity_;      // FM depth sensitivity (0-1, default 0.5)

    // Phase accumulator (32-bit for high precision)
    uint32_t phaseAccumulator_;
    
    // Helper functions
    float calculateEffectiveFrequency(float sampleRate);
    float generateSample(uint32_t phase, float morphPos);
};

#endif // BRAINWAVE_OSC_H
