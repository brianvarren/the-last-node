#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include <cmath>
#include <algorithm>
#include <atomic>

/**
 * Stereo Compressor/Limiter with Automatic Makeup Gain
 *
 * Features:
 * - Efficient peak/RMS envelope detection
 * - Soft-knee compression with adjustable ratio
 * - Automatic makeup gain calculation based on gain reduction history
 * - Integrated soft clipping for tasteful saturation
 * - Stereo-linked operation (uses max of L/R for side-chain)
 * - Dry/wet mix control
 *
 * Signal Flow:
 * 1. Input -> Envelope Detection (peak or RMS)
 * 2. Level Detection -> Gain Computer (threshold, ratio, knee)
 * 3. Gain Reduction -> Ballistics (attack/release smoothing)
 * 4. Apply GR + Makeup Gain
 * 5. Soft Clipping
 * 6. Dry/Wet Mix
 */
class Compressor {
public:
    Compressor(float sampleRate);

    // Parameter setters (lock-free; parameters stored atomically)
    void setThreshold(float dB);      // Threshold in dB (-60 to 0)
    void setRatio(float ratio);       // Compression ratio (1:1 to 20:1, inf for limiting)
    void setAttack(float ms);         // Attack time in milliseconds (0.1 to 100)
    void setRelease(float ms);        // Release time in milliseconds (10 to 1000)
    void setKnee(float dB);           // Soft knee width in dB (0 to 20)
    void setMix(float mix);           // Dry/wet mix (0 = dry, 1 = wet)
    void setAutoMakeup(bool enable);  // Enable automatic makeup gain
    void setManualMakeup(float dB);   // Manual makeup gain in dB (0 to 24)
    void setDetectionMode(bool rms);  // true = RMS, false = Peak

    // Process stereo audio (interleaved L/R)
    // output can be the same as input for in-place processing
    void process(const float* input, float* output, int numSamples);

    // Get current gain reduction in dB (negative number)
    float getGainReduction() const { return currentGainReductionDB; }

    // Get calculated makeup gain in dB (for display)
    float getMakeupGain() const { return makeupGainDB.load(); }

private:
    float sampleRate;

    // Parameters (atomics for cross-thread updates)
    std::atomic<float> thresholdDB;
    std::atomic<float> ratio;
    std::atomic<float> kneeDB;
    std::atomic<float> mix;
    std::atomic<bool> autoMakeup;
    std::atomic<float> manualMakeupDB;
    std::atomic<bool> rmsMode;

    // Ballistics coefficients (computed from attack/release setters)
    // Separate detector and gain-stage smoothing
    std::atomic<float> detAttackCoeff;
    std::atomic<float> detReleaseCoeff;
    std::atomic<float> gainAttackCoeff;
    std::atomic<float> gainReleaseCoeff;

    // State variables
    // Peak detector envelopes
    float envelopeL;
    float envelopeR;

    // RMS detector (exponential window, ~20ms)
    float rmsEnvL;
    float rmsEnvR;
    float rmsCoeff;

    // Gain smoothing state
    float gainSmoother;  // Smoothed gain (linear domain)
    float currentGainReductionDB;
    std::atomic<float> makeupGainDB;  // Auto-calculated or manual (dB)

    // Automatic makeup gain calculation (store recent GR dB)
    static constexpr int GR_HISTORY_SIZE = 1024;
    float grHistory[GR_HISTORY_SIZE];
    int grHistoryIndex;
    int grHistorySamples;
    int makeupSampleCounter; // accumulate processed samples for periodic updates

    // Helper functions
    float computeGainReduction(float inputDB, float threshold, float ratio, float knee);
    float softClip(float x);
    void updateMakeupGain();
};

#endif // COMPRESSOR_H
