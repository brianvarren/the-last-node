// test_latediff_tank.cpp
#include <cmath>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cassert>
#include "latediff_tank.hpp"  // Your LateDiffTank class

// If you didn't split files yet, copy the full LateDiffTank code from Bob's
// previous message *and* your Diffuser post above directly above main().

static bool approx_rel(float a, float b, float rel = 2e-3f) { // 0.2% default
    float da = std::fabs(a - b);
    float m  = std::max(std::fabs(a), std::fabs(b));
    return da <= rel * std::max(1.0f, m);
}

int main() {
    // ------------------- config --------------------------------------------
    const float SR    = 48000.0f;
    const float size  = 0.40f;                 // normalized Size
    const float Sz    = size * 100.0f;         // your graph convention
    const float DFFS  = 0.0f;                  // disable internal AP feedback for clean measurement
    const float RT60  = 1.500f;                // 1.5 s global decay
    const float HF    = 8000.0f, HB = 0.0f;    // shelves flat (magnitude ≈ 1)
    const float LF    = 200.0f,  LB = 0.0f;

    LateDiffTank tank(SR);
    tank.clear();

    // ------------------- preroll to settle smoothers -----------------------
    const int preroll = int(0.250f * SR);      // 250 ms is >> half-life (50 ms)
    for (int i = 0; i < preroll; ++i) {
        (void)tank.process(0.0f, Sz, DFFS, RT60, HF, HB, LF, LB);
    }

    // Derive expected T4 and AF from the now-settled mapping (≈ steady)
    // With 250 ms preroll, s4 ≈ p2t(Sz + 0). We'll recompute the target value.
    auto p2f = [](float p)->float { return 8.71742f * expf(0.05776226504666211f * p); };
    auto p2t = [&](float p)->float { float f = p2f(p); return (f > 1e-12f) ? 1.0f / f : 1e12f; };
    const float T1 = p2t(Sz + 9.0f);
    const float T2 = p2t(Sz + 6.0f);
    const float T3 = p2t(Sz + 3.0f);
    const float T4 = p2t(Sz + 0.0f);         // feedback branch delay
    const float AF_expected = expf(0.11512925464970229f * std::max(-60.0f * (T4 / RT60), -144.0f));

    // ------------------- impulse run ---------------------------------------
    const int total = int((T1 + T2 + T3 + 6*T4 + 0.2f) * SR); // enough for ~6 echoes
    std::vector<float> y; y.reserve(total);

    for (int n = 0; n < total; ++n) {
        float x = (n == 0) ? 1.0f : 0.0f;
        float out = tank.process(x, Sz, DFFS, RT60, HF, HB, LF, LB);
        y.push_back(out);
    }

    // ------------------- find first arrival and echo peaks -----------------
    // First arrival ~ T1+T2+T3 seconds after the impulse.
    int idx0 = std::max(0, int(std::round((T1 + T2 + T3) * SR)) - 8);
    // Search forward for absolute peak within a small window
    auto argmax_abs = [&](int a, int b)->int {
        a = std::max(0, a); b = std::min<int>(b, y.size());
        int k=a; float mv = 0.f;
        for (int i=a;i<b;++i){ float v = std::fabs(y[i]); if (v > mv){ mv=v; k=i; } }
        return k;
    };
    idx0 = argmax_abs(idx0, idx0 + 200); // +/- ~4 ms search

    // Successive echoes are spaced by roughly L+T4 where L ~ (T1+T2+T3)
    const float Lsec = (T1 + T2 + T3);
    const int   hop  = int(std::round((Lsec + T4) * SR));
    const int   num_echoes = 5;

    std::vector<int> peak_idx; peak_idx.reserve(num_echoes+1);
    std::vector<float> peak_val; peak_val.reserve(num_echoes+1);

    int center = idx0;
    for (int k = 0; k <= num_echoes; ++k) {
        int lo = center - 100, hi = center + 100; // ± ~2 ms window
        int pk = argmax_abs(lo, hi);
        peak_idx.push_back(pk);
        peak_val.push_back(y[pk]);
        center += hop;
    }

    // ------------------- checks -------------------------------------------
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "T1="<<T1*1000<<"ms  T2="<<T2*1000<<"ms  T3="<<T3*1000<<"ms  T4="<<T4*1000<<"ms\n";
    std::cout << "AF_expected=" << AF_expected << "\n\n";

    bool pass_ratios = true;
    for (int k = 1; k <= num_echoes; ++k) {
        float r = std::fabs(peak_val[k]) / std::max(1e-12f, std::fabs(peak_val[k-1]));
        bool ok = approx_rel(r, AF_expected, 4e-3f); // 0.4% wiggle for interpolation errors
        pass_ratios &= ok;
        std::cout << "Echo " << k
                  << "  idx=" << peak_idx[k]
                  << "  amp=" << peak_val[k]
                  << "  ratio=" << r
                  << (ok ? "  [OK]\n" : "  [MISMATCH]\n");
    }

    // Stability: ensure amplitude envelope decreases and no NaN/inf
    bool pass_stable = true;
    for (float v : y) {
        if (!std::isfinite(v)) { pass_stable = false; break; }
    }
    for (int k = 1; k <= num_echoes; ++k) {
        if (std::fabs(peak_val[k]) > std::fabs(peak_val[k-1]) + 1e-6f) {
            pass_stable = false; break;
        }
    }

    std::cout << "\n[RESULT] AF ratios: " << (pass_ratios ? "PASS" : "FAIL")
              << "   stability: " << (pass_stable ? "PASS" : "FAIL") << "\n";

    // Hard asserts for CI-style runs
    assert(pass_ratios && "Echo ratios != AF");
    assert(pass_stable && "Unstable or non-finite output");

    return 0;
}
