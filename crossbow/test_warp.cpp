#include <cstdio>
#include <cmath>
#include <vector>

// --- 5th-order tan approximation (as before) ---
inline float tanApprox(float x) {
    const float x2 = x * x;
    return x * (1.0f + x2 * (0.333333f + x2 * 0.133333f));
}

// --- Warp (exactly as in your Reaktor macro) ---
// Warp = tanApprox(F * (3.14159 / SR))
inline float warp(float F_hz, float sampleRate) {
    const float k = 3.14159f / sampleRate;  // literal, truncated π
    return tanApprox(F_hz * k);
}


int main() {
    const float sr = 48000.0f;
    std::vector<float> F = {5, 20, 60, 120, 500, 1000, 2000, 4000, 8000, 12000};

    std::puts("Warp = tanApprox(F * (3.14159 / SR))");
    std::puts("------------------------------------------------------------");
    std::puts("   F(Hz)     warp≈(3.14159)   warp(exact M_PI)    abs diff");
    std::puts("------------------------------------------------------------");

    for (float f : F) {
        float a = tanApprox(f * (3.14159f / sr));
        float e = std::tan(f * (float(M_PI) / sr));  // reference
        std::printf("%8.1f  %15.8f  %15.8f  %12.8f\n",
                    f, a, e, std::fabs(a - e));
    }

    std::puts("------------------------------------------------------------");
    std::puts("Done.");
    return 0;
}

