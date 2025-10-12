// render_rt60_defs.cpp
#include <vector>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "latediff_tank.hpp"   // has mapSize01ToLTsemi, tank, LFO, smoothers

// ======================= User Params (compile-time) =======================
#define SIZE   0.746f    // knob in [0..1]
#define SYM    0.25f     // stereo skew, recommended [-1..1] in knob units
#define DFFS   0.96f     // [0..1]
#define RT60   74.6094f  // seconds (render length after impulse)
#define MODF   0.63f     // Hz (8-phase LFO rate)
#define MODA   0.0006f   // seconds (± delay modulation depth)
#define HF     4185.0f   // Hz high shelf freq
#define HB       0.0f    // dB high shelf gain
#define LF       65.0f   // Hz low shelf freq
#define LB     -17.0f    // dB low shelf gain
#define SR      48000    // sample rate (Hz)
#define PREROLL_SEC 0.30f // unrecorded preroll to settle smoothers
// ========================================================================

// Stereo late-diffusion with 8-phase LFO + z^-1 cross-feedback
struct StereoLateDiff {
    float SRf{float(SR)};
    LateDiffTank L{SRf}, R{SRf};
    LFO8Parabolic lfo;
    SmoothHalfLife sSize, sSym;
    float zL{0.f}, zR{0.f};

    StereoLateDiff(){ setSampleRate(SRf); }
    void setSampleRate(float sr){
        SRf = sr;
        L.setSampleRate(sr); R.setSampleRate(sr);
        lfo.setSR(sr);
        sSize.setSR(sr); sSize.setHalf(0.05f);
        sSym .setSR(sr); sSym .setHalf(0.05f);
    }
    void clear(){
        L.clear(); R.clear(); lfo.reset(0.f);
        zL = zR = 0.f;
        sSize.reset(0.f); sSym.reset(0.f);
    }

    inline void process(float inL, float inR, float& outL, float& outR){
        // Smooth the knobs
        const float sizeSm = sSize.process(SIZE); // [0..1]
        const float symSm  = sSym .process(SYM);  // typically [-1..1]

        // Combine size±sym in knob domain, clamp to knob range, then map to LT index
        const float uL = std::clamp(sizeSm + symSm, 0.0f, 1.0f);
        const float uR = std::clamp(sizeSm - symSm, 0.0f, 1.0f);
        const float SzL = mapSize01ToLTsemi(uL);  // [+48 … -24] semitone-like index
        const float SzR = mapSize01ToLTsemi(uR);

        // 8-phase LFO → seconds offsets
        auto ph = lfo.process(MODF);
        const float M1 = MODA * ph[0], M2 = MODA * ph[1], M3 = MODA * ph[2], M4 = MODA * ph[3];
        const float M5 = MODA * ph[4], M6 = MODA * ph[5], M7 = MODA * ph[6], M8 = MODA * ph[7];

        // Cross-feedback (one-sample delay, unity)
        const float inLx = inL + zR;
        const float inRx = inR + zL;

        const float yL = L.process(inLx, SzL, DFFS, RT60, HF, HB, LF, LB, M1, M2, M3, M4);
        const float yR = R.process(inRx, SzR, DFFS, RT60, HF, HB, LF, LB, M5, M6, M7, M8);

        zL = yL; zR = yR;
        outL = yL; outR = yR;
    }
};

// Minimal WAV writer (float32 stereo)
static void write_wav_float32(const char* path, const std::vector<float>& interleaved,
                              uint32_t sampleRate, uint16_t numCh){
    uint32_t bps = 4, dataSize = interleaved.size()*bps;
    uint32_t riffSize = 4 + (8+16) + (8+dataSize);
    FILE* f = std::fopen(path, "wb"); if(!f){ std::perror("fopen"); return; }
    auto W4=[&](uint32_t v){ std::fwrite(&v,1,4,f); };
    auto W2=[&](uint16_t v){ std::fwrite(&v,1,2,f); };
    std::fwrite("RIFF",1,4,f); W4(riffSize);
    std::fwrite("WAVE",1,4,f);
    std::fwrite("fmt ",1,4,f); W4(16);
    W2(3); W2(numCh); W4(sampleRate);
    uint16_t blockAlign = numCh*bps; W4(sampleRate*blockAlign); W2(blockAlign); W2(32);
    std::fwrite("data",1,4,f); W4(dataSize);
    std::fwrite(interleaved.data(),1,dataSize,f); std::fclose(f);
}

int main(){
    StereoLateDiff tank; tank.clear();

    // preroll (not recorded)
    const int pre = int(PREROLL_SEC * SR);
    for(int n=0;n<pre;++n){ float oL,oR; tank.process(0.f,0.f,oL,oR); }

    // render impulse for exactly RT60 seconds
    const int N = int(std::ceil(RT60 * SR));
    std::vector<float> buf; buf.reserve(size_t(N)*2);

    for(int n=0;n<N;++n){
        const float in = (n==0)?1.f:0.f;
        float oL,oR; tank.process(in,in,oL,oR);
        buf.push_back(oL); buf.push_back(oR);
    }

    write_wav_float32("lateDiff_RT60.wav", buf, SR, 2);
    std::printf("Wrote lateDiff_RT60.wav (%d frames @ %d Hz)\n", N, SR);
    return 0;
}
