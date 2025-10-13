#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <atomic>

class Looper {
public:
    enum State { Empty, Recording, Playing, Overdubbing, Stopped };

    Looper() 
        : L(nullptr)
        , R(nullptr)
        , maxFrames(0)
        , loopLen(0)
        , w(0)
        , r(0)
        , state(Empty)
        , overdubWet(0.6f)
        , xfade(256)
        , armed(false)
        , stateChangeRequested(false)
        , nextState(Empty)
    {}

    void reset(float* bufL, float* bufR, uint32_t frames) {
        L = bufL; 
        R = bufR; 
        maxFrames = frames;
        loopLen = w = r = 0; 
        state = Empty; 
        armed = false;
        stateChangeRequested.store(false);
        std::fill(L, L + maxFrames, 0.0f);
        std::fill(R, R + maxFrames, 0.0f);
    }

    // UI actions (thread-safe via atomics)
    void pressRecPlay() {
        if (state == Empty || state == Stopped) { 
            requestStateChange(Recording);
        }
        else if (state == Recording) { 
            requestStateChange(Playing);
        }
        else if (state == Playing) { 
            requestStateChange(Stopped);
        }
        else if (state == Overdubbing) { 
            requestStateChange(Playing);
        }
    }

    void pressOverdub() { 
        if (state == Playing) {
            requestStateChange(Overdubbing);
        }
        else if (state == Overdubbing) {
            requestStateChange(Playing);
        }
    }

    void pressStop() { 
        requestStateChange(Stopped);
    }

    void pressClear() { 
        requestStateChange(Empty);
    }

    void setOverdubWet(float wet) {
        overdubWet = std::clamp(wet, 0.0f, 1.0f);
    }

    float getOverdubWet() const { return overdubWet; }

    State getState() const { return state; }
    
    uint32_t getLoopLength() const { return loopLen; }
    uint32_t getWritePosition() const { return w; }
    uint32_t getReadPosition() const { return r; }
    
    float getLoopLengthSeconds(float sampleRate) const {
        return loopLen / sampleRate;
    }
    
    float getCurrentTimeSeconds(float sampleRate) const {
        if (state == Recording) {
            return w / sampleRate;
        } else if (state == Playing || state == Overdubbing) {
            return r / sampleRate;
        }
        return 0.0f;
    }

    // Process interleaved stereo input/output
    void processBlock(const float* inL, const float* inR, float* outL, float* outR, uint32_t n) {
        // Check for state change requests
        if (stateChangeRequested.load()) {
            applyStateChange();
        }

        if (state == Empty) {
            // Pass through
            for (uint32_t i = 0; i < n; ++i) { 
                outL[i] = inL[i]; 
                outR[i] = inR[i]; 
            }
            return;
        }

        switch (state) {
            case Recording:
                processRecording(inL, inR, outL, outR, n);
                break;

            case Playing:
                processPlaying(inL, inR, outL, outR, n);
                break;

            case Overdubbing:
                processOverdubbing(inL, inR, outL, outR, n);
                break;

            case Stopped:
                // Pass input through, loop is stopped
                for (uint32_t i = 0; i < n; ++i) { 
                    outL[i] = inL[i]; 
                    outR[i] = inR[i]; 
                }
                break;

            default:
                break;
        }
    }

private:
    float* L;          // external storage
    float* R;
    uint32_t maxFrames;
    uint32_t loopLen;      // valid frames [0, loopLen)
    uint32_t w;            // write head
    uint32_t r;            // read head
    State state;

    // params
    float overdubWet;      // 0..1 amount of new input mixed in while overdubbing
    uint32_t xfade;        // wrap/punch crossfade length in samples
    bool armed;

    // Thread-safe state change
    std::atomic<bool> stateChangeRequested;
    State nextState;

    void requestStateChange(State newState) {
        nextState = newState;
        stateChangeRequested.store(true);
    }

    void applyStateChange() {
        State targetState = nextState;
        
        if (targetState == Playing && state == Recording) {
            finalizeFirstPass();
            r = 0;
        } else if (targetState == Empty) {
            loopLen = w = r = 0;
        } else if (targetState == Recording && state == Empty) {
            w = 0;
        }
        
        state = targetState;
        stateChangeRequested.store(false);
    }

    inline void finalizeFirstPass() {
        loopLen = std::min(w, maxFrames);
        if (loopLen == 0) loopLen = 1;
    }

    void processRecording(const float* inL, const float* inR, float* outL, float* outR, uint32_t n) {
        for (uint32_t i = 0; i < n; ++i) {
            if (w >= maxFrames) { 
                finalizeFirstPass(); 
                state = Playing; 
                r = 0; 
                break; 
            }
            L[w] = inL[i]; 
            R[w] = inR[i];
            outL[i] = inL[i]; 
            outR[i] = inR[i]; // thru while recording
            ++w;
        }
    }

    void processPlaying(const float* inL, const float* inR, float* outL, float* outR, uint32_t n) {
        if (loopLen == 0) {
            for (uint32_t i = 0; i < n; ++i) { 
                outL[i] = inL[i]; 
                outR[i] = inR[i]; 
            }
            return;
        }

        for (uint32_t i = 0; i < n; ++i) {
            // Read from loop
            uint32_t ri = r;
            float sL = L[ri];
            float sR = R[ri];
            
            if (++r >= loopLen) r = 0;

            // Crossfade at wrap points
            uint32_t pos = (r == 0) ? loopLen - 1 : r - 1;
            float xmul = 1.0f;
            if (pos < xfade) {
                xmul = float(pos) / float(xfade);  // fade-in at start
            }
            else if (loopLen - pos < xfade) {
                xmul = float(loopLen - pos) / float(xfade);  // fade-out near end
            }
            
            sL *= xmul; 
            sR *= xmul;

            // Mix with input (input passes through)
            outL[i] = sL + inL[i];
            outR[i] = sR + inR[i];
        }
    }

    void processOverdubbing(const float* inL, const float* inR, float* outL, float* outR, uint32_t n) {
        if (loopLen == 0) { 
            state = Recording; 
            w = 0; 
            processRecording(inL, inR, outL, outR, n); 
            return; 
        }

        for (uint32_t i = 0; i < n; ++i) {
            // Read existing
            float curL = L[r];
            float curR = R[r];
            
            // Mix input into buffer
            float newL = curL * (1.0f - overdubWet) + inL[i] * overdubWet;
            float newR = curR * (1.0f - overdubWet) + inR[i] * overdubWet;
            
            // Crossfade at wrap
            uint32_t pos = r;
            float xmul = 1.0f;
            if (pos < xfade) {
                xmul = float(pos) / float(xfade);
            }
            else if (loopLen - pos < xfade) {
                xmul = float(loopLen - pos) / float(xfade);
            }
            
            L[r] = newL;
            R[r] = newR;
            
            // Output is the new mixed signal plus input pass-through
            outL[i] = (curL * (1.0f - overdubWet) + newL * overdubWet) * xmul + inL[i];
            outR[i] = (curR * (1.0f - overdubWet) + newR * overdubWet) * xmul + inR[i];

            if (++r >= loopLen) r = 0;
        }
    }
};

