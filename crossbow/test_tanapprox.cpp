#include <cmath>
#include <cstdio>
#include <vector>

// 5th-order polynomial tan approximation (from your macro)
inline float tanApprox(float x) {
    const float x2 = x * x;
    return x * (1.0f + x2 * (0.333333f + x2 * 0.133333f));
}

int main() {
    // Ten test values across a realistic prewarp range (±1.0 rad)
    std::vector<float> testVals = {
        -1.0f, -0.75f, -0.5f, -0.25f, -0.1f,
         0.0f,  0.1f,   0.25f,  0.5f,  0.75f
    };

    std::puts("------------------------------------------------------------");
    std::puts("tanApprox vs std::tan (values in radians)");
    std::puts("------------------------------------------------------------");
    std::puts("    x(rad)    tanApprox(x)     std::tan(x)    abs diff");
    std::puts("------------------------------------------------------------");

    for (float x : testVals) {
        float approx = tanApprox(x);
        float exact  = std::tan(x);
        float diff   = std::fabs(approx - exact);
        std::printf("%10.5f %15.8f %15.8f %12.8f\n", x, approx, exact, diff);
    }

    std::puts("------------------------------------------------------------");
    std::puts("Done.");
    std::puts("------------------------------------------------------------");
    return 0;
}
