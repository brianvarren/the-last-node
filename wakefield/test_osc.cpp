#include "brainwave_osc.h"
#include <iostream>

int main() {
    BrainwaveOscillator osc;

    std::cout << "Testing oscillator with defaults:\n";
    std::cout << "  Shape: " << static_cast<int>(osc.getShape()) << "\n";
    std::cout << "  Morph: " << osc.getMorph() << "\n";
    std::cout << "  Note freq: " << osc.getNoteFrequency() << " Hz\n\n";

    std::cout << "Generating 10 samples at 48000 Hz:\n";
    for (int i = 0; i < 10; i++) {
        float sample = osc.process(48000.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        std::cout << "  Sample " << i << ": " << sample << "\n";
    }

    return 0;
}
