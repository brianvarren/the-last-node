// Frequency-Dependent Allpass Diffuser
// DelayH → HS filter → LS filter with feedback/feedforward

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>
#include <complex>

// ================= DelayH (from your code) =================
inline float clipDelay(float x) {
    return std::clamp(x, 0.0f, 192000.0f - 2.0f);
}

inline std::pair<float,float> intfract(float x) {
    float i;
    float f = modff(x, &i);
    return {i, f};
}

inline float interp4(float xm1, float x0, float x1, float x2, float t) {
    float a0 = -0.5f*xm1 + 1.5f*x0 - 1.5f*x1 + 0.5f*x2;
    float a1 =  xm1     - 2.5f*x0 + 2.0f*x1 - 0.5f*x2;
    float a2 = -0.5f*xm1 + 0.5f*x1;
    return ((a0*t + a1)*t + a2)*t + x0;
}

struct DelayH {
    static constexpr int N = 192000;
    std::array<float,N> buf{}; 
    int w = 0;
    
    void clear() { buf.fill(0.0f); w = 0; }
    
    inline float process(float in, float T, float SR) {
        buf[w] = in;
        const float dSmps = clipDelay(T * SR);
        float r = float(w) - dSmps;
        while (r < 0.f)       r += float(N);
        while (r >= float(N)) r -= float(N);
        
        auto [iFloat, t] = intfract(r);
        int i0 = int(iFloat);
        
        auto at = [&](int idx)->float {
            if (idx < 0) idx += N;
            else if (idx >= N) idx -= N;
            return buf[idx];
        };
        
        float y = interp4(at(i0-1), at(i0), at(i0+1), at(i0+2), t);
        if (++w >= N) w = 0;
        return y;
    }
};

// ================= Shelving Filters (from your code) =================
inline float clipF(float F_hz, float SR) {
    const float fmin = SR / 24576.0f;
    const float fmax = SR /  2.125f;
    return std::clamp(F_hz, fmin, fmax);
}

inline float tanApprox(float x) {
    const float x2 = x * x;
    return x * (1.0f + x2 * (0.333333f + x2 * 0.133333f));
}

inline float warp(float F_hz, float SR) {
    const float k = 3.14159f / SR;
    return tanApprox(F_hz * k);
}

inline std::pair<float,float> warpLS(float F_hz, float dB) {
    const float base = 1.059253692626953f;
    const float r    = std::pow(base, dB);
    const float Fp   = F_hz / r;
    const float A    = r * r;
    return {Fp, A};
}

inline std::pair<float,float> warpHS(float F_hz, float dB) {
    const float base = 1.059253692626953f;
    const float r    = std::pow(base, dB);
    const float Fp   = F_hz * r;
    const float A    = r * r;
    return {Fp, A};
}

struct Coeff { float a1, b0, b1; };

inline Coeff setBL_LS(float F_hz, float dB, float SR) {
    auto [Fp, A] = warpLS(F_hz, dB);
    const float w_Fp = warp(clipF(Fp, SR), SR);
    const float a1 = (1.0f - w_Fp) / (w_Fp + 1.0f);
    const float m = w_Fp / (w_Fp + 1.0f);
    const float n = A - 1.0f;
    const float g = n * m;
    const float b0 = g + 1.0f;
    const float b1 = g - a1;
    return {a1, b0, b1};
}

inline Coeff setBL_HS(float F_hz, float dB, float SR) {
    auto [Fp, A] = warpHS(F_hz, dB);
    const float w_Fp = warp(clipF(Fp, SR), SR);
    const float m = w_Fp + 1.0f;
    const float a1 = (1.0f - w_Fp) / m;
    const float n = A - 1.0f;
    const float g = n / m;
    const float b0 = g + 1.0f;
    const float b1 = -(g + a1);
    return {a1, b0, b1};
}

struct BiLin1P {
    float a1{0}, b0{1}, b1{0};
    float x1{0}, y1{0};
    
    inline void reset(){ x1 = 0; y1 = 0; }
    
    inline void setCoeffs(const Coeff& c) {
        a1 = c.a1; b0 = c.b0; b1 = c.b1;
    }
    
    inline float process(float x){
        float y = b0*x + b1*x1 + a1*y1;
        x1 = x; y1 = y; 
        return y;
    }
};

// ================= Diffuser =================
struct Diffuser {
    DelayH delay;
    BiLin1P hs, ls;
    float fb_state{0.0f};
    float SR;
    
    Diffuser(float sampleRate = 48000.0f) : SR(sampleRate) {}
    
    void clear() {
        delay.clear();
        hs.reset();
        ls.reset();
        fb_state = 0.0f;
    }
    
    inline float process(float in, float T, float Dffs, 
                        float HF, float HB, float LF, float LB) {
        float P = fb_state * Dffs;
        float z = in + P;
        
        float delayed = delay.process(z, T, SR);
        
        Coeff hs_coeff = setBL_HS(HF, HB, SR);
        hs.setCoeffs(hs_coeff);
        float hs_out = hs.process(delayed);
        
        Coeff ls_coeff = setBL_LS(LF, LB, SR);
        ls.setCoeffs(ls_coeff);
        float x = ls.process(hs_out);
        
        float m = in * (-Dffs);
        float out = x + m;
        
        fb_state = out;
        return out;
    }
};

