// test_realistic_params.cpp - Test with realistic musical parameters
#include <cmath>
#include <iostream>
#include <iomanip>
#include <vector>
#include "latediff_tank.hpp"

struct ControlMapping {
    static float calcSz(float size_knob) {
        float size_prime = size_knob * 100.0f;
        float LTSize = 48.0f - (size_prime * 0.72f);
        float LTSym = 2.5f;
        return LTSize + LTSym;
    }
    
    static float calcDffs(float size_knob) {
        float size_prime = size_knob * 100.0f;
        float x = size_prime * 0.01f;
        return std::sqrt(std::sqrt(std::sqrt(x)));
    }
};

struct TestResult {
    bool stable;
    float max_output;
    float energy_1s;  // Total energy in first second
};

TestResult test_config(float size, float RT60, float HB, float LB, 
                       float impulse_level = 1.0f, int test_samples = 96000) {
    const float SR = 48000.0f;
    const float Sz = ControlMapping::calcSz(size);
    const float Dffs = ControlMapping::calcDffs(size);
    const float HF = 8000.0f;
    const float LF = 200.0f;
    
    LateDiffTank tank(SR);
    tank.clear();
    
    // Preroll
    for (int i = 0; i < 12000; ++i) {
        tank.process(0.0f, Sz, Dffs, RT60, HF, HB, LF, LB);
    }
    
    // Test impulse
    TestResult result = {true, 0.0f, 0.0};
    for (int n = 0; n < test_samples; ++n) {
        float x = (n == 0) ? impulse_level : 0.0f;
        float out = tank.process(x, Sz, Dffs, RT60, HF, HB, LF, LB);
        
        if (!std::isfinite(out)) {
            result.stable = false;
            return result;
        }
        
        result.max_output = std::max(result.max_output, std::abs(out));
        
        // Check for runaway (growing over time)
        if (std::abs(out) > impulse_level * 100.0f) {
            result.stable = false;
            return result;
        }
        
        // Accumulate energy for first second
        if (n < 48000) {
            result.energy_1s += out * out;
        }
    }
    
    return result;
}

int main() {
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "================================================\n";
    std::cout << "  Realistic Reverb Parameter Stability Test\n";
    std::cout << "================================================\n\n";
    
    // Test 1: Typical reverb settings
    std::cout << "Test 1: Typical Musical Settings\n";
    std::cout << "---------------------------------\n";
    struct TypicalSetting {
        const char* name;
        float size, RT60, HB, LB;
    };
    
    TypicalSetting typical[] = {
        {"Small Room",      0.2f, 0.5f, -3.0f, 0.0f},
        {"Medium Hall",     0.5f, 1.5f, -3.0f, 0.0f},
        {"Large Hall",      0.8f, 2.5f, -4.0f, 0.0f},
        {"Bright Plate",    0.4f, 1.2f, -1.0f, 1.0f},
        {"Dark Chamber",    0.6f, 2.0f, -6.0f, 2.0f},
        {"Tiny Space",      0.1f, 0.3f, -2.0f, 0.0f},
    };
    
    for (const auto& preset : typical) {
        auto result = test_config(preset.size, preset.RT60, preset.HB, preset.LB);
        std::cout << std::left << std::setw(16) << preset.name << ": ";
        if (result.stable) {
            std::cout << "STABLE  (peak=" << std::setw(7) << result.max_output 
                      << ", energy=" << result.energy_1s << ")\n";
        } else {
            std::cout << "UNSTABLE\n";
        }
    }
    
    // Test 2: Damping sweep (most critical parameter)
    std::cout << "\nTest 2: High Shelf Damping Sweep\n";
    std::cout << "---------------------------------\n";
    std::cout << "HB (dB)  Status    Max Output\n";
    const float size = 0.5f, RT60 = 1.5f;
    for (float HB = -8.0f; HB <= 2.0f; HB += 1.0f) {
        auto result = test_config(size, RT60, HB, 0.0f);
        std::cout << std::setw(7) << HB << "  "
                  << (result.stable ? "STABLE  " : "UNSTABLE")
                  << "  " << result.max_output << "\n";
    }
    
    // Test 3: Size sweep
    std::cout << "\nTest 3: Size Parameter Sweep\n";
    std::cout << "-----------------------------\n";
    std::cout << "size   Sz      Dffs   Status    Max Output\n";
    for (float size = 0.1f; size <= 0.9f; size += 0.2f) {
        float Sz = ControlMapping::calcSz(size);
        float Dffs = ControlMapping::calcDffs(size);
        auto result = test_config(size, 1.5f, -3.0f, 0.0f);
        std::cout << std::setw(4) << size << "   "
                  << std::setw(5) << Sz << "  "
                  << std::setw(5) << Dffs << "  "
                  << (result.stable ? "STABLE  " : "UNSTABLE")
                  << "  " << result.max_output << "\n";
    }
    
    // Test 4: RT60 sweep
    std::cout << "\nTest 4: RT60 Sweep\n";
    std::cout << "-------------------\n";
    std::cout << "RT60(s)  Status    Max Output\n";
    for (float RT60 = 0.3f; RT60 <= 3.0f; RT60 += 0.5f) {
        auto result = test_config(0.5f, RT60, -3.0f, 0.0f);
        std::cout << std::setw(7) << RT60 << "  "
                  << (result.stable ? "STABLE  " : "UNSTABLE")
                  << "  " << result.max_output << "\n";
    }
    
    // Test 5: Extreme (but possible) settings
    std::cout << "\nTest 5: Stress Test (Extreme Settings)\n";
    std::cout << "---------------------------------------\n";
    struct ExtremeSetting {
        const char* name;
        float size, RT60, HB, LB;
    };
    
    ExtremeSetting extreme[] = {
        {"No damping",           0.5f, 1.5f,  0.0f, 0.0f},
        {"High boost",           0.5f, 1.5f, +3.0f, 0.0f},
        {"Very long RT60",       0.5f, 5.0f, -3.0f, 0.0f},
        {"Large + long",         0.9f, 3.0f, -3.0f, 0.0f},
        {"Bright + low damping", 0.5f, 1.5f, -1.0f, 3.0f},
    };
    
    for (const auto& test : extreme) {
        auto result = test_config(test.size, test.RT60, test.HB, test.LB);
        std::cout << std::left << std::setw(24) << test.name << ": "
                  << (result.stable ? "STABLE  " : "UNSTABLE") << "\n";
    }
    
    // Test 6: Input level sensitivity
    std::cout << "\nTest 6: Input Level Sensitivity\n";
    std::cout << "--------------------------------\n";
    std::cout << "Impulse  Status    Max Output\n";
    for (float level : {0.1f, 0.5f, 1.0f, 5.0f, 10.0f}) {
        auto result = test_config(0.5f, 1.5f, -3.0f, 0.0f, level);
        std::cout << std::setw(7) << level << "  "
                  << (result.stable ? "STABLE  " : "UNSTABLE")
                  << "  " << result.max_output << "\n";
    }
    
    std::cout << "\n================================================\n";
    std::cout << "Test complete. Check results above.\n";
    std::cout << "================================================\n";
    
    return 0;
}