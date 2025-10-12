#include "oscillator.h"

Oscillator::Oscillator()
    : waveform(Waveform::SINE)
    , frequency(440.0f)
    , phase(0.0f) {
}

float Oscillator::generateSine() {
    return std::sin(phase);
}

float Oscillator::generateSquare() {
    // Square wave: +0.7 or -0.7 (reduced amplitude to prevent clipping)
    return (phase < M_PI) ? 0.7f : -0.7f;
}

float Oscillator::generateSawtooth() {
    // Sawtooth: ramps from -1 to 1
    return (2.0f * phase / (2.0f * M_PI)) - 1.0f;
}

float Oscillator::generateTriangle() {
    // Triangle wave
    float value = (2.0f * phase / (2.0f * M_PI));  // 0 to 2
    if (value < 1.0f) {
        return 2.0f * value - 1.0f;  // Rising: -1 to 1
    } else {
        return 3.0f - 2.0f * value;  // Falling: 1 to -1
    }
}

float Oscillator::process(float sampleRate) {
    float sample;
    
    // Generate sample based on waveform type
    switch (waveform) {
        case Waveform::SINE:
            sample = generateSine();
            break;
        case Waveform::SQUARE:
            sample = generateSquare();
            break;
        case Waveform::SAWTOOTH:
            sample = generateSawtooth();
            break;
        case Waveform::TRIANGLE:
            sample = generateTriangle();
            break;
        default:
            sample = 0.0f;
    }
    
    // Advance phase
    float phaseIncrement = 2.0f * M_PI * frequency / sampleRate;
    phase += phaseIncrement;
    
    // Wrap phase
    while (phase >= 2.0f * M_PI) {
        phase -= 2.0f * M_PI;
    }
    
    return sample;
}

const char* Oscillator::getWaveformName(Waveform wf) {
    switch (wf) {
        case Waveform::SINE:     return "Sine";
        case Waveform::SQUARE:   return "Square";
        case Waveform::SAWTOOTH: return "Sawtooth";
        case Waveform::TRIANGLE: return "Triangle";
        default:                 return "Unknown";
    }
}