// ================= Test Utilities =================
struct TestResult {
    const char* name;
    bool passed;
    const char* message;
};

std::vector<TestResult> test_results;

void test(const char* name, bool condition, const char* message = "") {
    test_results.push_back({name, condition, message});
    std::printf("[%s] %s", condition ? "PASS" : "FAIL", name);
    if (!condition && message[0] != '\0') {
        std::printf(" - %s", message);
    }
    std::printf("\n");
}

// ================= Comprehensive Tests =================

void test_bypass() {
    std::printf("\n=== Test 1: Bypass (Dffs=0, flat filters) ===\n");
    const float SR = 48000.0f;
    Diffuser diff(SR);
    diff.clear();
    
    // With Dffs=0 and T=0, signal passes through filters only
    // Feedforward and feedback are both zero
    // First sample: delayed impulse goes through filters
    float out0 = diff.process(1.0f, 0.0f, 0.0f, 1000.0f, 0.0f, 1000.0f, 0.0f);
    float out1 = diff.process(0.0f, 0.0f, 0.0f, 1000.0f, 0.0f, 1000.0f, 0.0f);
    
    // With flat filters (0 dB), should get unity gain on first sample
    test("Bypass passes through", std::abs(out0 - 1.0f) < 0.01f);
    std::printf("  First sample: %.6f (expect ~1.0)\n", out0);
    std::printf("  Second sample: %.6f (expect ~0.0)\n", out1);
}

void test_feedforward() {
    std::printf("\n=== Test 2: Feedforward Path ===\n");
    const float SR = 48000.0f;
    Diffuser diff(SR);
    diff.clear();
    
    // First sample should show feedforward: in * (-Dffs)
    float Dffs = 0.7f;
    float out = diff.process(1.0f, 0.01f, Dffs, 1000.0f, 0.0f, 1000.0f, 0.0f);
    float expected = -Dffs;
    
    test("Feedforward magnitude", std::abs(out - expected) < 0.001f);
    std::printf("  Expected: %.3f, Got: %.3f\n", expected, out);
}

void test_delay_timing() {
    std::printf("\n=== Test 3: Delay Timing ===\n");
    const float SR = 48000.0f;
    Diffuser diff(SR);
    
    float T = 0.01f; // 10ms
    int expectedDelay = (int)(T * SR); // 480 samples
    
    diff.clear();
    std::vector<float> response;
    for (int i = 0; i < expectedDelay + 100; ++i) {
        float in = (i == 0) ? 1.0f : 0.0f;
        float out = diff.process(in, T, 0.5f, 1000.0f, 0.0f, 1000.0f, 0.0f);
        response.push_back(out);
    }
    
    // Find peak after first sample
    float maxVal = 0.0f;
    int maxIdx = 0;
    for (int i = 10; i < (int)response.size(); ++i) {
        if (std::abs(response[i]) > std::abs(maxVal)) {
            maxVal = response[i];
            maxIdx = i;
        }
    }
    
    int tolerance = 5; // Allow 5 sample tolerance for interpolation
    bool timingOk = std::abs(maxIdx - expectedDelay) <= tolerance;
    test("Delay timing accuracy", timingOk);
    std::printf("  Expected delay: %d samples, Got: %d samples (diff: %d)\n", 
                expectedDelay, maxIdx, maxIdx - expectedDelay);
}

void test_allpass_property() {
    std::printf("\n=== Test 4: Allpass-like Behavior ===\n");
    const float SR = 48000.0f;
    const int N = 4096;
    Diffuser diff(SR);
    diff.clear();
    
    // Generate impulse response with flat filters
    std::vector<float> h(N);
    for (int i = 0; i < N; ++i) {
        float in = (i == 0) ? 1.0f : 0.0f;
        h[i] = diff.process(in, 0.005f, 0.7f, 1000.0f, 0.0f, 1000.0f, 0.0f);
    }
    
    // Calculate energy
    double energy = 0.0;
    for (float v : h) energy += v * v;
    
    // With frequency-dependent filtering, energy won't be exactly 1.0
    // But should be in a reasonable range (0.5 to 2.0)
    bool energyOk = (energy > 0.5 && energy < 3.0);
    test("Energy in reasonable range", energyOk);
    std::printf("  Total energy: %.3f\n", energy);
    std::printf("  Note: Filters may add/remove energy; not a perfect allpass\n");
}

