#include <cmath>
#include <utility>

// WarpLS: two outputs {F', A} from inputs {F, dB}
// r = base^dB, F' = F / r, A = r^2
inline std::pair<float,float> warpLS(float F_hz, float dB) {
    const float base = 1.059253692626953f;       // literal from Core
    float r = std::pow(base, dB);                // Core ~exp(x) with Base
    float Fp = F_hz / r;                         // frequency warp
    float A  = r * r;                            // square (multiply block)
    return {Fp, A};
}

#include <cstdio>
int main() {
    float F = 1000.0f;                           // try any F you like
    for (float dB : {-12,-6,-3,-1,0,1,3,6,12}) {
        auto [Fp, A] = warpLS(F, dB);
        std::printf("dB=%4.0f  F_in=%7.2f  F_out=%9.4f  A=%1.8f\n", dB, F, Fp, A);
    }
}
