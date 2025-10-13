#ifndef LOOP_MANAGER_H
#define LOOP_MANAGER_H

#include <vector>
#include <cstdint>
#include <atomic>
#include "looper.h"

constexpr int MAX_LOOPS = 4;
constexpr int MAX_LOOP_SECONDS = 120;

class LoopManager {
public:
    LoopManager(float sampleRate);
    ~LoopManager();
    
    // Loop selection
    void selectLoop(int index);
    int getCurrentLoopIndex() const { return currentLoop.load(); }
    Looper* getCurrentLoop();
    Looper* getLoop(int index);
    
    // Process all loops (sum outputs)
    void processBlock(const float* inL, const float* inR, float* outL, float* outR, uint32_t nFrames);
    
    // Get loop state for UI
    Looper::State getLoopState(int index) const;
    float getLoopLength(int index) const;
    float getLoopTime(int index) const;
    
    // Global parameters
    void setOverdubMix(float wet);
    float getOverdubMix() const;
    
private:
    float sampleRate;
    uint32_t maxFrames;
    
    // Loop instances
    Looper loopers[MAX_LOOPS];
    
    // Buffer storage (pre-allocated)
    std::vector<float> bufferL[MAX_LOOPS];
    std::vector<float> bufferR[MAX_LOOPS];
    
    // Temp buffers for processing
    std::vector<float> tempL;
    std::vector<float> tempR;
    
    // Current loop selection
    std::atomic<int> currentLoop;
    
    // Apply soft limiting to prevent clipping
    inline float softLimit(float x) {
        // Soft knee limiter
        if (x > 0.8f) {
            return 0.8f + (x - 0.8f) * 0.2f;
        } else if (x < -0.8f) {
            return -0.8f + (x + 0.8f) * 0.2f;
        }
        return x;
    }
};

#endif // LOOP_MANAGER_H

