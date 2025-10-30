#ifndef FM_CONSTANTS_H
#define FM_CONSTANTS_H

// Shared constants describing the FM matrix dimensions.
constexpr int kFMSourceCount = 16;          // OSC1-4, SAMP1-4, Chaos1X-4Y
constexpr int kFMOscillatorTargetCount = 4; // Targets 0-3: Oscillators
constexpr int kFMSamplerTargetCount = 4;    // Targets 4-7: Samplers
constexpr int kFMChaosTargetCount = 4;      // Targets 8-11: Chaos clock frequency
constexpr int kFMTargetCount =
    kFMOscillatorTargetCount + kFMSamplerTargetCount + kFMChaosTargetCount;
constexpr int kFMChaosTargetOffset =
    kFMOscillatorTargetCount + kFMSamplerTargetCount;
constexpr int kFMChaosSourceCount = 8;      // Chaos X/Y pairs (4 generators)

#endif // FM_CONSTANTS_H

