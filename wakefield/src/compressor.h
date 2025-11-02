#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include <cmath>
#include <algorithm>

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

    // Parameter setters (thread-safe, call from any thread)
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

    // Get current gain reduction in dB (for metering)
    float getGainReduction() const { return currentGainReductionDB; }

    // Get calculated makeup gain in dB (for display)
    float getMakeupGain() const { return makeupGainDB; }

private:
    float sampleRate;

    // Parameters
    float thresholdDB;
    float ratio;
    float attackCoeff;
    float releaseCoeff;
    float kneeDB;
    float mix;
    bool autoMakeup;
    float manualMakeupDB;
    bool rmsMode;

    // State variables
    float envelopeL;
    float envelopeR;
    float gainSmoother;  // Smoothed gain reduction
    float currentGainReductionDB;
    float makeupGainDB;  // Auto-calculated or manual

    // RMS detection buffers (for efficient RMS calculation)
    static constexpr int RMS_WINDOW_SIZE = 64;
    float rmsBufferL[RMS_WINDOW_SIZE];
    float rmsBufferR[RMS_WINDOW_SIZE];
    int rmsIndex;
    float rmsSumL;
    float rmsSumR;

    // Automatic makeup gain calculation
    static constexpr int GR_HISTORY_SIZE = 1024;
    float grHistory[GR_HISTORY_SIZE];
    int grHistoryIndex;
    int grHistorySamples;

    // Helper functions
    float computeGainReduction(float inputDB);
    float softClip(float x);
    void updateMakeupGain();
    void updateCoefficients();
};

#endif // COMPRESSOR_H
