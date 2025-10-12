// latediff_params_dump.cpp
// One-shot run that prints LateDiff parameters at the exact use-site.
// Compile: g++ -O2 -std=c++17 latediff_params_dump.cpp -o dump && ./dump

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

// ===================== Utilities =====================
inline float db2lin(float dB) noexcept { return expf(0.11512925464970229f * dB); } // ln(10)/20

inline float p2f(float p_semitones) noexcept {
    return 8.71742f * expf(0.05776226504666211f * p_semitones); // ln(2)/12
}
inline float p2t(float p_semitones) noexcept {
    const float f = p2f(p_semitones);
    return (f > 1e-12f) ? 1.0f / f : 1e12f;
}

struct SmoothHalfLife {
    float sr{48000.f}, Thalf{0.05f}, a{0.f}, y{0.f};
    static constexpr float LN2 = 0.6931471805599453f;
    void setSR(float s){ sr = std::max(1.f, s); recompute(); }
    void setHalf(float t){ Thalf = std::max(1e-6f, t); recompute(); }
    void reset(float v=0.f){ y=v; }
    void recompute(){ a = 1.0f - expf(-LN2 / (sr * Thalf)); }
    inline float process(float x){ y += a * (x - y); return y; }
};

inline float rt60_to_AF(float DT_sec, float RT60_sec) noexcept {
    const float RT = std::max(RT60_sec, 1e-12f);
    const float dB = std::max(-60.0f * (DT_sec / RT), -144.0f);
    return db2lin(dB);
}

// ===================== DelayH (cubic interp) =====================
inline float clipDelay(float x) { return std::clamp(x, 0.0f, 192000.0f - 2.0f); }
inline std::pair<float,float> intfract(float x){ float i; float f = modff(x, &i); return {i,f}; }
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
    void clear(){ buf.fill(0.f); w=0; }
    inline float process(float in, float T, float SR) {
        buf[w] = in;
        const float dSmps = clipDelay(T * SR);
        float r = float(w) - dSmps;
        while (r < 0.f)       r += float(N);
        while (r >= float(N)) r -= float(N);
        auto [iFloat, t] = intfract(r);
        int i0 = int(iFloat);
        auto at = [&](int idx)->float{
            if (idx < 0) idx += N;
            else if (idx >= N) idx -= N;
            return buf[idx];
        };
        float y = interp4(at(i0-1), at(i0), at(i0+1), at(i0+2), t);
        if (++w >= N) w = 0;
        return y;
    }
};

// ===================== Shelving filters (your BL 1P HS/LS) =====================
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
    inline void reset(){ x1=0; y1=0; }
    inline void setCoeffs(const Coeff& c){ a1=c.a1; b0=c.b0; b1=c.b1; }
    inline float process(float x){
        float y = b0*x + b1*x1 + a1*y1;
        x1 = x; y1 = y; 
        return y;
    }
};

// ===================== Frequency-Dependent Allpass Diffuser =====================
struct Diffuser {
    DelayH delay;
    BiLin1P hs, ls;
    float fb_state{0.0f};  // previous output for feedback
    float SR{48000.f};
    Diffuser(float sampleRate = 48000.0f) : SR(sampleRate) {}
    void clear(){ delay.clear(); hs.reset(); ls.reset(); fb_state=0.f; }

    // T: delay seconds, Dffs: [0,1], shelves: HF/HB/LF/LB
    inline float process(float in, float T, float Dffs, float HF, float HB, float LF, float LB) {
        float P = fb_state * Dffs;
        float z = in + P;
        float delayed = delay.process(z, T, SR);
        hs.setCoeffs(setBL_HS(HF, HB, SR));
        float hs_out = hs.process(delayed);
        ls.setCoeffs(setBL_LS(LF, LB, SR));
        float x = ls.process(hs_out);
        float m = in * (-Dffs);
        float out = x + m;
        fb_state = out;
        return out;
    }
};

// ===================== LateDiffTank =====================
struct LateDiffTank {
    float SR{48000.f};
    Diffuser d1{SR}, d2{SR}, d3{SR};
    DelayH fbDelay;
    BiLin1P fbHS, fbLS;
    SmoothHalfLife s1, s2, s3, s4; // times (+9, +6, +3, +0)
    float fbSample{0.f};

    LateDiffTank(float sr=48000.f) : SR(sr) { setSampleRate(sr); }
    void setSampleRate(float sr){
        SR = std::max(1.f, sr);
        d1.SR = SR; d2.SR = SR; d3.SR = SR;
        s1.setSR(SR); s2.setSR(SR); s3.setSR(SR); s4.setSR(SR);
        s1.setHalf(0.05f); s2.setHalf(0.05f); s3.setHalf(0.05f); s4.setHalf(0.05f);
        fbDelay.clear(); fbHS.reset(); fbLS.reset(); fbSample=0.f;
    }
    void clear(){
        d1.clear(); d2.clear(); d3.clear();
        fbDelay.clear(); fbHS.reset(); fbLS.reset();
        s1.reset(); s2.reset(); s3.reset(); s4.reset(); fbSample=0.f;
    }

