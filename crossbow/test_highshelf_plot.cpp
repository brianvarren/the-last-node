// test_highshelf_plot.cpp
// Build & run:
//   g++ -std=c++17 -O2 test_highshelf_plot.cpp && ./a.out
//
// Optional args:
//   ./a.out           # run numeric tests + ASCII plot (default fc=1000, +6 dB)
//   ./a.out 48000 1000 6     # fs, fc, dB
//   ./a.out 48000 1000 6 csv # also write magnitude.csv
//
// To gnuplot quickly:
//   gnuplot -e "set logscale x; set grid; plot 'magnitude.csv' w l"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>

#include "filters.hpp"

// --------------------- helpers ---------------------
static float magAt(const OnePoleHighShelfBLT& f, float fHz){
    const float w = 2.0f*float(M_PI)*(fHz/f.sampleRate);
    const float zr = std::cos(w), zi = -std::sin(w); // z^-1
    const float numRe = f.b0 + f.b1*zr;
    const float numIm =         f.b1*zi;
    const float denRe = 1.0f + f.a1*zr;
    const float denIm =         f.a1*zi;
    const float num2 = numRe*numRe + numIm*numIm;
    const float den2 = denRe*denRe + denIm*denIm;
    return std::sqrt(num2/den2);
}

// quick ASCII plot (log-x) for human eyes
static void asciiPlot(const std::vector<float>& fx, const std::vector<float>& mdB,
                      float fs, float fc, float dB){
    const int W = 78, H = 24;
    std::vector<std::string> grid(H, std::string(W,' '));

    // y range auto: pad 3 dB
    float minv = *std::min_element(mdB.begin(), mdB.end());
    float maxv = *std::max_element(mdB.begin(), mdB.end());
    minv = std::floor((minv-3.f)/3.f)*3.f;
    maxv = std::ceil((maxv+3.f)/3.f)*3.f;

    for (int i=0;i<W;i++){ // vertical grid lines at decades
        grid[H-1][i] = '-';
    }
    // plot curve
    for (size_t n=0;n<fx.size();++n){
        float xFrac = (std::log10(fx[n]) - std::log10(20.f)) /
                      (std::log10(0.49f*fs) - std::log10(20.f));
        int x = std::clamp(int(xFrac*(W-1)+0.5f), 0, W-1);
        float yFrac = (mdB[n]-minv)/(maxv-minv);
        int y = std::clamp(H-2 - int(yFrac*(H-2)+0.5f), 0, H-2);
        grid[y][x] = '*';
    }

    std::printf("\nASCII magnitude (dB)  fs=%.0f  fc=%.0f  gain=%.1f dB\n", fs, fc, dB);
    for (int r=0;r<H;r++){
        float yLabel = minv + (maxv-minv)*(float(H-2-r)/(H-2));
        if (r < H-1) std::printf("%7.2f | ", yLabel);
        else         std::printf("        ");
        std::fwrite(grid[r].data(),1,grid[r].size(),stdout);
        std::printf("\n");
    }
    std::printf("        20Hz%s%.0fHz%s%.0fHz (log-x)\n",
                std::string(W/3-4,' ').c_str(), fs*0.01f,
                std::string(W/3-3,' ').c_str(), fs*0.49f);
}

int main(int argc, char** argv){
    // params
    float fs = 48000.f, fc = 1000.f, dBgain = 6.f;
    bool writeCSV = false;
    if (argc >= 2) fs = std::atof(argv[1]);
    if (argc >= 3) fc = std::atof(argv[2]);
    if (argc >= 4) dBgain = std::atof(argv[3]);
    if (argc >= 5) writeCSV = std::string(argv[4])=="csv";

    OnePoleHighShelfBLT hs;
    hs.setSampleRate(fs);
    hs.setCutoff(fc);
    hs.setGainDb(dBgain);

    // quick pass/fail checks (DC, HF, |H(fc)|)
    int fails = 0;
    auto pr = [&](const char* tag, float got, float want, float tolRel){
        float ok = std::fabs(got-want) <= tolRel*std::max(1.f,std::fabs(want));
        std::printf("%-12s %.3f (want ~%.3f)%s\n", tag, got, want, ok?"":"  **FAIL**");
        if(!ok) ++fails;
    };
    float A = std::pow(10.f, dBgain/20.f);
    pr("DC gain",  magAt(hs, 0.1f), 1.0f, 0.02f);
    pr("HF gain",  magAt(hs, 0.49f*fs), A, 0.05f);
    pr("|H(fc)|",  magAt(hs, fc), std::sqrt(0.5f*(A*A+1.f)), 0.05f);

    // make log-spaced frequencies (20 Hz .. 0.49*fs)
    const int N = 400;
    const float fLo = 20.f, fHi = 0.49f*fs;
    std::vector<float> f(N), mdB(N);
    for (int i=0;i<N;i++){
        float a = i/(N-1.f);
        float fHz = std::pow(10.f, std::log10(fLo) + a*(std::log10(fHi)-std::log10(fLo)));
        f[i] = fHz;
        mdB[i] = 20.f*std::log10(std::max(1e-20f, magAt(hs, fHz)));
    }

    // ASCII plot
    asciiPlot(f, mdB, fs, fc, dBgain);

    // optional CSV
    if (writeCSV){
        FILE* fp = std::fopen("magnitude.csv","w");
        if (fp){
            std::fprintf(fp, "freq_hz,mag,mag_db\n");
            for (int i=0;i<N;i++) std::fprintf(fp, "%.6f,%.9f,%.6f\n",
                                               f[i], std::pow(10.f, mdB[i]/20.f), mdB[i]);
            std::fclose(fp);
            std::printf("wrote magnitude.csv\n");
        } else std::printf("could not open magnitude.csv for writing\n");
    }

    if (fails){ std::printf("RESULT: %d failure(s)\n", fails); return 1; }
    std::printf("RESULT: all tests passed\n");
    return 0;
}
