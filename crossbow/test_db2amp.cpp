#include <iostream>
#include <iomanip>
#include <cmath>

// Convert dB to linear amplitude
inline float dB2amp_expf(float dB) noexcept {
    return expf(0.11512925464970229f * dB); // ln(10)/20 * dB
}

// Reference: pow(10, dB/20)
inline float dB2amp_pow(float dB) noexcept {
    return powf(10.0f, dB / 20.0f);
}

int main() {
    std::cout << std::fixed << std::setprecision(8);

    const float testVals[] = { -12.0f, -6.0f, 0.0f, +6.0f, +12.0f };

    for (float dB : testVals) {
        float ref = dB2amp_pow(dB);      // expected output
        float fast = dB2amp_expf(dB);    // expf() version
        float reactor = powf(1.12201845456459045f, dB); // literal Reaktor macro form

        std::cout << "dB=" << std::setw(6) << dB
                  << " | expf=" << fast
                  << " | pow(10,dB/20)=" << ref
                  << " | pow(1.122...)= " << reactor
                  << '\n';
    }
}
