#include "wavetable.h"
#include "fast_math.h"
#include <cmath>
#include <algorithm>
#include <iostream>

using namespace FastMath;

namespace Wavetable {

void WavetableBank::initialize() {
    if (initialized_) return;

    std::cout << "Initializing wavetables...\n";

    // Allocate storage
    sawTables_.resize(kMipmapLevels);
    for (int mip = 0; mip < kMipmapLevels; ++mip) {
        sawTables_[mip].resize(kMorphSteps);
    }

    pulseTables_.resize(kMipmapLevels);
    for (int mip = 0; mip < kMipmapLevels; ++mip) {
        pulseTables_[mip].resize(kMorphSteps * kDutySteps);
    }

    // Generate all tables
    generateSawTables();
    generatePulseTables();

    initialized_ = true;
    std::cout << "Wavetables initialized: "
              << (sawTables_[0].size() * kMipmapLevels) << " saw tables, "
              << (pulseTables_[0].size() * kMipmapLevels) << " pulse tables\n";
}

void WavetableBank::generateSawTables() {
    for (int mip = 0; mip < kMipmapLevels; ++mip) {
        for (int m = 0; m < kMorphSteps; ++m) {
            float morph = static_cast<float>(m) / (kMorphSteps - 1);
            generatePhaseDistortedSaw(morph, mip, sawTables_[mip][m]);
        }
    }
}

void WavetableBank::generatePulseTables() {
    for (int mip = 0; mip < kMipmapLevels; ++mip) {
        for (int m = 0; m < kMorphSteps / 16; ++m) {  // Reduced morph resolution for pulse
            for (int d = 0; d < kDutySteps; ++d) {
                float morph = static_cast<float>(m) / ((kMorphSteps / 16) - 1);
                float duty = static_cast<float>(d) / (kDutySteps - 1);
                int idx = m * kDutySteps + d;
                generateTanhPulse(morph, duty, mip, pulseTables_[mip][idx]);
            }
        }
    }
}

void WavetableBank::generatePhaseDistortedSaw(float morph, int mipmapLevel, WavetableData& output) {
    // Replicate the phase distortion algorithm from brainwave_osc.cpp
    // but pre-compute for all phase values

    bool mirror = false;
    float morphAmount = morph;

    if (morph < 0.5f) {
        mirror = true;
        morphAmount = 1.0f - morph * 2.0f;
        morphAmount = 0.5f + morphAmount * 0.5f;
    }

    float t = (morphAmount - 0.5f) * 2.0f;
    float pivot = 0.5f + 0.4999f * t;
    pivot = fastclamp(pivot, 0.0001f, 0.9999f);

    for (int i = 0; i < kTableSize; ++i) {
        float phase = static_cast<float>(i) / kTableSize;
        float workingPhase = mirror ? (1.0f - phase) : phase;

        float shapedPhase;
        if (workingPhase <= pivot) {
            float denom = fastmax(1e-6f, 2.0f * pivot);
            shapedPhase = workingPhase / denom;
        } else {
            float denom = fastmax(1e-6f, 1.0f - pivot);
            shapedPhase = 0.5f * (1.0f + ((workingPhase - pivot) / denom));
        }

        // Use std::cos for accurate waveform generation
        output.samples[i] = -std::cos(shapedPhase * kTwoPi);
    }

    // Apply band-limiting
    applyBandLimit(output, mipmapLevel);
}

void WavetableBank::generateTanhPulse(float morph, float duty, int mipmapLevel, WavetableData& output) {
    // Replicate tanh-shaped pulse generation
    float edge = fastclamp(morph, 0.0f, 1.0f);

    for (int i = 0; i < kTableSize; ++i) {
        float phase = static_cast<float>(i) / kTableSize;
        float sine = std::sin(kTwoPi * phase);

        float theta = kTwoPi * (duty - 0.5f);
        float x = sine - std::sin(theta);

        float beta = 1.0f + 80.0f * edge;
        float tanh_pulse = std::tanh(beta * x);

        // Crossfade
        output.samples[i] = (1.0f - edge) * sine + edge * tanh_pulse;
    }

    // Apply band-limiting
    applyBandLimit(output, mipmapLevel);
}

void WavetableBank::applyBandLimit(WavetableData& data, int mipmapLevel) {
    // Simple band-limiting: for each mipmap level, remove high harmonics
    // Level 0 = full bandwidth (up to Nyquist)
    // Level 1 = half bandwidth (remove top octave)
    // Level 2 = quarter bandwidth, etc.

    if (mipmapLevel == 0) return;  // Full bandwidth

    // FFT-based band-limiting would go here
    // For now, use a simple approach: low-pass filter in frequency domain

    // Calculate cutoff harmonic
    int maxHarmonic = kTableSize / (2 * (1 << mipmapLevel));
    maxHarmonic = fastmax(1, maxHarmonic);

    // Simple box-car low-pass: zero out high harmonics
    // In production, use proper windowing (Blackman, etc.)
    std::vector<float> spectrum(kTableSize);

    // Forward DFT (simplified - real signals only)
    for (int k = 0; k < kTableSize / 2; ++k) {
        float real = 0.0f;
        float imag = 0.0f;

        for (int n = 0; n < kTableSize; ++n) {
            float angle = -kTwoPi * k * n / kTableSize;
            real += data.samples[n] * std::cos(angle);
            imag += data.samples[n] * std::sin(angle);
        }

        float magnitude = std::sqrt(real * real + imag * imag);

        // Apply band-limit
        if (k > maxHarmonic) {
            magnitude = 0.0f;
        }

        spectrum[k] = magnitude;
    }

    // Inverse DFT (simplified)
    for (int n = 0; n < kTableSize; ++n) {
        float sum = 0.0f;

        for (int k = 0; k < kTableSize / 2; ++k) {
            if (k <= maxHarmonic) {
                float angle = kTwoPi * k * n / kTableSize;
                sum += spectrum[k] * std::cos(angle) / kTableSize;
            }
        }

        data.samples[n] = sum * 2.0f;  // Factor of 2 for real signals
    }
}

int WavetableBank::selectMipmapLevel(float frequency) const {
    // Select mipmap based on fundamental frequency to prevent aliasing
    // Level 0: 0-6kHz (full bandwidth)
    // Level 1: 6-12kHz
    // Level 2: 12-24kHz, etc.

    constexpr float baseFreq = 6000.0f;

    if (frequency < baseFreq) return 0;

    int level = static_cast<int>(std::log2(frequency / baseFreq)) + 1;
    return fastmin(level, kMipmapLevels - 1);
}

float WavetableBank::lookupSaw(uint32_t phase, float morph, float frequency) const {
    if (!initialized_) return 0.0f;

    // Select mipmap level
    int mipLevel = selectMipmapLevel(frequency);

    // Select morph tables for interpolation
    float morphScaled = morph * (kMorphSteps - 1);
    int morphIdx0 = static_cast<int>(morphScaled);
    int morphIdx1 = fastmin(morphIdx0 + 1, kMorphSteps - 1);
    float morphFrac = morphScaled - morphIdx0;

    // Extract table index and fraction from phase
    uint32_t tableIdx = phase >> 20;  // Top 12 bits (0-4095)
    uint32_t phaseFrac = (phase >> 4) & 0xFFFF;  // Next 16 bits for interpolation
    float fracFloat = phaseFrac * (1.0f / 65536.0f);

    // Sample from first morph table
    const float* table0 = sawTables_[mipLevel][morphIdx0].samples;
    float s00 = table0[tableIdx];
    float s01 = table0[(tableIdx + 1) & (kTableSize - 1)];
    float sample0 = s00 + (s01 - s00) * fracFloat;

    // Sample from second morph table
    const float* table1 = sawTables_[mipLevel][morphIdx1].samples;
    float s10 = table1[tableIdx];
    float s11 = table1[(tableIdx + 1) & (kTableSize - 1)];
    float sample1 = s10 + (s11 - s10) * fracFloat;

    // Interpolate between morph positions
    return sample0 + (sample1 - sample0) * morphFrac;
}

float WavetableBank::lookupPulse(uint32_t phase, float morph, float duty, float frequency) const {
    if (!initialized_) return 0.0f;

    // Select mipmap level
    int mipLevel = selectMipmapLevel(frequency);

    // Select morph and duty tables for interpolation
    // Use reduced morph resolution for pulse (16 instead of 256)
    float morphScaled = morph * ((kMorphSteps / 16) - 1);
    int morphIdx0 = static_cast<int>(morphScaled);
    int morphIdx1 = fastmin(morphIdx0 + 1, (kMorphSteps / 16) - 1);
    float morphFrac = morphScaled - morphIdx0;

    float dutyScaled = duty * (kDutySteps - 1);
    int dutyIdx0 = static_cast<int>(dutyScaled);
    int dutyIdx1 = fastmin(dutyIdx0 + 1, kDutySteps - 1);
    float dutyFrac = dutyScaled - dutyIdx0;

    // Extract table index and fraction from phase
    uint32_t tableIdx = phase >> 20;
    uint32_t phaseFrac = (phase >> 4) & 0xFFFF;
    float fracFloat = phaseFrac * (1.0f / 65536.0f);

    // Four corners for bilinear interpolation
    int idx00 = morphIdx0 * kDutySteps + dutyIdx0;
    int idx01 = morphIdx0 * kDutySteps + dutyIdx1;
    int idx10 = morphIdx1 * kDutySteps + dutyIdx0;
    int idx11 = morphIdx1 * kDutySteps + dutyIdx1;

    // Sample all four corners with phase interpolation
    auto sampleTable = [&](int idx) -> float {
        const float* table = pulseTables_[mipLevel][idx].samples;
        float s0 = table[tableIdx];
        float s1 = table[(tableIdx + 1) & (kTableSize - 1)];
        return s0 + (s1 - s0) * fracFloat;
    };

    float s00 = sampleTable(idx00);
    float s01 = sampleTable(idx01);
    float s10 = sampleTable(idx10);
    float s11 = sampleTable(idx11);

    // Bilinear interpolation
    float s0 = s00 + (s01 - s00) * dutyFrac;
    float s1 = s10 + (s11 - s10) * dutyFrac;

    return s0 + (s1 - s0) * morphFrac;
}

} // namespace Wavetable
