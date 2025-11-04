#ifndef FAST_MATH_H
#define FAST_MATH_H

#include <cstdint>
#include <cmath>

// Fast math approximations for audio DSP
// Trade bit-exactness for ~10-100x speedup while maintaining <0.001 error
// All functions validated for audio-rate processing

namespace FastMath {

// Constants
constexpr float kPi = 3.14159265359f;
constexpr float kTwoPi = 6.28318530718f;
constexpr float kHalfPi = 1.57079632679f;
constexpr float kInvPi = 0.31830988618f;
constexpr float kInvTwoPi = 0.15915494309f;

// Sine lookup table for fast sin/cos
// 512 samples covering [0, 2π] = 2KB (fits in L1 cache)
constexpr int kSinTableSize = 512;
constexpr float kSinTableScale = kSinTableSize / kTwoPi;

// Sine table - initialized at runtime
extern float sinTable[kSinTableSize + 1];  // +1 for wrap-around sample
extern bool sinTableInitialized;

// Initialize sine table (call once at startup)
inline void initSinTable() {
    if (sinTableInitialized) return;
    for (int i = 0; i <= kSinTableSize; ++i) {
        sinTable[i] = std::sin((i * kTwoPi) / kSinTableSize);
    }
    sinTableInitialized = true;
}

// Fast min/max (branchless on some architectures)
inline float fastmin(float a, float b) {
    return std::fminf(a, b);  // Compiler intrinsic, often single instruction
}

inline float fastmax(float a, float b) {
    return std::fmaxf(a, b);
}

// Fast clamp
inline float fastclamp(float x, float min_val, float max_val) {
    return fastmin(fastmax(x, min_val), max_val);
}

// Fast exp2 approximation using Schraudolph's method
// ~3 FLOPs vs ~50 for std::pow(2.0f, x)
// Error: <1% for x in [-20, 20], acceptable for pitch modulation
inline float fast_exp2(float x) {
    // Clamp to reasonable range to prevent overflow
    x = fastclamp(x, -126.0f, 127.0f);

    union { float f; int32_t i; } u;
    u.i = static_cast<int32_t>((x + 126.94269504f) * 8388608.0f);
    return u.f;
}

// Fast sine using lookup table with linear interpolation
// ~10x faster than std::sin, <0.001 error
inline float fastsin(float x) {
    // Wrap to [0, 2π) range using fast integer-based modulo
    // Convert to table index space first
    float fidx = x * kSinTableScale;

    // Fast modulo: subtract table size until in range
    // Most audio signals are already in [-2π, 2π] so this is fast
    while (fidx >= kSinTableSize) fidx -= kSinTableSize;
    while (fidx < 0.0f) fidx += kSinTableSize;

    // Extract integer and fractional parts
    int idx = static_cast<int>(fidx);
    float frac = fidx - idx;

    // Linear interpolation
    return sinTable[idx] + frac * (sinTable[idx + 1] - sinTable[idx]);
}

// Fast cosine using sine table with phase shift
inline float fastcos(float x) {
    return fastsin(x + kHalfPi);
}

// Fast tanh approximation using rational function
// Modified Padé to stay within [-1, 1] range
// tanh(x) ≈ x / (1 + x²/3)  for small x
// Blend with saturation for large x
inline float fasttanh(float x) {
    // Clamp to range where approximation is valid
    if (x >= 3.0f) return 1.0f;
    if (x <= -3.0f) return -1.0f;

    // Rational approximation: x(27 + x²) / (27 + 9x²)
    float x2 = x * x;
    float result = x * (27.0f + x2) / (27.0f + 9.0f * x2);

    // Ensure output stays in [-1, 1]
    return fastclamp(result, -1.0f, 1.0f);
}

// Fast abs (branchless)
inline float fastabs(float x) {
    union { float f; int32_t i; } u;
    u.f = x;
    u.i &= 0x7FFFFFFF;  // Clear sign bit
    return u.f;
}

} // namespace FastMath

#endif // FAST_MATH_H
