#ifndef FM_CONSTANTS_H
#define FM_CONSTANTS_H

// Shared constants describing the FM matrix dimensions.
constexpr int kFMOscillatorTargetCount = 4; // Targets 0-3: Oscillators
constexpr int kFMSamplerTargetCount = 4;    // Targets 4-7: Samplers
constexpr int kFMChaosTargetCount = 4;      // Targets 8-11: Chaos clock frequency
constexpr int kFMChaosSourceCount = 4;      // Chaos X outputs (4 generators)
constexpr int kFMSourceCount =
    kFMOscillatorTargetCount + kFMSamplerTargetCount + kFMChaosSourceCount; // OSC1-4, SAMP1-4, Chaos1X-4
constexpr int kFMTargetCount =
    kFMOscillatorTargetCount + kFMSamplerTargetCount + kFMChaosTargetCount;
constexpr int kFMChaosTargetOffset =
    kFMOscillatorTargetCount + kFMSamplerTargetCount;

#endif // FM_CONSTANTS_H
