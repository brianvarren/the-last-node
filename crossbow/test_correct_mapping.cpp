// test_correct_mapping.cpp - Use actual control mapping
#include <cmath>
#include <iostream>
#include <iomanip>
#include "latediff_tank.hpp"

// Actual control mapping from knobs
struct ControlMapping {
    static float calcSz(float size_knob) {
        float size_prime = size_knob * 100.0f;
        float LTSize = 48.0f - (size_prime * 0.72f);
        float LTSym = 2.5f;
        return LTSize + LTSym;
    }
    
    static float calcDffs(float size_knob) {
        float size_prime = size_knob * 100.0f;
        float x = size_prime * 0.01f;  // Back to [0,1]
        // 8th root: sqrt(sqrt(sqrt(x)))
        return std::sqrt(std::sqrt(std::sqrt(x)));
    }
};

int main() {
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "==========================================\n";
    std::cout << "  Control Mapping Verification\n";
    std::cout << "==========================================\n\n";
    
    // Show mapping for various size values
    std::cout << "size   Sz      LTDffs\n";
    std::cout << "----   -----   ------\n";
    for (float size = 0.0f; size <= 1.0f; size += 0.1f) {
        float Sz = ControlMapping::calcSz(size);
        float Dffs = ControlMapping::calcDffs(size);
        std::cout << std::setw(4) << size << "   "
                  << std::setw(5) << Sz << "   "
                  << std::setw(6) << Dffs << "\n";
    }
    
    std::cout << "\n==========================================\n";
    std::cout << "  Stability Test with Correct Mapping\n";
    std::cout << "==========================================\n\n";
    
    const float SR = 48000.0f;
    const float RT60 = 1.5f;
    const float HF = 8000.0f, HB = -3.0f;
    const float LF = 200.0f, LB = 0.0f;
    
    // Test various size knob positions
    float test_sizes[] = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f};
    
    for (float size : test_sizes) {
        float Sz = ControlMapping::calcSz(size);
        float Dffs = ControlMapping::calcDffs(size);
        
        std::cout << "Testing size=" << size 
                  << " (Sz=" << Sz << ", Dffs=" << Dffs << ")\n";
        
        LateDiffTank tank(SR);
        tank.clear();
        
        // Preroll
        for (int i = 0; i < 12000; ++i) {
            tank.process(0.0f, Sz, Dffs, RT60, HF, HB, LF, LB);
        }
        
        // Test impulse
        bool stable = true;
        float max_out = 0.0f;
        for (int n = 0; n < 48000; ++n) {  // 1 second
            float x = (n == 0) ? 1.0f : 0.0f;
            float out = tank.process(x, Sz, Dffs, RT60, HF, HB, LF, LB);
            
            if (!std::isfinite(out) || std::abs(out) > 1000.0f) {
                std::cout << "  UNSTABLE at sample " << n 
                          << " (out=" << out << ")\n";
                stable = false;
                break;
            }
            max_out = std::max(max_out, std::abs(out));
        }
        
        if (stable) {
            std::cout << "  STABLE - max output: " << max_out << "\n";
        }
        std::cout << "\n";
    }
    
    std::cout << "==========================================\n";
    std::cout << "Test complete.\n";
    std::cout << "==========================================\n";
    
    return 0;
}