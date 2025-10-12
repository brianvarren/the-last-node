#pragma once
#include <algorithm>
#include <array>
#include <cmath>

inline float mapSize01ToLTsemi(float u01) noexcept {
    // size' = size*100; LTSize = 48 - (size' * 0.72) == 48 - 72*size
    u01 = std::clamp(u01, 0.0f, 1.0f);
    return 48.0f - 72.0f * u01;  // semitones in [+48 … -24]
}

// ===== 8-phase LFO =========================================================
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

// ===== dB -> linear =========================================================
inline float db2lin(float dB) noexcept { return expf(0.11512925464970229f * dB); }

// ===== pitch->freq/time (log time control via semitone steps) ===============
inline float p2f(float p_semitones) noexcept {
    return 8.71742f * expf(0.05776226504666211f * p_semitones); // ln(2)/12
}
inline float p2t(float p_semitones) noexcept {
    float f = p2f(p_semitones);
    return (f > 1e-12f) ? 1.0f / f : 1e12f;
}

// ===== smoother (half-life seconds) ========================================
struct SmoothHalfLife {
    float sr{48000.f}, Thalf{0.05f}, a{0.f}, y{0.f};
    static constexpr float LN2 = 0.6931471805599453f;
    void setSR(float s){ sr = std::max(1.f, s); recompute(); }
    void setHalf(float t){ Thalf = std::max(1e-6f, t); recompute(); }
    void reset(float v=0.f){ y = v; }
    void recompute(){ a = 1.f - expf(-LN2 / (sr * Thalf)); }
    inline float process(float x){ y += a * (x - y); return y; }
};

// ===== RT60 -> attenuation factor for a per-pass delay DT ===================
inline float rt60_to_AF(float DT_sec, float RT60_sec) noexcept {
    float RT = std::max(RT60_sec, 1e-12f);
    float dB = std::max(-60.0f * (DT_sec / RT), -144.0f);
    return db2lin(dB);
}

// ===== Your DelayH / Shelves / Diffuser (paste from your post) =============
// (BEGIN paste region)
inline float clipDelay(float x) { return std::clamp(x, 0.0f, 192000.0f - 2.0f); }
inline std::pair<float,float> intfract(float x){ float i; float f = modff(x, &i); return {i,f}; }
inline float interp4(float xm1,float x0,float x1,float x2,float t){
    float a0 = -0.5f*xm1 + 1.5f*x0 - 1.5f*x1 + 0.5f*x2;
    float a1 =  xm1     - 2.5f*x0 + 2.0f*x1 - 0.5f*x2;
    float a2 = -0.5f*xm1 + 0.5f*x1;
    return ((a0*t + a1)*t + a2)*t + x0;
}
struct DelayH {
    static constexpr int N = 192000;
    std::array<float,N> buf{}; int w=0;
    void clear(){ buf.fill(0.f); w=0; }
    inline float process(float in, float T, float SR){
        buf[w] = in;
        float dSmps = clipDelay(T * SR);
        float r = float(w) - dSmps;
        while (r < 0.f) r += float(N);
        while (r >= float(N)) r -= float(N);
        auto [iFloat,t] = intfract(r);
        int i0 = int(iFloat);
        auto at = [&](int idx)->float{ if (idx<0) idx+=N; else if (idx>=N) idx-=N; return buf[idx]; };
        float y = interp4(at(i0-1), at(i0), at(i0+1), at(i0+2), t);
        if (++w >= N) w = 0;
        return y;
    }
};

inline float clipF(float F_hz, float SR) {
    const float fmin = SR / 24576.0f;
    const float fmax = SR /  2.125f;
    return std::clamp(F_hz, fmin, fmax);
}
inline float tanApprox(float x){ float x2=x*x; return x*(1.f + x2*(0.333333f + x2*0.133333f)); }
inline float warp(float F_hz, float SR){ const float k = 3.14159f / SR; return tanApprox(F_hz * k); }
inline std::pair<float,float> warpLS(float F_hz, float dB){
    const float base=1.059253692626953f; float r = pow(base,dB); float Fp = F_hz / r; float A = r*r; return {Fp,A};
}
inline std::pair<float,float> warpHS(float F_hz, float dB){
    const float base=1.059253692626953f; float r = pow(base,dB); float Fp = F_hz * r; float A = r*r; return {Fp,A};
}
struct Coeff{ float a1,b0,b1; };
inline Coeff setBL_LS(float F_hz,float dB,float SR){
    auto [Fp,A] = warpLS(F_hz,dB);
    float w_Fp = warp(clipF(Fp,SR), SR);
    float a1 = (1.f - w_Fp) / (w_Fp + 1.f);
    float m = w_Fp / (w_Fp + 1.f);
    float g = (A - 1.f) * m;
    float b0 = g + 1.f;
    float b1 = g - a1;
    return {a1,b0,b1};
}
inline Coeff setBL_HS(float F_hz,float dB,float SR){
    auto [Fp,A] = warpHS(F_hz,dB);
    float w_Fp = warp(clipF(Fp,SR), SR);
    float m = w_Fp + 1.f;
    float a1 = (1.f - w_Fp) / m;
    float g = (A - 1.f) / m;
    float b0 = g + 1.f;
    float b1 = -(g + a1);
    return {a1,b0,b1};
}
struct BiLin1P {
    float a1{0}, b0{1}, b1{0}, x1{0}, y1{0};
    inline void reset(){ x1=0; y1=0; }
    inline void setCoeffs(const Coeff& c){ a1=c.a1; b0=c.b0; b1=c.b1; }
    inline float process(float x){ float y = b0*x + b1*x1 + a1*y1; x1=x; y1=y; return y; }
};

