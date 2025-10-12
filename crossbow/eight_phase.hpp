#pragma once
#include <cmath>
#include <array>

// frac(x) in [0,1)
inline float fracf(float x) noexcept { return x - floorf(x); }

// Triangle distance to nearest integer in [0, 0.5]
inline float wrap_tri(float p) noexcept {
    float f = fracf(p);
    return f <= 0.5f ? f : (1.0f - f);
}

// Parabolic “raised-cosine” shaper on [0, 0.5] → [0, 1]
inline float par_shape(float t) noexcept {
    // y = t * (8 - 16*t) = 1 - 16*(t - 0.25)^2
    return t * (8.0f - 16.0f * t);
}

// Phase accumulator with smoothable Hz input outside this class.
// p is kept in [0,1).
struct PhaseAcc {
    float p{0.f}, sr{48000.f};
    void setSR(float s){ sr = s > 1.f ? s : 1.f; }
    inline float step(float hz){
        p += hz / sr;
        p -= floorf(p);
        return p;
    }
};

// 8-phase LFO: outputs 8 parabolic “cosine-like” waves in [-1, +1]
struct LFO8Parabolic {
    PhaseAcc ph;
    float depth{1.0f};
    // phase offsets: 0, 1/8, 2/8, 3/8, 4/8, 5/8, 6/8, 7/8
    static constexpr float offs[8] = {
        0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f
    };

    void setSR(float s){ ph.setSR(s); }
    void reset(float p0=0.f){ ph.p = fracf(p0); }

    // Advance with frequency (Hz) and compute all 8 outputs.
    // Returns an array of 8 floats in [-depth, +depth].
    std::array<float,8> process(float hz){
        float p = ph.step(hz);
        std::array<float,8> y{};
        for (int i = 0; i < 8; ++i) {
            float t = wrap_tri(p + offs[i]);     // [0,0.5]
            float u = par_shape(t);              // [0,1]
            y[i] = depth * (2.0f*u - 1.0f);      // → [-1,1] then scale
        }
        return y;
    }
};
