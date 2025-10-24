#ifndef MODULATION_H
#define MODULATION_H

#include <cstdint>

constexpr int kModulationSlotCount = 16;
constexpr int kClockModSourceIndex = 12;
constexpr int kClockTargetSequencerBase = 69;
constexpr int kClockTargetSamplerBase = 73;

// Modulation slot for the modulation matrix
struct ModulationSlot {
    int8_t source;       // -1 = empty, 0-12 = source index
    int8_t curve;        // -1 = empty, 0-3 = curve type
    int8_t amount;       // -99 to +99
    int8_t destination;  // -1 = empty, 0-76 = destination index
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
