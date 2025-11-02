#ifndef FM_CONSTANTS_H
#define FM_CONSTANTS_H

// Shared constants describing the FM matrix dimensions (8 × 8 matrix).
constexpr int kFMOscillatorTargetCount = 4; // Targets 0-3: Oscillators
constexpr int kFMSamplerTargetCount = 4;    // Targets 4-7: Samplers

// Chaos generators are no longer routed directly through the FM matrix.
constexpr int kFMSourceCount =
    kFMOscillatorTargetCount + kFMSamplerTargetCount;  // OSC1-4, SAMP1-4
constexpr int kFMTargetCount =
    kFMOscillatorTargetCount + kFMSamplerTargetCount;

#endif // FM_CONSTANTS_H
