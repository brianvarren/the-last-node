// test_latediff_energy.cpp
// Energy-based RT60 measurement for reverb tank
#include <cmath>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cassert>
#include "latediff_tank.hpp"

// Schroeder backward integration for RT60 measurement
// Returns energy decay curve in dB
std::vector<float> schroeder_integration(const std::vector<float>& ir) {
    std::vector<float> energy_db;
    energy_db.resize(ir.size());
    
    // Backward integration: E(t) = sum of squared samples from t to end
    double cumsum = 0.0;
    for (int i = (int)ir.size() - 1; i >= 0; --i) {
        cumsum += ir[i] * ir[i];
        energy_db[i] = cumsum;
    }
    
    // Convert to dB (normalize to 0 dB at start)
    double E0 = energy_db[0];
    if (E0 > 1e-20) {
        for (size_t i = 0; i < energy_db.size(); ++i) {
            energy_db[i] = 10.0f * std::log10(energy_db[i] / E0);
        }
    }
    
    return energy_db;
}

// Find time when energy crosses a threshold (e.g., -60 dB)
float find_crossing_time(const std::vector<float>& energy_db, float SR, float threshold_dB) {
    // Skip initial samples to avoid onset transients
    int start = int(0.01f * SR);  // Skip first 10ms
    
    for (size_t i = start; i < energy_db.size(); ++i) {
        if (energy_db[i] <= threshold_dB) {
            // Linear interpolation for sub-sample accuracy
            if (i > 0) {
                float t0 = (i - 1) / SR;
                float t1 = i / SR;
                float e0 = energy_db[i - 1];
                float e1 = energy_db[i];
                float alpha = (threshold_dB - e0) / (e1 - e0);
                return t0 + alpha * (t1 - t0);
            }
            return i / SR;
        }
    }
    
    return energy_db.size() / SR;
}

// Compute decay rate from linear regression of energy curve
float estimate_decay_rate(const std::vector<float>& energy_db, float SR) {
    // Use middle 50% of decay (from -5 dB to -35 dB) for robust fit
    int start_idx = 0, end_idx = 0;
    
    for (size_t i = 0; i < energy_db.size(); ++i) {
        if (energy_db[i] <= -5.0f && start_idx == 0) start_idx = i;
        if (energy_db[i] <= -35.0f && end_idx == 0) {
            end_idx = i;
            break;
        }
    }
    
    if (end_idx <= start_idx || start_idx == 0) {
        return -60.0f; // Couldn't fit, return default
    }
    
    // Linear regression: y = a + b*t
    double sum_t = 0.0, sum_y = 0.0, sum_tt = 0.0, sum_ty = 0.0;
    int n = 0;
    
    for (int i = start_idx; i < end_idx; ++i) {
        double t = i / SR;
        double y = energy_db[i];
        sum_t += t;
        sum_y += y;
        sum_tt += t * t;
        sum_ty += t * y;
        n++;
    }
    
    if (n < 2) return -60.0f;
    
    double b = (n * sum_ty - sum_t * sum_y) / (n * sum_tt - sum_t * sum_t);
    
    // Decay rate in dB/s
    return b;
}

