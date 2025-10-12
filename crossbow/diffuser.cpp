// Frequency-Dependent Allpass Diffuser
// DelayH → HS filter → LS filter with feedback/feedforward

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

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
    float fb_state{0.0f};  // previous output for feedback
    float SR;
    
    Diffuser(float sampleRate = 48000.0f) : SR(sampleRate) {}
    
    void clear() {
        delay.clear();
        hs.reset();
        ls.reset();
        fb_state = 0.0f;
    }
    
    // Process one sample
    // T: delay time normalized [0,1]
    // Dffs: diffusion amount [0,1]
    // HF, HB: high shelf frequency (Hz) and boost (dB)
    // LF, LB: low shelf frequency (Hz) and boost (dB)
    inline float process(float in, float T, float Dffs, 
                        float HF, float HB, float LF, float LB) {
        // P = out * Dffs
        float P = fb_state * Dffs;
        
        // z = in + P
        float z = in + P;
        
        // x = processLS(processHS(processDelayH(z, T, SR)))
        float delayed = delay.process(z, T, SR);
        
        // HS filter
        Coeff hs_coeff = setBL_HS(HF, HB, SR);
        hs.setCoeffs(hs_coeff);
        float hs_out = hs.process(delayed);
        
        // LS filter
        Coeff ls_coeff = setBL_LS(LF, LB, SR);
        ls.setCoeffs(ls_coeff);
        float x = ls.process(hs_out);
        
        // m = in * (-Dffs)
        float m = in * (-Dffs);
        
        // return x + m
        float out = x + m;
        
        // Store output for next iteration
        fb_state = out;
        
        return out;
    }
};

// ================= Test/Example =================
int main() {
    const float SR = 48000.0f;
    Diffuser diff(SR);
    
    // Example parameters
    const float T = 0.01f;      // 10ms delay (normalized: 0.01 * 48000 = 480 samples)
    const float Dffs = 0.7f;    // 70% diffusion
    const float HF = 8000.0f;   // High shelf at 8kHz
    const float HB = -3.0f;     // -3dB boost (damping)
    const float LF = 200.0f;    // Low shelf at 200Hz
    const float LB = 2.0f;      // +2dB boost (warmth)
    
    std::printf("Frequency-Dependent Allpass Diffuser\n");
    std::printf("=====================================\n");
    std::printf("Sample Rate: %.0f Hz\n", SR);
    std::printf("Delay Time: %.2f ms\n", T * SR / SR * 1000.0f);
    std::printf("Diffusion: %.1f%%\n", Dffs * 100.0f);
    std::printf("High Shelf: %.0f Hz, %.1f dB\n", HF, HB);
    std::printf("Low Shelf: %.0f Hz, %.1f dB\n\n", LF, LB);
    
    // Process impulse response
    diff.clear();
    
    // Calculate expected delay in samples
    int delaySamples = (int)(T * SR);
    int samplesToShow = delaySamples + 50; // show delay + 50 samples after
    
    std::printf("Expected delay: %d samples (%.2f ms)\n\n", delaySamples, delaySamples / SR * 1000.0f);
    std::printf("Impulse response (showing key samples):\n");
    
    std::vector<float> impulseResp;
    impulseResp.reserve(samplesToShow);
    
    for (int i = 0; i < samplesToShow; ++i) {
        float in = (i == 0) ? 1.0f : 0.0f;
        float out = diff.process(in, T, Dffs, HF, HB, LF, LB);
        impulseResp.push_back(out);
    }
    
    // Print first 10 samples (feedforward)
    std::printf("\nFirst 10 samples (feedforward):\n");
    for (int i = 0; i < std::min(10, (int)impulseResp.size()); ++i) {
        std::printf("  [%4d] %+.6f\n", i, impulseResp[i]);
    }
    
    // Print around the delay point
    std::printf("\nAround delay point (%d samples):\n", delaySamples);
    for (int i = std::max(0, delaySamples - 5); 
         i < std::min((int)impulseResp.size(), delaySamples + 20); ++i) {
        std::printf("  [%4d] %+.6f", i, impulseResp[i]);
        if (i == delaySamples) std::printf("  <-- expected delay point");
        std::printf("\n");
    }
    
    // Find actual peak
    float maxVal = 0.0f;
    int maxIdx = 0;
    for (int i = 10; i < (int)impulseResp.size(); ++i) {
        if (std::abs(impulseResp[i]) > std::abs(maxVal)) {
            maxVal = impulseResp[i];
            maxIdx = i;
        }
    }
    std::printf("\nActual peak: %.6f at sample %d (%.2f ms)\n", 
                maxVal, maxIdx, maxIdx / SR * 1000.0f);
    
    return 0;
}