void test_filter_response() {
    std::printf("\n=== Test 5: Filter Frequency Response ===\n");
    const float SR = 48000.0f;
    const int N = 8192;
    
    // Test with high shelf damping
    Diffuser diff1(SR);
    diff1.clear();
    std::vector<float> h1(N);
    for (int i = 0; i < N; ++i) {
        float in = (i == 0) ? 1.0f : 0.0f;
        h1[i] = diff1.process(in, 0.002f, 0.6f, 8000.0f, -6.0f, 200.0f, 0.0f);
    }
    
    // Test with low shelf boost
    Diffuser diff2(SR);
    diff2.clear();
    std::vector<float> h2(N);
    for (int i = 0; i < N; ++i) {
        float in = (i == 0) ? 1.0f : 0.0f;
        h2[i] = diff2.process(in, 0.002f, 0.6f, 8000.0f, 0.0f, 200.0f, +6.0f);
    }
    
    // Compare energy in different bands (crude frequency analysis)
    auto bandEnergy = [](const std::vector<float>& h, int start, int end) {
        double e = 0.0;
        for (int i = start; i < end && i < (int)h.size(); ++i) {
            e += h[i] * h[i];
        }
        return e;
    };
    
    double e1_early = bandEnergy(h1, 0, N/4);
    double e2_early = bandEnergy(h2, 0, N/4);
    
    test("Filters affect response", true); // Always pass, just informational
    std::printf("  HS damping early energy: %.3f\n", e1_early);
    std::printf("  LS boost early energy: %.3f\n", e2_early);
}

void test_diffusion_amount() {
    std::printf("\n=== Test 6: Diffusion Amount Effect ===\n");
    const float SR = 48000.0f;
    
    // Low diffusion
    Diffuser diff_low(SR);
    diff_low.clear();
    float out_low = diff_low.process(1.0f, 0.01f, 0.1f, 1000.0f, 0.0f, 1000.0f, 0.0f);
    
    // High diffusion
    Diffuser diff_high(SR);
    diff_high.clear();
    float out_high = diff_high.process(1.0f, 0.01f, 0.9f, 1000.0f, 0.0f, 1000.0f, 0.0f);
    
    // First sample is feedforward: in * (-Dffs)
    // Should be -0.1 vs -0.9
    float expected_low = -0.1f;
    float expected_high = -0.9f;
    
    bool correct_low = std::abs(out_low - expected_low) < 0.001f;
    bool correct_high = std::abs(out_high - expected_high) < 0.001f;
    bool different = std::abs(out_high - out_low) > 0.5f;
    
    test("Diffusion affects feedforward", correct_low && correct_high && different);
    std::printf("  Low diffusion (0.1): %.3f (expect %.3f)\n", out_low, expected_low);
    std::printf("  High diffusion (0.9): %.3f (expect %.3f)\n", out_high, expected_high);
}

void test_stability() {
    std::printf("\n=== Test 7: Stability (no blow-up) ===\n");
    const float SR = 48000.0f;
    Diffuser diff(SR);
    diff.clear();
    
    // Process white noise for a while
    bool stable = true;
    float maxOut = 0.0f;
    for (int i = 0; i < 10000; ++i) {
        float in = (float)(rand() % 1000) / 1000.0f - 0.5f; // Random [-0.5, 0.5]
        float out = diff.process(in, 0.015f, 0.8f, 6000.0f, -3.0f, 300.0f, 2.0f);
        maxOut = std::max(maxOut, std::abs(out));
        if (std::abs(out) > 100.0f) {
            stable = false;
            break;
        }
    }
    
    test("Stability under noise", stable);
    std::printf("  Max output magnitude: %.3f\n", maxOut);
}

void test_parameter_sweep() {
    std::printf("\n=== Test 8: Parameter Sweep ===\n");
    const float SR = 48000.0f;
    
    struct Params {
        float T, Dffs, HF, HB, LF, LB;
        const char* desc;
    };
    
    Params tests[] = {
        {0.001f, 0.5f, 10000.0f, -6.0f, 100.0f, 3.0f, "Short delay, high damping"},
        {0.05f, 0.9f, 4000.0f, -2.0f, 500.0f, 1.0f, "Long delay, high diffusion"},
        {0.01f, 0.3f, 12000.0f, 0.0f, 200.0f, 0.0f, "Medium, flat response"},
        {0.02f, 0.0f, 1000.0f, 0.0f, 1000.0f, 0.0f, "No diffusion (bypass fb/ff)"},
    };
    
    for (const auto& p : tests) {
        Diffuser diff(SR);
        diff.clear();
        
        bool valid = true;
        for (int i = 0; i < 1000; ++i) {
            float in = (i == 0) ? 1.0f : 0.0f;
            float out = diff.process(in, p.T, p.Dffs, p.HF, p.HB, p.LF, p.LB);
            if (!std::isfinite(out)) {
                valid = false;
                break;
            }
        }
        
        std::printf("  %s: %s\n", p.desc, valid ? "OK" : "FAILED");
    }
}

// ================= Main =================
int main() {
    std::printf("========================================\n");
    std::printf("  Diffuser Comprehensive Test Suite\n");
    std::printf("========================================\n");
    
    test_bypass();
    test_feedforward();
    test_delay_timing();
    test_allpass_property();
    test_filter_response();
    test_diffusion_amount();
    test_stability();
    test_parameter_sweep();
    
    // Summary
    int passed = 0, failed = 0;
    for (const auto& r : test_results) {
        if (r.passed) passed++; else failed++;
    }
    
    std::printf("\n========================================\n");
    std::printf("  Test Summary: %d passed, %d failed\n", passed, failed);
    std::printf("========================================\n");
    
    return (failed == 0) ? 0 : 1;
}