// Frequency-dependent allpass diffuser from your post
struct Diffuser {
    DelayH delay; BiLin1P hs, ls; float fb_state{0.f}; float SR{48000.f};
    Diffuser(float sampleRate=48000.f): SR(sampleRate) {}
    void clear(){ delay.clear(); hs.reset(); ls.reset(); fb_state=0.f; }
    inline float process(float in, float T, float Dffs, float HF,float HB,float LF,float LB){
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
// (END paste region)

// ====== LateDiff tank =======================================================
struct LateDiffTank {
    float SR{48000.f};

    // three serial diffusers
    Diffuser d1{SR}, d2{SR}, d3{SR};

    // feedback branch: extra delay + HS->LS shelf
    DelayH fbDelay;
    BiLin1P fbHS, fbLS;

    // time mappers/smoothers for Sz offsets +9,+6,+3,+0
    SmoothHalfLife s1, s2, s3, s4;

    // shelf params (shared: both in diffusers and final EQ)
    float HF{8000.f}, HB{0.f}, LF{200.f}, LB{0.f};

    // feedback state (computed from previous output)
    float fbSample{0.f};

    LateDiffTank(float sampleRate=48000.f) : SR(sampleRate) {
        setSampleRate(sampleRate);
    }

    void setSampleRate(float sr){
        SR = std::max(1.f, sr);
        d1.SR = SR; d2.SR = SR; d3.SR = SR;
        s1.setSR(SR); s2.setSR(SR); s3.setSR(SR); s4.setSR(SR);
        s1.setHalf(0.05f); s2.setHalf(0.05f); s3.setHalf(0.05f); s4.setHalf(0.05f);
        fbDelay.clear(); fbHS.reset(); fbLS.reset();
    }

    void clear(){
        d1.clear(); d2.clear(); d3.clear();
        fbDelay.clear(); fbHS.reset(); fbLS.reset();
        s1.reset(); s2.reset(); s3.reset(); s4.reset();
        fbSample = 0.f;
    }

    // Process one sample.
    // Sz = size' (size * 100).
    // Dffs ∈ [0,1]
    // RT60 in seconds (global decay)
    // M1..M4 are time offsets in SECONDS added after smoothing; set 0 if unused.
    inline float process(float x,
                         float Sz, float Dffs, float RT60,
                         float hfHz, float hb_dB, float lfHz, float lb_dB,
                         float M1=0.f, float M2=0.f, float M3=0.f, float M4=0.f)
    {
        HF = hfHz; HB = hb_dB; LF = lfHz; LB = lb_dB;

        // ---- make four times (seconds) from Sz with +9,+6,+3,+0 offsets
        float T1 = s1.process(p2t(Sz + 9.0f)) + M1;
        float T2 = s2.process(p2t(Sz + 6.0f)) + M2;
        float T3 = s3.process(p2t(Sz + 3.0f)) + M3;
        float T4 = s4.process(p2t(Sz + 0.0f)) + M4; // feedback delay time

        // ---- input node (external in + previous feedback)
        float in = x + fbSample;

        // ---- three serial diffusers (frequency-dependent allpass)
        float y = d1.process(in, T1, Dffs, HF, HB, LF, LB);
        y      = d2.process(y,  T2, Dffs, HF, HB, LF, LB);
        y      = d3.process(y,  T3, Dffs, HF, HB, LF, LB);

        // ---- final shelf EQ on tank output (HS -> LS)
        fbHS.setCoeffs(setBL_HS(HF, HB, SR));
        y = fbHS.process(y);
        fbLS.setCoeffs(setBL_LS(LF, LB, SR));
        float out = fbLS.process(y);

        // ---- compute feedback for next sample: DelayH(T4) then AF
        float fbDelayed = fbDelay.process(out, T4, SR);
        float AF = rt60_to_AF(T4, RT60);
        fbSample = fbDelayed * AF;

        return out;
    }
};
