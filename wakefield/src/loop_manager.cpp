#include "loop_manager.h"
#include <algorithm>
#include <cstring>

LoopManager::LoopManager(float sampleRate) 
    : sampleRate(sampleRate)
    , currentLoop(0)
{
    maxFrames = static_cast<uint32_t>(MAX_LOOP_SECONDS * sampleRate);
    
    // Allocate buffers for each loop
    for (int i = 0; i < MAX_LOOPS; ++i) {
        bufferL[i].resize(maxFrames, 0.0f);
        bufferR[i].resize(maxFrames, 0.0f);
        loopers[i].reset(bufferL[i].data(), bufferR[i].data(), maxFrames);
    }
    
    // Allocate temp buffers for processing
    tempL.resize(8192, 0.0f);  // Enough for typical buffer sizes
    tempR.resize(8192, 0.0f);
}

LoopManager::~LoopManager() {
    // Vectors auto-cleanup
}

void LoopManager::selectLoop(int index) {
    if (index >= 0 && index < MAX_LOOPS) {
        currentLoop.store(index);
    }
}

Looper* LoopManager::getCurrentLoop() {
    int index = currentLoop.load();
    if (index >= 0 && index < MAX_LOOPS) {
        return &loopers[index];
    }
    return nullptr;
}

Looper* LoopManager::getLoop(int index) {
    if (index >= 0 && index < MAX_LOOPS) {
        return &loopers[index];
    }
    return nullptr;
}

void LoopManager::processBlock(const float* inL, const float* inR, float* outL, float* outR, uint32_t nFrames) {
    // Ensure temp buffers are large enough
    if (nFrames > tempL.size()) {
        tempL.resize(nFrames);
        tempR.resize(nFrames);
    }
    
    // Clear output buffers
    std::fill(outL, outL + nFrames, 0.0f);
    std::fill(outR, outR + nFrames, 0.0f);
    
    // Process each loop and sum outputs
    for (int i = 0; i < MAX_LOOPS; ++i) {
        // Process this loop
        loopers[i].processBlock(inL, inR, tempL.data(), tempR.data(), nFrames);
        
        // Add to output (with soft limiting to prevent clipping)
        for (uint32_t j = 0; j < nFrames; ++j) {
            outL[j] = softLimit(outL[j] + tempL[j]);
            outR[j] = softLimit(outR[j] + tempR[j]);
        }
    }
}

Looper::State LoopManager::getLoopState(int index) const {
    if (index >= 0 && index < MAX_LOOPS) {
        return loopers[index].getState();
    }
    return Looper::Empty;
}

float LoopManager::getLoopLength(int index) const {
    if (index >= 0 && index < MAX_LOOPS) {
        return loopers[index].getLoopLengthSeconds(sampleRate);
    }
    return 0.0f;
}

float LoopManager::getLoopTime(int index) const {
    if (index >= 0 && index < MAX_LOOPS) {
        return loopers[index].getCurrentTimeSeconds(sampleRate);
    }
    return 0.0f;
}

void LoopManager::setOverdubMix(float wet) {
    // Apply to current loop
    Looper* loop = getCurrentLoop();
    if (loop) {
        loop->setOverdubWet(wet);
    }
}

float LoopManager::getOverdubMix() const {
    int index = currentLoop.load();
    if (index >= 0 && index < MAX_LOOPS) {
        return loopers[index].getOverdubWet();
    }
    return 0.6f;
}

