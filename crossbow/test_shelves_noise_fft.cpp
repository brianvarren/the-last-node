// test_shelves_impulse_fft.cpp
// Measure LS/HS frequency response using impulse response + FFT

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <complex>
#include <utility>

// ================= Core-faithful helpers =================
static constexpr float kSR = 48000.0f;

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

// BiLin I (DF-I) with two memories
struct BiLin1P {
    float a1{0}, b0{1}, b1{0};
    float x1{0}, y1{0};
    inline void reset(){ x1 = 0; y1 = 0; }
    inline float process(float x){
        float y = b0*x + b1*x1 + a1*y1;
        x1 = x; y1 = y; return y;
    }
};

// ================= FFT =================
struct FFT {
    int N = 0;
    std::vector<std::complex<double>> tw;
    void init(int n){
        N = n; tw.resize(N);
        const double twoPiOverN = -2.0*M_PI/N;
        for(int k=0;k<N;++k) tw[k] = std::polar(1.0, twoPiOverN*k);
    }
    void run(std::vector<std::complex<double>>& a) const {
        // bit-reverse
        int j=0;
        for(int i=1;i<N;i++){
            int bit = N>>1;
            for(; j&bit; bit>>=1) j ^= bit;
            j ^= bit;
            if(i<j) std::swap(a[i],a[j]);
        }
        for(int len=2; len<=N; len<<=1){
            int half=len>>1; int step=N/len;
            for(int i=0;i<N;i+=len){
                int k=0;
                for(int j2=0;j2<half;++j2){
                    auto u = a[i+j2];
                    auto v = a[i+j2+half] * tw[k];
                    a[i+j2]      = u + v;
                    a[i+j2+half] = u - v;
                    k += step;
                }
            }
        }
    }
};

// ================= ASCII plot =================
static void ascii_plot(const char* title,
                       const std::vector<double>& dB,
                       double sr, int nfft,
                       double fmin=20.0, double fmax=20000.0)
{
    std::printf("\n%s\n", title);
    const int cols = 70;
    const int bins = (int)dB.size();
    for(int r=0;r<40;++r){
        double t = (double)r / 39.0;
        double f = fmin * std::pow(fmax/fmin, t);
        int bin = (int)std::round(f * (nfft) / sr);
        bin = std::clamp(bin, 1, bins-1);
        double val = dB[bin];
        double mn = -12.0, mx = +12.0; // ±12 dB span
        int col = (int)std::round((val-mn)/(mx-mn) * (cols-1));
        col = std::clamp(col, 0, cols-1);
        int ax  = (int)std::round((0.0 - mn)/(mx-mn) * (cols-1));
        for(int c=0;c<cols;++c){
            if (c==col) std::putchar('*');
            else if (c==ax) std::putchar('|');
            else std::putchar(' ');
        }
        std::printf("  %6.0f Hz  %+.2f dB\n", f, val);
    }
}

static void check(const char* name, bool ok, double val){
    std::printf("[%s] %s  (%.2f dB)\n", ok?"PASS":"FAIL", name, val);
}

// ================= Run measurement =================
int main(){
    const float Fc = 1000.0f;     // cutoff frequency
    const float dBamt = +6.0f;    // shelf amount
    const int   N = 1<<14;        // 16384 samples for impulse response (good resolution)

    // Build filters
    Coeff lsp = setBL_LS(Fc, dBamt, kSR);    // LS +6
    Coeff lsn = setBL_LS(Fc,-dBamt, kSR);    // LS -6
    Coeff hsp = setBL_HS(Fc, dBamt, kSR);    // HS +6
    Coeff hsn = setBL_HS(Fc,-dBamt, kSR);    // HS -6

    // Measure impulse response and compute frequency response
    auto measure = [&](const Coeff& c){
        BiLin1P f; 
        f.a1=c.a1; f.b0=c.b0; f.b1=c.b1; 
        f.reset();
        
        // Generate impulse response
        std::vector<float> h(N);
        for(int n=0; n<N; ++n){
            float x = (n == 0) ? 1.0f : 0.0f;  // impulse
            h[n] = f.process(x);
        }
        
        // FFT to get frequency response
        std::vector<std::complex<double>> H(N);
        for(int n=0; n<N; ++n) H[n] = std::complex<double>(h[n], 0.0);
        
        FFT fft;
        fft.init(N);
        fft.run(H);
        
        // Convert to magnitude in dB (one-sided spectrum)
        std::vector<double> mag_db(N/2 + 1);
        for(int k=0; k<=N/2; ++k){
            double mag = std::abs(H[k]);
            mag_db[k] = 20.0 * std::log10(std::max(mag, 1e-20));
        }
        
        return mag_db;
    };

    auto dLS_p = measure(lsp);
    auto dLS_n = measure(lsn);
    auto dHS_p = measure(hsp);
    auto dHS_n = measure(hsn);

    // Extract gain at specific frequencies
    auto pick = [&](const std::vector<double>& d, double f){
        int bin = (int)std::round(f * N / kSR);
        bin = std::clamp(bin, 1, (int)d.size()-1);
        return d[bin];
    };

    double fLow=40.0, fMid=Fc, fHigh=12000.0;
    
    std::printf("\n========== VERIFICATION ==========\n");
    
    // LS +6 : low - high ~ +6 dB
    check("LS +6 dB tilt (low - high ≈ +6 dB)",
          std::fabs((pick(dLS_p,fLow) - pick(dLS_p,fHigh)) - 6.0) <= 0.5,
          (pick(dLS_p,fLow) - pick(dLS_p,fHigh)));
    
    // LS -6 : low - high ~ -6 dB
    check("LS -6 dB tilt (low - high ≈ -6 dB)",
          std::fabs((pick(dLS_n,fLow) - pick(dLS_n,fHigh)) + 6.0) <= 0.5,
          (pick(dLS_n,fLow) - pick(dLS_n,fHigh)));
    
    // HS +6 : high - low ~ +6 dB
    check("HS +6 dB tilt (high - low ≈ +6 dB)",
          std::fabs((pick(dHS_p,fHigh) - pick(dHS_p,fLow)) - 6.0) <= 0.5,
          (pick(dHS_p,fHigh) - pick(dHS_p,fLow)));
    
    // HS -6 : high - low ~ -6 dB
    check("HS -6 dB tilt (high - low ≈ -6 dB)",
          std::fabs((pick(dHS_n,fHigh) - pick(dHS_n,fLow)) + 6.0) <= 0.5,
          (pick(dHS_n,fHigh) - pick(dHS_n,fLow)));
    
    // Check gain at cutoff frequency (should be ~±3 dB for shelves)
    std::printf("\nGain at cutoff frequency (%g Hz):\n", fMid);
    std::printf("  LS +6: %+.2f dB (expect ~+3 dB)\n", pick(dLS_p, fMid));
    std::printf("  LS -6: %+.2f dB (expect ~-3 dB)\n", pick(dLS_n, fMid));
    std::printf("  HS +6: %+.2f dB (expect ~+3 dB)\n", pick(dHS_p, fMid));
    std::printf("  HS -6: %+.2f dB (expect ~-3 dB)\n", pick(dHS_n, fMid));

    // Plots
    ascii_plot("Low Shelf +6 dB (Frequency Response)", dLS_p, kSR, N);
    ascii_plot("Low Shelf -6 dB (Frequency Response)", dLS_n, kSR, N);
    ascii_plot("High Shelf +6 dB (Frequency Response)", dHS_p, kSR, N);
    ascii_plot("High Shelf -6 dB (Frequency Response)", dHS_n, kSR, N);

    return 0;
}