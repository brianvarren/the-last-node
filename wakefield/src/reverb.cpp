#include "reverb.h"
#include "../reverb/greyhole.cpp"
#include <algorithm>
#include <cmath>
#include <cstring>

GreyholeReverb::GreyholeReverb(float sampleRate)
    : sampleRate(sampleRate)
    , dsp(nullptr)
    , feedback(0.9f)
    , modDepth(0.1f)
    , modFreq(2.0f)
    , diffusion(0.5f)
    , delayTime(0.2f)
    , size(1.0f)
    , damping(0.0f)
    , mix(0.3f)
    , leftInput(512, 0.0f)
    , rightInput(512, 0.0f)
    , leftOutput(512, 0.0f)
    , rightOutput(512, 0.0f) {
    
    // Create and initialize the Faust DSP
    dsp = new mydsp();
    dsp->init(static_cast<int>(sampleRate));
    
    // Set up input/output pointers
    inputs[0] = leftInput.data();
    inputs[1] = rightInput.data();
    outputs[0] = leftOutput.data();
    outputs[1] = rightOutput.data();
    
    // Initialize parameters
    updateParameters();
}

GreyholeReverb::~GreyholeReverb() {
    delete dsp;
}

void GreyholeReverb::updateParameters() {
    if (!dsp) return;
    
    // Access the Faust hslider parameters directly (as defined in greyhole.cpp buildUserInterface)
    // fHslider4 = delayTime (0.001 - 1.45, default 0.2)
    // fHslider0 = damping (0.0 - 0.99, default 0.0)
    // fHslider6 = size (0.5 - 3.0, default 1.0)
    // fHslider5 = diffusion (0.0 - 0.99, default 0.5)
    // fHslider1 = feedback (0.0 - 1.0, default 0.9)
    // fHslider3 = modDepth (0.0 - 1.0, default 0.1)
    // fHslider2 = modFreq (0.0 - 10.0, default 2.0)
    
    // Note: We need to access these through a setter method or directly
    // For now, we'll create a simple UI class to set parameters
    
    class SimpleUI : public UI {
    public:
        float* damping;
        float* diffusion;
        float* feedback;
        float* modDepth;
        float* modFreq;
        float* delayTime;
        float* size;
        
        SimpleUI() : damping(nullptr), diffusion(nullptr), feedback(nullptr), 
                     modDepth(nullptr), modFreq(nullptr), delayTime(nullptr), size(nullptr) {}
        
        virtual ~SimpleUI() {}
        
        virtual void openVerticalBox(const char*) override {}
        virtual void openHorizontalBox(const char*) override {}
        virtual void closeBox() override {}
        virtual void declare(void*, const char*, const char*) override {}
        
        virtual void addHorizontalSlider(const char* label, float* zone, float init, float min, float max, float step) override {
            if (strcmp(label, "damping") == 0) damping = zone;
            else if (strcmp(label, "diffusion") == 0) diffusion = zone;
            else if (strcmp(label, "feedback") == 0) feedback = zone;
            else if (strcmp(label, "modDepth") == 0) modDepth = zone;
            else if (strcmp(label, "modFreq") == 0) modFreq = zone;
            else if (strcmp(label, "delayTime") == 0) delayTime = zone;
            else if (strcmp(label, "size") == 0) size = zone;
        }
    };
    
    static SimpleUI* ui = nullptr;
    if (!ui) {
        ui = new SimpleUI();
        dsp->buildUserInterface(ui);
    }
    
    // Set the parameters
    *ui->damping = damping;
    *ui->diffusion = diffusion;
    *ui->feedback = feedback;
    *ui->modDepth = modDepth;
    *ui->modFreq = modFreq;
    *ui->delayTime = delayTime;
    *ui->size = size;
}

void GreyholeReverb::setDelayTime(float t) {
    // Map delay time (0-1) to Greyhole delayTime (0.001-1.45s)
    t = std::clamp(t, 0.0f, 1.0f);
    delayTime = 0.001f + t * 1.449f;
    updateParameters();
}

void GreyholeReverb::setSize(float s) {
    // Map size (0-1) to Greyhole size (0.5-3.0)
    s = std::clamp(s, 0.0f, 1.0f);
    size = 0.5f + s * 2.5f;
    updateParameters();
}

void GreyholeReverb::setDamping(float d) {
    damping = std::clamp(d, 0.0f, 0.99f);
    updateParameters();
}

void GreyholeReverb::setMix(float m) {
    mix = std::clamp(m, 0.0f, 1.0f);
    // Mix is handled in process(), not passed to DSP
}

void GreyholeReverb::setDecay(float d) {
    feedback = std::clamp(d, 0.0f, 1.0f);
    updateParameters();
}

void GreyholeReverb::setDiffusion(float d) {
    diffusion = std::clamp(d, 0.0f, 0.99f);
    updateParameters();
}

void GreyholeReverb::setModDepth(float d) {
    modDepth = std::clamp(d, 0.0f, 1.0f);
    updateParameters();
}

void GreyholeReverb::setModFreq(float f) {
    modFreq = std::clamp(f, 0.0f, 10.0f);
    updateParameters();
}

void GreyholeReverb::process(float* left, float* right, int numSamples) {
    if (!dsp) return;
    
    // Resize buffers if necessary
    if (numSamples > static_cast<int>(leftInput.size())) {
        leftInput.resize(numSamples);
        rightInput.resize(numSamples);
        leftOutput.resize(numSamples);
        rightOutput.resize(numSamples);
        inputs[0] = leftInput.data();
        inputs[1] = rightInput.data();
        outputs[0] = leftOutput.data();
        outputs[1] = rightOutput.data();
    }
    
    // Copy input data and store dry signal
    for (int i = 0; i < numSamples; ++i) {
        leftInput[i] = left[i];
        rightInput[i] = right[i];
    }
    
    // Process through Faust DSP
    dsp->compute(numSamples, inputs, outputs);
    
    // Mix dry and wet signals
    float dryGain = 1.0f - mix;
    float wetGain = mix;
    
    for (int i = 0; i < numSamples; ++i) {
        left[i] = leftInput[i] * dryGain + leftOutput[i] * wetGain;
        right[i] = rightInput[i] * dryGain + rightOutput[i] * wetGain;
    }
}
