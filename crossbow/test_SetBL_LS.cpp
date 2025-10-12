// setbl_ls_test.cpp
#include <algorithm>  // std::clamp
#include <cmath>      // std::pow, std::sqrt
#include <cstdio>
#include <utility>

// --- helpers (faithful to your Core macros) ---
inline float clipF(float F_hz, float SR) {
    const float fmin = SR / 24576.0f;
    const float fmax = SR /  2.125f;
    return std::clamp(F_hz, fmin, fmax);
}

inline float tanApprox(float x) {                // from your Tan macro
    const float x2 = x * x;
    return x * (1.0f + x2 * (0.333333f + x2 * 0.133333f));
}

inline std::pair<float,float> warpLS(float F_hz, float dB) {
    // r = base^dB, F' = F / r, A = r^2 (we return F' and A)
    const float base = 1.059253692626953f;      // literal from Core
    const float r    = std::pow(base, dB);
    const float Fp   = F_hz / r;
    const float A    = r * r;
    return {Fp, A};
}

inline float warp(float F_hz, float SR) {        // g = tanApprox(F * (π/SR))
    const float k = 3.14159f / SR;               // truncated π literal
    return tanApprox(F_hz * k);
}

//--- SetBL_LS: builds a1, b0, b1 for 1-pole BL/TPT Low Shelf ---

struct CoeffLS { float a1, b0, b1; };

inline CoeffLS setBL_LS(float F_hz, float dB, float SR) {
    auto [Fp, A] = warpLS(F_hz, dB);     // 1) WarpLS first
    const float w_Fp = warp(clipF(Fp, SR), SR);  // 2) ClipF → Warp on F'
    const float a1 = (1.0f - w_Fp) / (w_Fp + 1.0f);

    const float m  = w_Fp / (w_Fp + 1.0f);
    const float n  = A - 1.0f;
    const float g  = n * m;

    const float b0 = g + 1.0f;
    const float b1 = g - a1;

    return {a1, b0, b1};
}

// --- tiny test you can mirror in Reaktor ---
int main() {
    const float SR = 48000.0f;
    struct Case { float F, dB; } cases[] = {
        {60.0f,  -6.0f},
        {120.0f,  0.0f},
        {500.0f,  +3.0f},
        {2000.0f, +6.0f},
        {8000.0f, -9.0f}
    };

    std::puts("SetBL_LS coefficients (a1, b0, b1)");
    std::puts("F(Hz)   dB      a1            b0            b1");
    std::puts("------------------------------------------------------");
    for (auto c : cases) {
        CoeffLS k = setBL_LS(c.F, c.dB, 44100.0f);
        std::printf("%7.1f %6.1f  % .6f  % .6f  % .6f\n",
                    c.F, c.dB, k.a1, k.b0, k.b1);
    } 
    return 0;
}
