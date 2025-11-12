#ifndef MODULATION_H
#define MODULATION_H

#include <cstdint>

constexpr int kModulationSlotCount = 16;
constexpr int kClockModSourceIndex = 13;  // Clock source (was 12 before MIDI Note was added)
constexpr int kClockTargetSequencerBase = 76;  // Was 69, shifted +6 for reverb params
constexpr int kClockTargetSamplerBase = 80;    // Was 73, shifted +6 for reverb params

// Modulation slot for the modulation matrix
struct ModulationSlot {
    int8_t source;       // -1 = empty, 0-21 = source index
    int8_t curve;        // -1 = empty, 0-3 = curve type
    int8_t amount;       // -99 to +99
    int16_t destination; // -1 = empty, extended range for new destinations
    int8_t type;         // -1 = empty, 0 = unidirectional, 1 = bidirectional

    ModulationSlot()
        : source(-1), curve(-1), amount(0), destination(-1), type(-1) {}

    bool isEmpty() const {
        return source == -1 || curve == -1 || destination == -1 || type == -1;
    }

    bool isComplete() const {
        return source >= 0 && curve >= 0 && destination >= 0 && type >= 0;
    }

    void clear() {
        source = -1;
        curve = -1;
        amount = 0;
        destination = -1;
        type = -1;
    }
};

#endif // MODULATION_H
