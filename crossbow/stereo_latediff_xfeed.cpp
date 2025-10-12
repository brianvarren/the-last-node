// stereo_latediff_xfeed.cpp
#include <cstdio>
#include "latediff_tank.hpp"

#define SLD_DEMO  // Define to enable main() demo at bottom

// Stereo late-diffusion with 8-phase LFO and cross-feedback (z^-1)
struct StereoLateDiff {
    float SR{48000.f};
    LateDiffTank L{SR}, R{SR};
    LFO8Parabolic lfo;

    // z^-1 states for crossfeed (delayed by one sample)
    float zL{0.f}, zR{0.f};

    // Parameter smoothers (to mirror your Smooth 50 ms)
    SmoothHalfLife sSize, sSym;

    StereoLateDiff(float sr=48000.f){ setSampleRate(sr); }

    void setSampleRate(float sr){
        SR = sr > 1.f ? sr : 1.f;
        L.setSampleRate(SR);
        R.setSampleRate(SR);
        lfo.setSR(SR);
        sSize.setSR(SR); sSize.setHalf(0.05f);
        sSym .setSR(SR); sSym .setHalf(0.05f);
    }

    void clear(){
        L.clear(); R.clear(); lfo.reset(0.f);
        zL = zR = 0.f;
        sSize.reset(0.f); sSym.reset(0.f);
    }

    // One-sample tick
    // - size01 in [0,1]; sym skews size between L/R (same units as size01)
    // - dffs [0,1]; rt60 seconds
    // - shelves: hf/hb_dB and lf/lb_dB
    // - modFreq_Hz, modDepth_sec (seconds added to T1..T4)
    inline void process(float inL, float inR,
                        float size01, float sym,
                        float dffs, float rt60_sec,
                        float hf, float hb_dB, float lf, float lb_dB,
                        float modFreq_Hz, float modDepth_sec,
                        float& outL, float& outR)
    {
        // settle GUI params
        float sizeSm = sSize.process(size01);
        float symSm  = sSym .process(sym);

        // Size to Sz’ (size*100) and stereo skew
        float SzL = (sizeSm + symSm) * 100.0f;
        float SzR = (sizeSm - symSm) * 100.0f;

        // 8-phase parabolic LFO → seconds (M1..M8)
        auto ph = lfo.process(modFreq_Hz);
        float M1 = modDepth_sec * ph[0];
        float M2 = modDepth_sec * ph[1];
        float M3 = modDepth_sec * ph[2];
        float M4 = modDepth_sec * ph[3];
        float M5 = modDepth_sec * ph[4];
        float M6 = modDepth_sec * ph[5];
        float M7 = modDepth_sec * ph[6];
        float M8 = modDepth_sec * ph[7];

        // --- External cross-feedback (one-sample delay, unity gain)
        // L input gets delayed right output; R input gets delayed left output.
        float inLx = inL + zR;
        float inRx = inR + zL;

        // --- Process tanks
        float yL = L.process(inLx, SzL, dffs, rt60_sec, hf, hb_dB, lf, lb_dB, M1, M2, M3, M4);
        float yR = R.process(inRx, SzR, dffs, rt60_sec, hf, hb_dB, lf, lb_dB, M5, M6, M7, M8);

        // --- Update z^-1 states AFTER using them this sample
        zL = yL;
        zR = yR;

        outL = yL;
        outR = yR;
    }
};

// ------------------- tiny impulse sanity run -------------------
#ifdef SLD_DEMO
int main(){
    const float SR = 48000.f;
    StereoLateDiff s(SR);
    s.clear();

    float size=0.45f, sym=0.10f, dffs=0.65f, rt60=2.0f;
    float hf=8000.f, hb=-2.f, lf=200.f, lb=+1.f;
    float modF=0.2f, modA=0.0015f; // ±1.5 ms

    // preroll
    for (int n=0; n<int(0.3f*SR); ++n){
        float oL,oR;
        s.process(0,0,size,sym,dffs,rt60,hf,hb,lf,lb,modF,modA,oL,oR);
    }

    // impulse
    for (int n=0; n<64; ++n){
        float in = (n==0)?1.f:0.f;
        float oL,oR;
        s.process(in,in,size,sym,dffs,rt60,hf,hb,lf,lb,modF,modA,oL,oR);
        std::printf("[%02d] L=%+.6f  R=%+.6f\n", n, oL, oR);
    }
    return 0;
}
#endif
