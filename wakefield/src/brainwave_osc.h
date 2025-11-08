#ifndef BRAINWAVE_OSC_H
#define BRAINWAVE_OSC_H

#include <cstdint>
#include <cmath>

// Brainwave oscillator modes
enum class BrainwaveMode {
    FREE = 0,  // Free-running, user controls absolute frequency
    KEY = 1    // Key-tracking, MIDI note controls frequency
};

// Discrete oscillator shapes (fast switch without crossfades)
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
    
    // Discrete shape control
    void setShape(int shapeIndex);
    BrainwaveShape getShape() const { return shape_; }

    // Morph control (phase distortion / tanh hardness)
    void setMorph(float morph);
    float getMorph() const { return morph_; }
    
    // Note control (for KEY mode)
    void setNoteFrequency(float freq) { noteFrequency_ = freq; }
    float getNoteFrequency() const { return noteFrequency_; }

    // FM control (Through-Zero FM support)
    void setFMSensitivity(float sensitivity) { fmSensitivity_ = sensitivity; }
    float getFMSensitivity() const { return fmSensitivity_; }

    // Generate one sample and advance phase (with optional FM input and modulation offsets)
    // Note: Level is NOT applied here - it's handled at mixing stage in voice
    float process(float sampleRate, float fmInput = 0.0f,
                  float pitchMod = 0.0f, float morphMod = 0.0f,
                  float ratioMod = 0.0f, float offsetMod = 0.0f);

    // Reset phase
    void reset() {
        phaseAccumulator_ = 0;
        morphState_ = morph_;
    }
    
private:
    BrainwaveMode mode_;
    float baseFrequency_;      // User-controlled frequency or offset
    float noteFrequency_;      // MIDI note frequency (KEY mode)
    BrainwaveShape shape_;
    float morph_;
    float morphState_;
    float ratio_;              // Frequency multiplier
    float offsetHz_;           // Frequency offset in Hz
    float fmSensitivity_;      // FM depth sensitivity (0-1, default 0.5)

    // Phase accumulator (32-bit for high precision)
    uint32_t phaseAccumulator_;

};

#endif // BRAINWAVE_OSC_H