    // M1..M4 are time nudges (seconds) for T1..T4 respectively
    inline float process(float x,
                         float Sz, float Dffs, float RT60,
                         float HF, float HBdB, float LF, float LBdB,
                         float M1=0.f, float M2=0.f, float M3=0.f, float M4=0.f)
    {
        float T1 = s1.process(p2t(Sz + 9.0f)) + M1;
        float T2 = s2.process(p2t(Sz + 6.0f)) + M2;
        float T3 = s3.process(p2t(Sz + 3.0f)) + M3;
        float T4 = s4.process(p2t(Sz + 0.0f)) + M4;

        // run three serial diffusers
        float y = d1.process(x + fbSample, T1, Dffs, HF, HBdB, LF, LBdB);
        y = d2.process(y, T2, Dffs, HF, HBdB, LF, LBdB);
        y = d3.process(y, T3, Dffs, HF, HBdB, LF, LBdB);

        // final shelf on tank output
        fbHS.setCoeffs(setBL_HS(HF, HBdB, SR));
        y = fbHS.process(y);
        fbLS.setCoeffs(setBL_LS(LF, LBdB, SR));
        float out = fbLS.process(y);

        // feedback path: delay T4 then AF
        float fbDelayed = fbDelay.process(out, T4, SR);
        float AF = rt60_to_AF(T4, RT60);
        fbSample = fbDelayed * AF;

        return out;
    }
};

// ===================== Stereo wrapper with debug print =====================
struct LateDiffStereo {
    LateDiffTank L, R;
    float SR{48000.f};
    static constexpr float LTSym = 2.5f; // per your spec

    LateDiffStereo(float sr=48000.f) : L(sr), R(sr), SR(sr) {}

    static inline void map_size(float size01, float& Sz_base, float& Dffs){
        const float szp = size01 * 100.0f;                 // size'
        const float LTSize = 48.0f - (szp * 0.72f);        // LTSize
        Sz_base = LTSize;
        Dffs = std::pow(size01, 1.0f/8.0f);                // LTDffs = size^(1/8)
    }

    inline void process_debug(float xL, float xR,
                              float size01, float RT60,
                              float HF, float HBdB, float LF, float LBdB,
                              float ModF_Hz, float ModA,   // pass-through info
                              float M1_samp, float M5_samp,// sample nudges (few samples)
                              float& yL, float& yR,
                              bool debugOnce=false)
    {
        float Sz_base, Dffs;
        map_size(size01, Sz_base, Dffs);

        const float SzL = Sz_base + LTSym;
        const float SzR = Sz_base - LTSym;

        const float M1_sec = M1_samp / SR;
        const float M5_sec = M5_samp / SR;

        if (debugOnce) {
            std::printf("\n=== LateDiffStereo PARAMS @ use-site ===\n");
            std::printf("Size      : %.6f (norm)\n", size01);
            std::printf("SzL       : %.6f  (LTSize%+0.1f)\n", SzL, +LTSym);
            std::printf("SzR       : %.6f  (LTSize%+0.1f)\n", SzR, -LTSym);
            std::printf("Dffs      : %.6f  (= size^(1/8))\n", Dffs);
            std::printf("RT60      : %.6f s\n", RT60);
            std::printf("ModF      : %.6f Hz\n", ModF_Hz);
            std::printf("ModA      : %.6f (raw units)\n", ModA);
            std::printf("HF / HB   : %.1f Hz  /  %.2f dB\n", HF, HBdB);
            std::printf("LF / LB   : %.1f Hz  /  %.2f dB\n", LF, LBdB);
            std::printf("M1        : % .3f samp  |  %.6f s\n", M1_samp, M1_sec);
            std::printf("M5        : % .3f samp  |  %.6f s\n", M5_samp, M5_sec);
            std::printf("SR        : %.0f Hz\n", SR);
            std::printf("========================================\n");
        }

        // Apply M1 to first diffuser time (L), M5 to feedback time (R) as requested
        yL = L.process(xL, SzL, Dffs, RT60, HF, HBdB, LF, LBdB,
                       /*M1*/ M1_sec, /*M2*/ 0.f, /*M3*/ 0.f, /*M4*/ 0.f);
        yR = R.process(xR, SzR, Dffs, RT60, HF, HBdB, LF, LBdB,
                       /*M1*/ 0.f,    /*M2*/ 0.f, /*M3*/ 0.f, /*M4*/ M5_sec);
    }
};

// ===================== Main: run once and print =====================
int main(){
    const float SR   = 48000.f;
    const float size = 0.42f;   // Size in [0,1]
    const float RT60 = 2.20f;   // seconds
    const float HF   = 8000.f;  // shelf freqs
    const float HB   = -3.0f;   // dB
    const float LF   = 250.f;
    const float LB   = +1.5f;

    const float ModF = 0.35f;   // user-provided context (not used internally here)
    const float ModA = 0.0025f; // raw units (context)

    const float M1_samples = +3.0f;  // “a few samples”
    const float M5_samples = +5.0f;  // “a few samples”

    LateDiffStereo stereo(SR);

    // preroll to settle the 50 ms smoothers (250 ms is plenty)
    for (int i = 0; i < int(0.25f * SR); ++i){
        float dummyL, dummyR;
        stereo.process_debug(0.f, 0.f, size, RT60, HF, HB, LF, LB, ModF, ModA,
                             0.f, 0.f, dummyL, dummyR, false);
    }

    // process one real sample; print params at use-site
    float wetL=0.f, wetR=0.f;
    stereo.process_debug(/*xL*/1.f, /*xR*/1.f,
                         size, RT60, HF, HB, LF, LB,
                         ModF, ModA,
                         M1_samples, M5_samples,
                         wetL, wetR,
                         /*debugOnce*/ true);

    // (Optional) print the first wet output sample just to show it's computed
    std::printf("WetL=%+.7f  WetR=%+.7f  (single-sample output)\n", wetL, wetR);
    return 0;
}
