#ifndef WAVETABLE_H
#define WAVETABLE_H

#include <cstdint>
#include <vector>
#include <memory>

// Wavetable system for high-performance oscillator synthesis
// Replaces real-time trig/distortion with pre-computed lookup tables

namespace Wavetable {

// Constants
constexpr int kTableSize = 4096;  // Samples per waveform cycle
constexpr int kMorphSteps = 256;  // Number of morph positions
constexpr int kDutySteps = 16;    // Number of duty cycle positions (for pulse)
constexpr int kMipmapLevels = 8;  // Octave-based mipmaps for anti-aliasing

// Waveform types
enum class WaveType {
    SAW = 0,    // Phase-distorted sawtooth
    PULSE = 1   // Tanh-shaped pulse
};

// Single wavetable: one morph position, one mipmap level
struct WavetableData {
    float samples[kTableSize];

    WavetableData() {
        for (int i = 0; i < kTableSize; ++i) {
            samples[i] = 0.0f;
        }
    }
};

// Wavetable bank manages all pre-computed waveforms
class WavetableBank {
public:
    static WavetableBank& getInstance() {
        static WavetableBank instance;
        return instance;
    }

    // Initialize wavetables (call once at startup)
    void initialize();

    // Check if initialized
    bool isInitialized() const { return initialized_; }

    // Lookup sample with linear interpolation
    // phase: 32-bit phase accumulator
    // morph: 0.0 to 1.0
    // duty: 0.0 to 1.0 (pulse only)
    // frequency: for mipmap selection
    float lookupSaw(uint32_t phase, float morph, float frequency) const;
    float lookupPulse(uint32_t phase, float morph, float duty, float frequency) const;

private:
    WavetableBank() : initialized_(false) {}
    ~WavetableBank() = default;

    // Prevent copying
    WavetableBank(const WavetableBank&) = delete;
    WavetableBank& operator=(const WavetableBank&) = delete;

    // Generate methods
    void generateSawTables();
    void generatePulseTables();

    // Generate a single waveform
    void generatePhaseDistortedSaw(float morph, int mipmapLevel, WavetableData& output);
    void generateTanhPulse(float morph, float duty, int mipmapLevel, WavetableData& output);

    // Band-limiting (removes harmonics above Nyquist for given mipmap level)
    void applyBandLimit(WavetableData& data, int mipmapLevel);

    // Helper: select mipmap level based on frequency
    int selectMipmapLevel(float frequency) const;

    // Storage: [mipmap][morph]
    // Saw: 8 mipmaps × 256 morph = 2048 tables × 4KB = 8 MB
    std::vector<std::vector<WavetableData>> sawTables_;

    // Pulse: 8 mipmaps × 16 morph × 16 duty = 2048 tables × 4KB = 8 MB
    // Index as: [mipmap][morph * kDutySteps + duty]
    std::vector<std::vector<WavetableData>> pulseTables_;

    bool initialized_;
};

} // namespace Wavetable

#endif // WAVETABLE_H
