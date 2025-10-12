// test_dffs_stability.cpp - Find DFFS stability limit
#include <iostream>
#include <iomanip>
#include "latediff_tank.hpp"

bool test_stability(float DFFS, float RT60, float HB, int samples = 48000) {
    const float SR = 48000.0f;
    const float Sz = 40.0f;
    const float HF = 8000.0f;
    const float LF = 200.0f;
    const float LB = 0.0f;
    
    LateDiffTank tank(SR);
    tank.clear();
    
    // Preroll
    for (int i = 0; i < 12000; ++i) {
        tank.process(0.0f, Sz, DFFS, RT60, HF, HB, LF, LB);
    }
    
    // Test impulse
    float max_out = 0.0f;
    for (int n = 0; n < samples; ++n) {
        float x = (n == 0) ? 1.0f : 0.0f;
        float out = tank.process(x, Sz, DFFS, RT60, HF, HB, LF, LB);
        
        if (!std::isfinite(out) || std::abs(out) > 1000.0f) {
            return false;  // Unstable
        }
        max_out = std::max(max_out, std::abs(out));
    }
    
    // Check if output is growing (sign of instability)
    // Run a bit longer and see if it's still bounded
    for (int n = 0; n < 12000; ++n) {
        float out = tank.process(0.0f, Sz, DFFS, RT60, HF, HB, LF, LB);
        if (!std::isfinite(out) || std::abs(out) > max_out * 2.0f) {
            return false;  // Growing
        }
    }
    
    return true;
}

int main() {
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "====================================\n";
    std::cout << "  DFFS Stability Sweep\n";
    std::cout << "====================================\n\n";
    
    // Test 1: Sweep DFFS with default params
    std::cout << "Test 1: DFFS sweep (RT60=1.5s, HB=-3dB)\n";
    for (float dffs = 0.0f; dffs <= 1.0f; dffs += 0.1f) {
        bool stable = test_stability(dffs, 1.5f, -3.0f);
        std::cout << "  DFFS=" << dffs << ": " 
                  << (stable ? "STABLE  " : "UNSTABLE") << "\n";
    }
    
    // Test 2: Try with no filter damping
    std::cout << "\nTest 2: DFFS sweep (RT60=1.5s, HB=0dB - no damping)\n";
    for (float dffs = 0.0f; dffs <= 1.0f; dffs += 0.1f) {
        bool stable = test_stability(dffs, 1.5f, 0.0f);
        std::cout << "  DFFS=" << dffs << ": " 
                  << (stable ? "STABLE  " : "UNSTABLE") << "\n";
    }
    
    // Test 3: Try with shorter RT60
    std::cout << "\nTest 3: DFFS sweep (RT60=0.5s, HB=-3dB - short decay)\n";
    for (float dffs = 0.0f; dffs <= 1.0f; dffs += 0.1f) {
        bool stable = test_stability(dffs, 0.5f, -3.0f);
        std::cout << "  DFFS=" << dffs << ": " 
                  << (stable ? "STABLE  " : "UNSTABLE") << "\n";
    }
    
    // Test 4: Edge cases
    std::cout << "\nTest 4: Edge cases\n";
    std::cout << "  DFFS=0.5, RT60=2.0s, HB=-6dB: " 
              << (test_stability(0.5f, 2.0f, -6.0f) ? "STABLE" : "UNSTABLE") << "\n";
    std::cout << "  DFFS=0.9, RT60=0.3s, HB=0dB:  " 
              << (test_stability(0.9f, 0.3f, 0.0f) ? "STABLE" : "UNSTABLE") << "\n";
    std::cout << "  DFFS=1.0, RT60=1.0s, HB=-3dB: " 
              << (test_stability(1.0f, 1.0f, -3.0f) ? "STABLE" : "UNSTABLE") << "\n";
    
    std::cout << "\n====================================\n";
    std::cout << "Sweep complete.\n";
    std::cout << "====================================\n";
    
    return 0;
}