// test_macros.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>
#include <cstdlib>

// ---- your helpers (as you wrote them) ----
inline float clipDelay(float x) {
    return std::clamp(x, 0.0f, 192000.0f - 2.0f); // [0, 191998]
}

inline std::pair<float,float> intfract(float x) {
    float i;
    float f = modff(x, &i);   // returns frac, stores int
    return {i, f};
}

// ---- utilities ----
static inline void line() { std::puts("------------------------------------------------------------"); }

static inline void header(const char* title) {
    line(); std::puts(title); line();
}

int main() {
    // ---------------- ClipDelay tests ----------------
    header("ClipDelay: clamp to [0, 191998]");

    // Spot tests (explicit edge cases)
    {
        const float tests[] = {-1000.0f, -1.0f, 0.0f, 1.0f, 191997.0f, 191998.0f, 191999.0f, 192100.0f};
        std::puts("Spot tests:");
        for (float x : tests) {
            float y = clipDelay(x);
            std::printf("in=%9.1f  out=%9.1f  delta=%9.1f\n", x, y, y - x);
        }
    }

    // Sweep a small window around the upper bound
    {
        std::puts("\nUpper-bound sweep (191996..192002):");
        for (int k = 191996; k <= 192002; ++k) {
            float x = float(k);
            float y = clipDelay(x);
            std::printf("in=%9.1f  out=%9.1f  %s\n", x, y, (y == 191998.0f ? "<- CLIPPED" : ""));
        }
    }

    // ---------------- IntFract tests ----------------
    header("IntFract: split x into Int=floor(x), Fract=x-Int");

    // Spot tests including boundaries
    {
        const float tests[] = {0.0f, 0.25f, 0.99f, 1.0f, 1.75f, 17.5f, 123.9f, 191997.99f};
        std::puts("Spot tests:");
        for (float x : tests) {
            auto [i, f] = intfract(x);
            float sum = i + f;
            float err = sum - x;
            std::printf("x=%10.5f  Int=%9.5f  Fract=%8.5f  sum=%10.5f  err=%+.6g\n",
                        x, i, f, sum, err);
        }
    }

    // Randomized regression (non-negative inputs like in the delay read path)
    {
        std::puts("\nRandomized check (0..200000): sum==x and Fract in [0,1)");
        std::srand(12345);
        int N = 10000;
        int ok = 0, bad = 0;
        for (int n=0; n<N; ++n) {
            float x = (float(std::rand()) / float(RAND_MAX)) * 200000.0f;
            auto [i, f] = intfract(x);
            float sum = i + f;
            float err = std::fabs(sum - x);
            bool fractInRange = (f >= 0.0f && f < 1.0f);
            if (err < 1e-6f && fractInRange) ++ok; else ++bad;
        }
        std::printf("Random trials: ok=%d  bad=%d\n", ok, bad);
    }

    // Combined sanity: pass through clipDelay then intfract
    {
        header("Combined: clipDelay -> intfract (monotonic & consistent)");
        const float tests[] = {-1000.0f, 0.0f, 191997.2f, 191998.7f, 192123.4f};
        for (float x : tests) {
            float c = clipDelay(x);
            auto [i, f] = intfract(c);
            std::printf("in=%10.2f  clamped=%10.2f  Int=%9.2f  Fract=%6.3f\n", x, c, i, f);
        }
    }

    line();
    std::puts("Done.");
    line();
    return 0;
}

