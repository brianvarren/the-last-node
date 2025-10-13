#ifndef REVERB_H
#define REVERB_H

#include <vector>

// Forward declaration of Faust-generated class
class mydsp;

class GreyholeReverb {
private:
    float sampleRate;
    mydsp* dsp;
    
    // Parameter values
    float feedback;
    float modDepth;
    float modFreq;
    float diffusion;
    float delayTime;
    float size;
    float damping;
    float mix;
    
    // Buffers for stereo processing
    std::vector<float> leftInput;
    std::vector<float> rightInput;
    std::vector<float> leftOutput;
    std::vector<float> rightOutput;
    float* inputs[2];
    float* outputs[2];
    
public:
    GreyholeReverb(float sampleRate);
    ~GreyholeReverb();
    
    // Parameter setters
    void setDelayTime(float t);     // Delay time 0-1 (maps to 0.001-1.45s)
    void setSize(float s);          // Room size 0-1 (maps to 0.5-3.0)
    void setDamping(float d);       // Damping parameter
    void setMix(float m);           // Dry/wet mix (0-1)
    void setDecay(float d);         // Maps to feedback parameter
    void setDiffusion(float d);     // Diffusion amount (0-1)
    void setModDepth(float d);      // Modulation depth (0-1)
    void setModFreq(float f);       // Modulation frequency (0-10 Hz)
    
    // Process stereo audio
    void process(float* left, float* right, int numSamples);
    
private:
    void updateParameters();
};

#endif // REVERB_H