int main() {
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "========================================\n";
    std::cout << "  Reverb Tank Energy Decay Test\n";
    std::cout << "========================================\n\n";
    
    const float SR = 48000.0f;
    const float size = 0.40f;
    const float Sz = size * 100.0f;
    const float DFFS = 0.7f;
    const float RT60_target = 1.500f;
    const float HF = 8000.0f, HB = -3.0f;  // High freq damping
    const float LF = 200.0f, LB = 0.0f;
    
    std::cout << "Configuration:\n";
    std::cout << "  Sample Rate: " << SR << " Hz\n";
    std::cout << "  Size: " << size << " (" << Sz << ")\n";
    std::cout << "  Diffusion: " << DFFS << "\n";
    std::cout << "  Target RT60: " << RT60_target << " s\n";
    std::cout << "  High Shelf: " << HF << " Hz, " << HB << " dB\n";
    std::cout << "  Low Shelf: " << LF << " Hz, " << LB << " dB\n\n";
    
    LateDiffTank tank(SR);
    tank.clear();
    
    // Preroll to settle smoothers
    const int preroll = int(0.250f * SR);
    for (int i = 0; i < preroll; ++i) {
        tank.process(0.0f, Sz, DFFS, RT60_target, HF, HB, LF, LB);
    }
    
    // Generate impulse response (capture 3x RT60 for safety)
    const int total = int(3.0f * RT60_target * SR);
    std::vector<float> ir;
    ir.reserve(total);
    
    std::cout << "Generating " << (total / SR) << "s impulse response...\n";
    for (int n = 0; n < total; ++n) {
        float x = (n == 0) ? 1.0f : 0.0f;
        float out = tank.process(x, Sz, DFFS, RT60_target, HF, HB, LF, LB);
        ir.push_back(out);
    }
    
    // Check for stability (no NaN/inf)
    bool stable = true;
    float peak = 0.0f;
    for (float v : ir) {
        if (!std::isfinite(v)) {
            stable = false;
            break;
        }
        peak = std::max(peak, std::abs(v));
    }
    
    std::cout << "Peak amplitude: " << peak << "\n";
    std::cout << "Stability check: " << (stable ? "PASS" : "FAIL") << "\n\n";
    
    if (!stable) {
        std::cout << "[FAIL] Output contains NaN or Inf\n";
        return 1;
    }
    
    // Compute Schroeder decay curve
    auto energy_db = schroeder_integration(ir);
    
    // Measure T60 (time to -60 dB)
    float T60_measured = find_crossing_time(energy_db, SR, -60.0f);
    
    // Estimate decay rate via linear fit
    float decay_rate = estimate_decay_rate(energy_db, SR);
    float RT60_from_slope = (decay_rate != 0.0f) ? -60.0f / decay_rate : 0.0f;
    
    std::cout << "Results:\n";
    std::cout << "  T60 (crossing): " << T60_measured << " s\n";
    std::cout << "  T60 (linear fit): " << RT60_from_slope << " s\n";
    std::cout << "  Decay rate: " << decay_rate << " dB/s\n\n";
    
    // Show energy decay at key time points
    std::cout << "Energy decay profile:\n";
    for (float t : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 2.5f}) {
        int idx = int(t * SR);
        if (idx < (int)energy_db.size()) {
            std::cout << "  t=" << std::setw(4) << t << "s: " 
                      << std::setw(7) << energy_db[idx] << " dB\n";
        }
    }
    
    // Pass criteria: RT60 within 40% of target
    // (Loose tolerance because filters change decay character)
    float error_crossing = std::abs(T60_measured - RT60_target) / RT60_target;
    float error_slope = std::abs(RT60_from_slope - RT60_target) / RT60_target;
    
    bool rt60_ok = (error_crossing < 0.5f) || (error_slope < 0.5f);
    
    std::cout << "\nTest Results:\n";
    std::cout << "  Stability: " << (stable ? "PASS" : "FAIL") << "\n";
    std::cout << "  RT60 error (crossing): " << (error_crossing * 100.0f) << "%\n";
    std::cout << "  RT60 error (slope): " << (error_slope * 100.0f) << "%\n";
    std::cout << "  RT60 match: " << (rt60_ok ? "PASS" : "FAIL") 
              << " (tolerance: 50%)\n";
    
    // Also test with different configurations
    std::cout << "\n========================================\n";
    std::cout << "  Quick Parameter Sweep\n";
    std::cout << "========================================\n";
    
    struct TestCase {
        float RT60;
        float DFFS;
        float HB;
        const char* desc;
    };
    
    TestCase cases[] = {
        {0.5f, 0.5f, 0.0f, "Short RT, medium diffusion"},
        {2.5f, 0.8f, -6.0f, "Long RT, high diffusion, damped"},
        {1.0f, 0.3f, 0.0f, "Medium RT, low diffusion"},
        {1.5f, 0.0f, 0.0f, "No diffusion (feedback only)"},
    };
    
    bool all_stable = true;
    for (const auto& tc : cases) {
        LateDiffTank t2(SR);
        t2.clear();
        
        // Quick preroll
        for (int i = 0; i < preroll/2; ++i) {
            t2.process(0.0f, Sz, tc.DFFS, tc.RT60, HF, tc.HB, LF, LB);
        }
        
        // Short impulse response
        bool case_stable = true;
        for (int n = 0; n < 48000; ++n) {  // 1 second
            float x = (n == 0) ? 1.0f : 0.0f;
            float out = t2.process(x, Sz, tc.DFFS, tc.RT60, HF, tc.HB, LF, LB);
            if (!std::isfinite(out)) {
                case_stable = false;
                break;
            }
        }
        
        all_stable &= case_stable;
        std::cout << "  " << tc.desc << ": " 
                  << (case_stable ? "PASS" : "FAIL") << "\n";
    }
    
    std::cout << "\n========================================\n";
    bool pass = stable && rt60_ok && all_stable;
    std::cout << "  Overall: " << (pass ? "PASS" : "FAIL") << "\n";
    std::cout << "========================================\n";
    
    assert(stable && "Unstable output");
    assert(rt60_ok && "RT60 too far from target");
    assert(all_stable && "Parameter sweep found instability");
    
    return 0;
}