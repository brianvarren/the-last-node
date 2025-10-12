// test_delayh.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

// ---------- helpers (stateless, one-liners) ----------
inline float clipDelay(float x) {
    // faithful to your Clip macro: [0, 192000-2] = [0, 191998]
    return std::clamp(x, 0.0f, 192000.0f - 2.0f);
}

inline std::pair<float,float> intfract(float x) {
    // fractional part returned, integer part written (float precision)
    float i;
    float f = modff(x, &i);
    return {i, f};
}

// 4-point Hermite/Catmull–Rom as in your Interpol(4pt)
inline float interp4(float xm1, float x0, float x1, float x2, float t) {
    float a0 = -0.5f*xm1 + 1.5f*x0 - 1.5f*x1 + 0.5f*x2;
    float a1 =  xm1     - 2.5f*x0 + 2.0f*x1 - 0.5f*x2;
    float a2 = -0.5f*xm1 + 0.5f*x1;
    return ((a0*t + a1)*t + a2)*t + x0;
}

// ---------- DelayH (192k, record + 4x play + Interpol(4pt)) ----------
struct DelayH {
	static constexpr int N = 192000; // buffer size
    std::array<float,N> buf{}; 
    int w = 0;

    void clear() { buf.fill(0.0f); w = 0; }

    // T is normalized [0,1]; SR is sample rate (Hz)
    inline float process(float in, float T, float SR) {
        buf[w] = in;

        // convert to samples first, then clip to [0, N-2]
        const float dSmps = clipDelay(T * SR);

        float r = float(w) - dSmps;              // read position
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

// ---------- tiny test harness ----------
static void printLine() { std::puts("----------------------------------------------------------------"); }

static void dumpFirst(const std::vector<float>& v, int max = 24) {
    for (int n = 0; n < (int)v.size() && n < max; ++n) {
        std::printf("%10.6f%s", v[n], ((n % 8) == 7) ? "\n" : " ");
    }
    if (max < (int)v.size()) std::puts("...");
}

static void impulseProbe(float delay, int frames = 512) {
    DelayH d; d.clear();
    std::vector<float> out; out.reserve(frames);
    float peak = 0.f; int pidx = -1;

    for (int n=0; n<frames; ++n) {
        float y = d.process(n==0 ? 1.0f : 0.0f, delay);
        out.push_back(y);
        float ay = std::fabs(y);
        if (ay > peak) { peak = ay; pidx = n; }
    }

    printLine();
    std::printf("Impulse test  delay=%.2f samples  (expect main lobe around n≈%.2f)\n", delay, delay);
    dumpFirst(out, 24);
    std::printf("\npeak@ n=%d  amp=%.6f\n\n", pidx, peak);
}

static void boundaryProbe() {
    DelayH d; d.clear();
    printLine();
    std::puts("Boundary checks (near 0 and near 191998):");

    // very small delay -> output follows input almost immediately
    for (float D : {0.0f, 0.1f, 0.49f}) {
        d.clear();
        float p = 0.f; int pi=-1;
        for (int n=0;n<64;++n){
            float y = d.process(n==0?1.f:0.f, D);
            if (std::fabs(y)>p){p=std::fabs(y);pi=n;}
        }
        std::printf("delay=%7.2f  peak@%2d  amp=%.6f\n", D, pi, p);
    }

    // near the maximum legal delay
    for (float D : {191996.f, 191997.f, 191998.f, 191999.f, 192050.f}) {
        d.clear();
        float Dc = clipDelay(D);
        float p = 0.f; int pi=-1;
        for (int n=0;n<256;++n){
            float y = d.process(n==0?1.f:0.f, Dc);
            if (std::fabs(y)>p){p=std::fabs(y);pi=n;}
        }
        std::printf("delay(in)=%9.2f  delay(clamped)=%9.2f  peak@%3d  amp=%.6f\n", D, Dc, pi, p);
    }
}

static void dcDriftProbe() {
    // feed steady 1.0 through a *constant* fractional delay; output should be ~1.0 (no DC drift)
    DelayH d; d.clear();
    const float D = 17.5f;
    const int N = 4096;
    double sum = 0.0, sumsq = 0.0;
    for (int n=0;n<N;++n) {
        float y = d.process(1.0f, D);
        sum   += y;
        sumsq += double(y)*double(y);
    }
    double mean = sum / N;
    double rms  = std::sqrt(sumsq / N);
    printLine();
    std::printf("DC test @ delay=%.2f  mean=%.8f  rms=%.8f  (expect mean≈1.0)\n", D, mean, rms);
}

int main() {
    printLine(); std::puts("DelayH single-file test");

    // impulse probes at your canonical delays
    impulseProbe(3.25f);
    impulseProbe(17.5f);
    impulseProbe(123.9f);

    // boundary behaviour
    boundaryProbe();

    // crude DC sanity (helps catch indexing/sign mistakes)
    dcDriftProbe();

    printLine(); std::puts("Done.");
    return 0;
}
