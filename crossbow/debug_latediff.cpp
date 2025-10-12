// debug_latediff.cpp - Find where it blows up
#include <iostream>
#include <iomanip>
#include "latediff_tank.hpp"

int main() {
    const float SR = 48000.0f;
    const float Sz = 40.0f;
    
    std::cout << std::fixed << std::setprecision(6);
    
    // Test 1: No diffusion, check main feedback loop only
    std::cout << "=== Test 1: DFFS=0 (no diffuser feedback) ===\n";
    {
        LateDiffTank tank(SR);
        tank.clear();
        
        // Preroll
        for (int i = 0; i < 12000; ++i) {
            tank.process(0.0f, Sz, 0.0f, 1.5f, 8000.0f, -3.0f, 200.0f, 0.0f);
        }
        
        bool stable = true;
        float max_out = 0.0f;
        for (int n = 0; n < 1000; ++n) {
            float x = (n == 0) ? 1.0f : 0.0f;
            float out = tank.process(x, Sz, 0.0f, 1.5f, 8000.0f, -3.0f, 200.0f, 0.0f);
            max_out = std::max(max_out, std::abs(out));
            if (!std::isfinite(out) || std::abs(out) > 100.0f) {
                std::cout << "  Blew up at sample " << n << ": " << out << "\n";
                stable = false;
                break;
            }
        }
        if (stable) {
            std::cout << "  STABLE - max output: " << max_out << "\n";
        }
    }
    
    // Test 2: Check AF computation
    std::cout << "\n=== Test 2: AF computation ===\n";
    {
        auto p2f = [](float p)->float { return 8.71742f * expf(0.05776226504666211f * p); };
        auto p2t = [&](float p)->float { float f = p2f(p); return (f > 1e-12f) ? 1.0f / f : 1e12f; };
        
        float T4 = p2t(Sz + 0.0f);
        float RT60 = 1.5f;
        float AF = rt60_to_AF(T4, RT60);
        
        std::cout << "  T4: " << T4 << " s\n";
        std::cout << "  RT60: " << RT60 << " s\n";
        std::cout << "  AF: " << AF << "\n";
        std::cout << "  AF should be < 1.0: " << (AF < 1.0f ? "PASS" : "FAIL") << "\n";
        
        if (AF >= 1.0f) {
            std::cout << "  ERROR: AF >= 1.0 will cause feedback to grow!\n";
        }
    }
    
    // Test 3: Check filter unity gain
    std::cout << "\n=== Test 3: Filter unity gain (0 dB) ===\n";
    {
        BiLin1P filt;
        filt.reset();
        filt.setCoeffs(setBL_HS(8000.0f, 0.0f, SR));
        
        float sum_in = 0.0f, sum_out = 0.0f;
        for (int i = 0; i < 1000; ++i) {
            float x = (i == 0) ? 1.0f : 0.0f;
            float y = filt.process(x);
            sum_in += x * x;
            sum_out += y * y;
        }
        float gain = std::sqrt(sum_out / sum_in);
        std::cout << "  HS 0dB energy gain: " << gain << "\n";
        std::cout << "  Should be ~1.0: " << (std::abs(gain - 1.0f) < 0.1f ? "PASS" : "FAIL") << "\n";
    }
    
    // Test 4: Minimal case - single impulse, low diffusion
    std::cout << "\n=== Test 4: Minimal test ===\n";
    {
        LateDiffTank tank(SR);
        tank.clear();
        
        // Very short RT60, low diffusion
        for (int i = 0; i < 12000; ++i) {
            tank.process(0.0f, Sz, 0.1f, 0.3f, 8000.0f, 0.0f, 200.0f, 0.0f);
        }
        
        bool stable = true;
        for (int n = 0; n < 100; ++n) {
            float x = (n == 0) ? 0.1f : 0.0f;  // Small impulse
            float out = tank.process(x, Sz, 0.1f, 0.3f, 8000.0f, 0.0f, 200.0f, 0.0f);
            if (!std::isfinite(out)) {
                std::cout << "  Blew up at sample " << n << ": " << out << "\n";
                stable = false;
                break;
            }
            if (n < 10) {
                std::cout << "  [" << n << "] " << out << "\n";
            }
        }
        std::cout << "  Result: " << (stable ? "STABLE" : "UNSTABLE") << "\n";
    }
    
    return 0;
}