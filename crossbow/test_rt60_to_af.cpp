// test_rt60_to_af.cpp
#include <cmath>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cassert>

// --------- DUT (same math as our earlier macro) ------------------------------
inline float db2lin(float dB) noexcept { return expf(0.11512925464970229f * dB); }

inline float rt60_from_size(float size01) noexcept {
    return db2lin(size01 * 100.0f) * 0.001f; // seconds
}

struct RT60ToAF {
    static constexpr float kMinRT = 1e-12f;
    inline float operator()(float DT_sec, float size01) const noexcept {
        const float RT = fmaxf(rt60_from_size(size01), kMinRT);
        const float dB = fmaxf((DT_sec / RT) * -60.0f, -144.0f);
        return db2lin(dB);
    }
    static inline float from_DT_RT(float DT_sec, float RT_sec) noexcept {
        const float RT = fmaxf(RT_sec, kMinRT);
        const float dB = fmaxf((DT_sec / RT) * -60.0f, -144.0f);
        return db2lin(dB);
    }
};
// ---------------------------------------------------------------------------

static bool approx(float a, float b, float eps = 1e-6f) {
    float d = fabsf(a - b);
    float m = fmaxf(1.0f, fmaxf(fabsf(a), fabsf(b)));
    return d <= eps * m;
}

int main() {
    std::cout << std::fixed << std::setprecision(8);

    // 1) Closed-form check against 10^(dB/20) where dB = -60 * DT/RT (no floor)
    {
        float DT = 0.050f;    // 50 ms diffuser delay
        float RT = 2.000f;    // 2 s RT60 target
        float af_dut  = RT60ToAF::from_DT_RT(DT, RT);
        float dB_pass = -60.0f * (DT / RT);
        float af_ref1 = db2lin(dB_pass);            // expf(ln10/20 * dB)
        float af_ref2 = powf(10.0f, dB_pass / 20.0f); // pow version

        std::cout << "[closed-form]  DT=" << DT << "  RT=" << RT
                  << "  AF_dut=" << af_dut
                  << "  AF_exp=" << af_ref1
                  << "  AF_pow=" << af_ref2 << "\n";

        assert(approx(af_dut, af_ref1));
        assert(approx(af_dut, af_ref2));
    }

    // 2) RT60 property: after RT seconds, amplitude should be ~0.001 (−60 dB)
    {
        float DT = 0.037f;     // arbitrary step (not a divisor of RT)
        float RT = 1.800f;     // 1.8 s RT60
        float AF = RT60ToAF::from_DT_RT(DT, RT);

        // Theoretical amplitude at t=RT for discrete passes is AF^(RT/DT)
        float passes = RT / DT;
        float amp_at_RT = powf(AF, passes);

        std::cout << "[rt60 check]   DT=" << DT << "  RT=" << RT
                  << "  passes=" << passes
                  << "  AF=" << AF
                  << "  amp_at_RT=" << amp_at_RT << " (expect ~0.001)\n";

        assert(approx(amp_at_RT, 0.001f, 1e-4f)); // allow relaxed tolerance
    }

    // 3) Floor at -144 dB: choose DT/RT so dB_per_pass < -144 → clamps
    {
        float DT = 1.0f, RT = 0.30f; // dB_per_pass = -60*(1/0.3) = -200 dB → floor
        float AF = RT60ToAF::from_DT_RT(DT, RT);
        float AF_floor_exp = db2lin(-144.0f);
        float AF_floor_pow = powf(10.0f, -144.0f / 20.0f);

        std::cout << "[floor check]  DT=" << DT << "  RT=" << RT
                  << "  AF=" << AF
                  << "  AF_floor_exp=" << AF_floor_exp
                  << "  AF_floor_pow=" << AF_floor_pow << "\n";

        assert(approx(AF, AF_floor_exp));
        assert(approx(AF, AF_floor_pow));
    }

    // 4) Zero/near-zero RT input: should clamp and equal the -144 dB floor
    {
        float DT = 0.05f, RT = 0.0f;
        float AF = RT60ToAF::from_DT_RT(DT, RT);
        float AF_floor = db2lin(-144.0f);
        std::cout << "[rt clamp]     DT=" << DT << "  RT=" << RT
                  << "  AF=" << AF << "  AF_floor=" << AF_floor << "\n";
        assert(approx(AF, AF_floor));
    }

    // 5) End-to-end with size→RT60 mapping (uses your size curve)
    {
        float DT = 0.05f;
        float size01 = 0.50f;             // arbitrary
        float RT = rt60_from_size(size01);
        float AF_via_size = RT60ToAF{}(DT, size01);
        float AF_direct   = RT60ToAF::from_DT_RT(DT, RT);

        std::cout << "[size path]    size=" << size01 << "  RT=" << RT
                  << "  AF_size=" << AF_via_size
                  << "  AF_direct=" << AF_direct << "\n";

        assert(approx(AF_via_size, AF_direct));
    }

    std::cout << "\nALL TESTS PASSED\n";
    return 0;
}
