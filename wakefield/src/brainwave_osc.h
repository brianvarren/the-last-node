#ifndef BRAINWAVE_OSC_H
#define BRAINWAVE_OSC_H

#include <cstdint>
#include <cmath>

// Brainwave oscillator modes
enum class BrainwaveMode {
    FREE = 0,  // Free-running, user controls frequency
    KEY = 1    // Key-tracking, MIDI note controls frequency
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
    
    // Morph control (0.0 to 1.0, maps to frames 0-255)
    void setMorph(float morph) { morphPosition_ = morph; }
    float getMorph() const { return morphPosition_; }
    
    // Octave control (-3 to +3, bipolar offset)
    void setOctave(int octave) { octaveOffset_ = octave; }
    int getOctave() const { return octaveOffset_; }
    
    // LFO control
    void setLFOEnabled(bool enabled) { lfoEnabled_ = enabled; }
    bool getLFOEnabled() const { return lfoEnabled_; }
    void setLFOSpeed(int speed);  // 0-9 index
    int getLFOSpeed() const { return lfoSpeedIndex_; }
    
    // Note control (for KEY mode)
    void setNoteFrequency(float freq) { noteFrequency_ = freq; }
    float getNoteFrequency() const { return noteFrequency_; }
    
    // Generate one sample and advance phase
    float process(float sampleRate);
    
    // Reset phase
    void reset() { phaseAccumulator_ = 0; lfoPhase_ = 0.0f; }
    
private:
    BrainwaveMode mode_;
    float baseFrequency_;      // User-controlled frequency or offset
    float noteFrequency_;      // MIDI note frequency (KEY mode)
    float morphPosition_;      // 0.0 to 1.0
    int octaveOffset_;         // 0 to 8
    
    // LFO
    bool lfoEnabled_;
    int lfoSpeedIndex_;        // 0-9
    float lfoPhase_;           // LFO phase (0.0 to 1.0)
    
    // Phase accumulator (32-bit for high precision)
    uint32_t phaseAccumulator_;
    
    // LFO frequency lookup table
    static constexpr int LFO_STEPS = 10;
    static constexpr float LFO_MIN_PERIOD = 0.2f;
    static constexpr float LFO_MAX_PERIOD = 1200.0f;
    float lfoFreqLUT_[LFO_STEPS];
    
    // Helper functions
    void buildLFOLUT();
    float calculateEffectiveFrequency(float sampleRate);
    float interpolateSample(uint32_t phase, float morphPos);
    
    // Linear interpolation
    inline float lerp(float a, float b, float t) const {
        return a + (b - a) * t;
    }
};

#endif // BRAINWAVE_OSC_H

