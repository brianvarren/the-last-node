#include "src/fast_math.h"
#include <cmath>
#include <iostream>
#include <iomanip>

using namespace FastMath;

int main() {
    std::cout << std::fixed << std::setprecision(6);

    // Test fast_exp2
    std::cout << "Testing fast_exp2:\n";
    for (float x : {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f}) {
        float expected = std::pow(2.0f, x);
        float actual = fast_exp2(x);
        float error = std::abs(expected - actual) / expected;
        std::cout << "  exp2(" << x << "): expected=" << expected
                  << ", actual=" << actual << ", error=" << (error*100) << "%\n";
    }

    // Test fastsin
    std::cout << "\nTesting fastsin:\n";
    for (float x : {0.0f, kPi/4, kPi/2, kPi, 3*kPi/2, kTwoPi}) {
        float expected = std::sin(x);
        float actual = fastsin(x);
        float error = std::abs(expected - actual);
        std::cout << "  sin(" << x << "): expected=" << expected
                  << ", actual=" << actual << ", error=" << error << "\n";
    }

    // Test fastcos
    std::cout << "\nTesting fastcos:\n";
    for (float x : {0.0f, kPi/4, kPi/2, kPi}) {
        float expected = std::cos(x);
        float actual = fastcos(x);
        float error = std::abs(expected - actual);
        std::cout << "  cos(" << x << "): expected=" << expected
                  << ", actual=" << actual << ", error=" << error << "\n";
    }

    // Test fasttanh
    std::cout << "\nTesting fasttanh:\n";
    for (float x : {-3.0f, -1.0f, 0.0f, 1.0f, 3.0f}) {
        float expected = std::tanh(x);
        float actual = fasttanh(x);
        float error = std::abs(expected - actual);
        std::cout << "  tanh(" << x << "): expected=" << expected
                  << ", actual=" << actual << ", error=" << error << "\n";
    }

    return 0;
}
