#ifndef OSCILLATOR_H
#define OSCILLATOR_H

#include <cmath>

enum class Waveform {
    SINE,
    SQUARE,
    SAWTOOTH,
    TRIANGLE
};

class Oscillator {
public:
    Oscillator();
    
    void setWaveform(Waveform wf) { waveform = wf; }
    Waveform getWaveform() const { return waveform; }
    
    void setFrequency(float freq) { frequency = freq; }
    float getFrequency() const { return frequency; }
    
    void setPhase(float p) { phase = p; }
    float getPhase() const { return phase; }
    
    // Generate one sample and advance phase
    float process(float sampleRate);
    
    // Get waveform name as string
    static const char* getWaveformName(Waveform wf);
    
private:
    Waveform waveform;
    float frequency;
    float phase;  // 0 to 2π
    
    float generateSine();
    float generateSquare();
    float generateSawtooth();
    float generateTriangle();
};

#endif // OSCILLATOR_H