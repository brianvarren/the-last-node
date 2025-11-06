#ifndef BRAINWAVE_OSC_H
#define BRAINWAVE_OSC_H

#include <cstdint>
#include <cmath>

// Brainwave oscillator modes
enum class BrainwaveMode {
    FREE = 0,  // MIDI note + frequency offset (allows detuning from MIDI)
    KEY = 1    // MIDI note only (standard key-tracking)
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
    
    // Shape control (0.0 to 1.0: Sine → Triangle → Saw → Square → Pulse)
    // This is the only waveform control - morph and duty are deprecated
    void setShape(float shape) { shape_ = std::min(std::max(shape, 0.0f), 1.0f); }
    float getShape() const { return shape_; }

    // Deprecated: Morph is now integrated into shape parameter
    void setMorph(float morph) { (void)morph; }  // No-op for backward compatibility
    float getMorph() const { return 0.0f; }

    // Deprecated: Duty is now controlled by shape parameter (pulse region)
    void setDuty(float duty) { (void)duty; }  // No-op for backward compatibility
    float getDuty() const { return 0.5f; }
    
    // Note control (for KEY mode)
    void setNoteFrequency(float freq) { noteFrequency_ = freq; }
    float getNoteFrequency() const { return noteFrequency_; }

    // FM control (Through-Zero FM support)
    void setFMSensitivity(float sensitivity) { fmSensitivity_ = sensitivity; }
    float getFMSensitivity() const { return fmSensitivity_; }

    // Generate one sample and advance phase (with optional FM input and modulation offsets)
    // Note: Level is NOT applied here - it's handled at mixing stage in voice
    float process(float sampleRate, float fmInput = 0.0f,
                  float pitchMod = 0.0f, float morphMod = 0.0f, float dutyMod = 0.0f,
                  float ratioMod = 0.0f, float offsetMod = 0.0f);

    // Reset phase
    void reset() { phaseAccumulator_ = 0; }
    
private:
    BrainwaveMode mode_;
    float baseFrequency_;      // User-controlled frequency or offset
    float noteFrequency_;      // MIDI note frequency (KEY mode)
    float shape_;              // 0.0 to 1.0 (Sine → Triangle → Saw → Square → Pulse)
    float ratio_;              // Frequency multiplier
    float offsetHz_;           // Frequency offset in Hz
    float fmSensitivity_;      // FM depth sensitivity (0-1, default 0.5)

    // Phase accumulator (32-bit for high precision)
    uint32_t phaseAccumulator_;

    // Helper functions
    float calculateEffectiveFrequency(float sampleRate);
    float generateSample(uint32_t phase, float phaseInc, float shapePos);
};

#endif // BRAINWAVE_OSC_H
