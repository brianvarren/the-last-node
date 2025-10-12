#include <cmath>
#include <cstdio>
#include <vector>
#include <random>
#include <algorithm>

// ---------- Minimal test harness ----------
static int g_fail = 0, g_pass = 0;
#define ASSERT_TRUE(msg, expr) do{ if(!(expr)){ \
    std::printf("[FAIL] %s (line %d)\n", msg, __LINE__); g_fail++; \
  } else { g_pass++; } }while(0)
#define ASSERT_NEAR(msg, a, b, eps) ASSERT_TRUE(msg, std::fabs((a)-(b)) <= (eps))

#include "OnePoleTPT.hpp"

// Reference coded slightly differently (should match bitwise within float noise)
struct OnePoleRef {
    explicit OnePoleRef(float sr = 48000.f) : sr(sr) { setCutoff(1000.f); }
    void setSampleRate(float s){ sr = std::max(1.f, s); setCutoff(fc); }
    void setCutoff(float hz){
        fc = std::clamp(hz, 0.f, 0.49f*sr);
        g  = std::tan(float(M_PI) * fc / sr);
    }
    void reset(float s0=0.f){ s = s0; }
    float processLP(float x){
        const float v = (x - s) * (g / (1.f + g));
        const float lp = v + s;
        s = lp + v;
        return lp;
    }
    float sr=48000.f, fc=1000.f, s=0.f, g=0.f;
};

// ---------- helpers ----------
static std::vector<float> makeSine(int n, float freq, float sr){
    std::vector<float> x(n); float ph=0.f, w=2.f*float(M_PI)*freq/sr;
    for(int i=0;i<n;++i){ x[i]=std::sin(ph); ph+=w; }
    return x;
}
static std::vector<float> makeNoise(int n){
    std::vector<float> x(n);
    std::mt19937 rng(0xC0FFEE); std::uniform_real_distribution<float> U(-1.f,1.f);
    for(int i=0;i<n;++i) x[i]=U(rng);
    return x;
}
static float rms(const std::vector<float>& v, int a, int b){
    double acc=0.0; int N=std::max(0,b-a);
    for(int i=a;i<b;++i) acc += double(v[i])*double(v[i]);
    return N? std::sqrt(float(acc/N)) : 0.f;
}

// ---------- tests ----------
static void test_complementarity(){
    std::puts("[TEST] Complementarity: lp + hp ≈ x");
    OnePoleTPT f(48000.f); f.setCutoff(9876.f);
    auto x = makeNoise(48000);
    double maxErr=0.0, meanErr=0.0;
    for(float xi : x){
        auto [lp,hp] = f.process(xi);
        double e = std::abs(double(lp+hp) - double(xi));
        maxErr = std::max(maxErr, e); meanErr += e;
    }
    meanErr /= x.size();
    ASSERT_NEAR("max error < 1e-6", maxErr, 0.0, 1e-6);
    ASSERT_NEAR("mean error < 1e-8", meanErr, 0.0, 1e-8);
}

static void test_minus3dB_at_fc(){
    std::puts("[TEST] -3 dB at cutoff (LP & HP RMS @ fc)");
    const float sr=48000.f, fc=4000.f, target=std::sqrt(0.5f);
    OnePoleTPT f(sr); f.setCutoff(fc);
    auto x = makeSine(4096, fc, sr);
    std::vector<float> lp(x.size()), hp(x.size());
    for(size_t i=0;i<x.size();++i){ auto y=f.process(x[i]); lp[i]=y.first; hp[i]=y.second; }
    int settle = int(sr/fc); // ~1 period
    float xR = rms(x, settle, x.size());
    float lpR = rms(lp, settle, lp.size());
    float hpR = rms(hp, settle, hp.size());
    ASSERT_NEAR("LP gain @ fc ~ 0.7071", lpR/xR, target, 0.01f);
    ASSERT_NEAR("HP gain @ fc ~ 0.7071", hpR/xR, target, 0.01f);
}

static void test_impulse_step(){
    std::puts("[TEST] Impulse/step sanity");
    const float sr=48000.f, fc=1234.f;
    OnePoleTPT f(sr); f.setCutoff(fc); f.reset(0.f);
    auto y0 = f.process(1.f);
    float g = std::tan(float(M_PI)*fc/sr);
    ASSERT_NEAR("impulse lp0 = g/(1+g)", y0.first,  g/(1.f+g), 1e-6f);
    ASSERT_NEAR("impulse hp0 = 1/(1+g)", y0.second, 1.f/(1.f+g), 1e-6f);

    // Step monotonic rise to 1, HP→0
    f.reset(0.f);
    float prevLP = -1e9f, lastHP=0.f;
    for(int n=0;n<2000;++n){
        auto y=f.process(1.f);
        ASSERT_TRUE("LP monotonic (allow tiny epsilon)", y.first >= prevLP - 1e-5f);
        prevLP = y.first; lastHP = y.second;
    }
    ASSERT_NEAR("LP→1 on DC", prevLP, 1.f, 1e-3f);
    ASSERT_NEAR("HP→0 on DC", lastHP, 0.f, 1e-3f);
}

static void test_modulation_bounded(){
    std::puts("[TEST] Fast cutoff modulation is bounded");
    const float sr=48000.f;
    OnePoleTPT f(sr); f.reset(0.f);
    auto x = makeSine(48000, 2000.f, sr);
    float maxAbs=0.f;
    for(int n=0;n<(int)x.size();++n){
        float t = float(n)/sr;
        float fc = 50.f + 0.45f*sr * 0.5f * (1.f + std::sin(2.f*float(M_PI)*2.f*t)); // 2 Hz sweep
        f.setCutoff(fc);
        auto y = f.process(x[n]);
        maxAbs = std::max({maxAbs, std::abs(y.first), std::abs(y.second)});
    }
    ASSERT_TRUE("outputs bounded (<5 for |x|<=1)", maxAbs < 5.f);
}

static void test_reference_equivalence(){
    std::puts("[TEST] Reference equivalence (alt coding of same math)");
    const float sr=48000.f, fc=1234.5f;
    OnePoleTPT a(sr); a.setCutoff(fc); a.reset(0.f);
    OnePoleRef b(sr); b.setCutoff(fc); b.reset(0.f);
    auto x = makeNoise(10000);
    for(float xi : x){
        float ya = a.process(xi).first;
        float yb = b.processLP(xi);
        ASSERT_NEAR("LP match within 1e-7", ya, yb, 1e-7f);
    }
}

int main(){
    std::puts("== OnePole (TPT) unit tests, no deps ==");
    test_complementarity();
    test_minus3dB_at_fc();
    test_impulse_step();
    test_modulation_bounded();
    test_reference_equivalence();
    std::printf("\nSummary: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
