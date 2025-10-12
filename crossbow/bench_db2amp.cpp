#include <cmath>
#include <chrono>
#include <iostream>
#include <random>
#include <vector>
#include <iomanip>
#include <cstdlib>

// --- Implementations ---------------------------------------------------------


// Use this one:
// 10^(dB/20) == exp(ln(10)/20 * dB)  (single-precision)
inline float dB2amp_expf(float dB) noexcept {
    return expf(0.11512925464970229f * dB); // ln(10)/20
}

// These two are slower, for comparison:
// Reference (theoretical) version
inline float dB2amp_pow10(float dB) noexcept {
    return powf(10.0f, dB / 20.0f);
}

// Reaktor macro literal: pow(base, dB) where base = 10^(1/20)
inline float dB2amp_pow_base(float dB) noexcept {
    return powf(1.12201845456459045f, dB);
}


// --- Micro-benchmark harness -------------------------------------------------

template<typename Fn>
static double bench(const std::vector<float>& xs, Fn&& f, volatile float& sink) {
    using clock = std::chrono::high_resolution_clock;

    // warm-up (helps avoid first-call penalties)
    for (size_t i = 0; i < 10000 && i < xs.size(); ++i) sink += f(xs[i]);

    auto t0 = clock::now();
    for (float x : xs) sink += f(x);
    auto t1 = clock::now();

    std::chrono::duration<double> dt = t1 - t0;
    return dt.count();
}

int main(int argc, char** argv) {
    // Number of operations (default 10M)
    size_t N = (argc > 1) ? static_cast<size_t>(std::atoll(argv[1])) : 10'000'000ULL;

    // Generate random dB inputs across a realistic range [-60 dB, +24 dB]
    std::mt19937 rng(0xC0FFEE);
    std::uniform_real_distribution<float> dist(-60.0f, 24.0f);
    std::vector<float> xs; xs.reserve(N);
    for (size_t i = 0; i < N; ++i) xs.push_back(dist(rng));

    // Use a volatile sink so the compiler can't optimize the calls away
    volatile float sink = 0.0f;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "N = " << N << " samples\n";

    // Measure
    double t_exp  = bench(xs, dB2amp_expf,      sink);
    double t_p10  = bench(xs, dB2amp_pow10,     sink);
    double t_pbas = bench(xs, dB2amp_pow_base,  sink);

    auto report = [N](const char* name, double seconds){
        const double ns_per = (seconds * 1e9) / double(N);
        const double msps   = double(N) / (seconds * 1e6);
        std::cout << std::setw(18) << name
                  << "  time=" << seconds << " s"
                  << "  ns/op=" << ns_per
                  << "  Msamples/s=" << msps << '\n';
    };

    std::cout << "\n--- dB -> amplitude performance ---\n";
    report("expf(ln10/20 * x)", t_exp);
    report("powf(10, x/20)",    t_p10);
    report("powf(base, x)",     t_pbas);

    // quick correctness sanity check on a few points
    const float tests[] = { -12.f, -6.f, 0.f, +6.f, +12.f };
    std::cout << "\n--- sample outputs (expf vs pow) ---\n";
    std::cout << std::setprecision(8);
    for (float dB : tests) {
        float a = dB2amp_expf(dB);
        float b = dB2amp_pow10(dB);
        float c = dB2amp_pow_base(dB);
        std::cout << "dB=" << std::setw(6) << dB
                  << "  expf=" << a
                  << "  pow10=" << b
                  << "  pow(base)=" << c << '\n';
    }

    // Touch sink so the compiler keeps computations
    std::cout << "\n(ignore) sink=" << sink << '\n';
    return 0;
}
