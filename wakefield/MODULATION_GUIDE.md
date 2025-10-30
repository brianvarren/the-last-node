# Wakefield Modulation Matrix & MIDI Learn Guide

## Quick Summary

This document provides a quick reference for understanding and extending the modulation matrix and MIDI learn systems in Wakefield.

### Key Findings

1. **Chaos as Modulation SOURCES:** Already complete! Chaos X/Y outputs (indices 13-20) are available as modulation sources
2. **Chaos as Modulation DESTINATIONS:** NOT yet implemented - this needs to be added
3. **MIDI CC Learn for Chaos Parameters:** PARTIALLY implemented
   - Parameters 350-353 (Chaos, Clock, Interp, Running) have definitions but 354-355, 410-413 missing CC handlers

---

## Current State

### Modulation Matrix
- **16 slots** for routing
- **83 destinations** (OSC, Filter, Reverb, Sampler, LFO, Mixer, Clock Targets, FM)
- **21 sources** (LFO, ENV, MIDI CC, Clock, Chaos X/Y)

### MIDI Learn Support
- **Supported parameters:** 300+ (OSC, LFO, ENV, Reverb, Filter, Sampler)
- **Missing handlers:** 354 (FAST), 355 (DIFF), 410-413 (Chaos levels)
- **Known issue:** CC loop only checks parameters 0-50, should check all 500

---

## Architecture Overview

### Three-Layer System

```
LAYER 1: Parameter Definition (UI Layer)
├─ ui_parameters.cpp
│  └─ Each parameter has: ID, type, range, page, MIDI learn flag
├─ ui_mod_data.cpp
│  └─ Modulation destinations grouped by module
└─ ui.h
   └─ SynthParameters struct with atomic parameter storage

LAYER 2: MIDI CC Learning & Application (Input Layer)
├─ main.cpp
│  ├─ onControlChange() - CC message handler
│  ├─ applyMIDICCToParameter() - CC to parameter converter
│  └─ mapCCToParameter() - Value mapping (0-127 to parameter range)
└─ ui/ui_parameters.cpp
   └─ Parameter getter/setter methods

LAYER 3: Modulation Matrix & Synth (Audio/Control Layer)
├─ synth.cpp
│  ├─ processModulationMatrix() - Applies all 16 slots
│  └─ getModulationSource() - Gets source values (LFO, ENV, Chaos, etc.)
├─ synth.h
│  └─ ModulationOutputs struct - Intermediate values for modulation
└─ ChaosGenerator class
   └─ Ikeda map implementation with clock-driven iteration
```

### Data Flow

**MIDI CC Input:**
```
MIDI Hardware → onControlChange() → applyMIDICCToParameter() → SynthParameters atomic
                                                              → Synth setters
```

**MIDI Learn:**
```
User presses "Learn" → midiLearnActive = true → Next CC message → parameterCCMap[id] = CC
```

**Modulation Routing:**
```
ModulationSlot[16] → processModulationMatrix() 
    ├─ Fetch source value (LFO/ENV/Chaos/etc.)
    ├─ Apply curve (Linear/Exp/Log/S-Curve)
    ├─ Multiply by amount
    └─ Add to destination parameter → ModulationOutputs
```

---

## Implementation Roadmap

### Phase 1: Add Chaos as Modulation Destinations (REQUIRED)

**Files to modify:** 3
1. `ui_mod_data.cpp` - Add Chaos 1-4 modules to destination list
2. `synth.h` - Add chaos output fields to ModulationOutputs struct
3. `synth.cpp` - Add switch cases to apply chaos modulation

**Result:** 16 new modulation destinations (Chaos Parameter, Clock, Interp, FAST for each of 4 generators)

### Phase 2: Fix MIDI CC Handlers for Chaos (REQUIRED)

**Files to modify:** 1
1. `main.cpp` - Add missing case statements (354, 355, 410-413)

**Result:** Full MIDI CC support for FAST, DIFF, and Chaos mixer levels

### Phase 3: Fix MIDI CC Parameter Loop (IMPORTANT)

**Files to modify:** 1
1. `main.cpp` - Change loop range from 50 to 500

**Result:** New parameters can actually respond to CC (currently they don't!)

---

## File Reference Quick Index

| Task | File | Lines | Action |
|------|------|-------|--------|
| Add chaos destinations | `ui_mod_data.cpp` | After 109 | Insert 4 modules |
| Add modulation outputs | `synth.h` | After 147 | Insert 16 fields |
| Route chaos modulation | `synth.cpp` | After 1172 | Insert 16 cases |
| Fix CC loop range | `main.cpp` | 557 | Change 50→500 |
| Add FAST CC handler | `main.cpp` | 466+ | Insert case 354 |
| Add DIFF CC handler | `main.cpp` | 466+ | Insert case 355 |
| Add Chaos level CC | `main.cpp` | 466+ | Insert cases 410-413 |

---

## Design Patterns

### Adding a Modulation Destination

**Pattern:** 1 UI module = 4-7 destinations (one per parameter)

Example: Oscillator 1
```
UI: {"Oscillator 1", {Pitch, Morph, Duty, Ratio, Offset, Amp}}
Destinations: 0-5
Outputs struct: osc1Pitch, osc1Morph, osc1Duty, osc1Ratio, osc1Offset, osc1Amp
Synth apply: Case 0-5 in switch statement
```

For Chaos, we want:
```
UI: {"Chaos 1", {Parameter, Clock, Interp, FAST}}
    {"Chaos 2", {Parameter, Clock, Interp, FAST}}
    {"Chaos 3", {Parameter, Clock, Interp, FAST}}
    {"Chaos 4", {Parameter, Clock, Interp, FAST}}
Destinations: 83-98 (16 total)
```

### Adding MIDI CC Support

**Pattern:** Parameter ID → CC Number → mapCCToParameter() → applyMIDICCToParameter()

Example: Filter Cutoff (ID 32)
```
onControlChange(ccNumber, ccValue) {
    if (parameterCCMap[32] == ccNumber) {
        applyMIDICCToParameter(32, ccValue);
    }
}

applyMIDICCToParameter(32, ccValue) {
    float mapped = mapCCToParameter(ccValue, 20.0f, 20000.0f, logarithmic);
    synthParams->filterCutoff = mapped;
}
```

---

## Code Snippet: Full MIDI Learn Flow

```cpp
// 1. USER INITIATES LEARN
ui->startMidiLearn(parameterId);  // Sets midiLearnActive=true, midiLearnParameterId=id

// 2. MIDI CC MESSAGE ARRIVES
void onControlChange(int cc, int value) {
    if (synthParams->midiLearnActive) {
        int paramId = synthParams->midiLearnParameterId;
        synthParams->parameterCCMap[paramId] = cc;  // Store mapping
        synthParams->midiLearnActive = false;
        ui->showMessage("Learned CC#" + cc + " for " + paramName);
    }
}

// 3. SUBSEQUENT CC MESSAGES APPLY
void onControlChange(int cc, int value) {
    // Check if any parameter is mapped to this CC
    for (int paramId = 0; paramId < 500; ++paramId) {
        if (synthParams->parameterCCMap[paramId] == cc) {
            applyMIDICCToParameter(paramId, value);
            break;
        }
    }
}

// 4. VALUE IS APPLIED
void applyMIDICCToParameter(int paramId, int ccValue) {
    switch (paramId) {
        case 32:  // Filter Cutoff
            float freq = mapCCToParameter(ccValue, 20.0f, 20000.0f, true);
            synthParams->filterCutoff = freq;
            break;
    }
}
```

---

## Parameter ID System

### Groups
- **1-42:** Main parameters (OSC, Reverb, Filter, etc.)
- **50-68:** Mixer and Sampler parameters
- **200-206:** LFO parameters
- **300-323:** Envelope parameters (4 envelopes × 6 params)
- **350-355:** Chaos parameters
- **360:** FM Global Depth
- **400:** CPU Monitor
- **410-413:** Chaos mixer levels

### MIDI Learn Support Matrix
```
Parameter Range  | Supports Learn | CC Handler Present | Notes
1-42            | YES            | YES                | Basic parameters
50-68           | MIXED          | PARTIAL            | Mixer levels: IMMUNE
200-206         | YES            | YES                | LFO (applies to LFO 0)
300-323         | YES            | YES                | Envelope (applies to Env 0)
350-353         | YES            | PARTIAL            | Chaos (no handlers!)
354-355         | YES            | NO                 | FAST/DIFF missing handlers
360             | YES            | YES                | FM
410-413         | NO (IMMUNE)    | NO                 | Chaos levels missing
```

---

## Parameter Mapping Details

### Mapping Strategy by Type

**Linear Parameters (0.0-1.0):**
```cpp
float value = mapCCToParameter(ccValue, 0.0f, 1.0f, false);
// CC 0 = 0.0, CC 127 = 1.0
```

**Logarithmic Parameters (frequency-like):**
```cpp
float freq = mapCCToParameter(ccValue, 20.0f, 20000.0f, true);
// CC 0 = 20 Hz (log), CC 127 = 20kHz (log)
```

**Enum/Discrete Parameters:**
```cpp
int mode = static_cast<int>(mapCCToParameter(ccValue, 0, 3, false));
// CC 0-42 = mode 0, CC 43-85 = mode 1, CC 86-127 = mode 2+
```

**Boolean Parameters:**
```cpp
bool enabled = (ccValue > 63);  // Standard MIDI convention
// CC 0-63 = false, CC 64-127 = true
```

---

## Chaos Generator Integration

### Chaos as Modulation Source (WORKING)
- 4 generators × 2 outputs (X, Y) = 8 modulation sources (indices 13-20)
- Available in modulation matrix now
- ChaosGenerator class in `/src/chaos.h`

### Chaos as Modulation Destination (NEEDS WORK)
- Not yet exposed as modulation destinations
- Needed: Parameter, Clock, Interp, FAST for each chaos generator
- When modulated: Enables dynamic chaos parameter variation (e.g., increase chaos with velocity)

### Chaos Parameter Mappings
```
UI Parameter ID | Name        | Range            | Type       | MIDI Learn?
350             | Chaos       | 0.0-1.0 linear  | FLOAT      | YES (handler missing)
351             | Clock       | 0.00001-20000Hz | FLOAT LOG  | YES (handler missing)
352             | Interp      | 0-2 (ENUM)      | ENUM       | YES (handler missing)
353             | Running     | BOOL            | BOOL       | NO (IMMUNE)
354             | FAST        | BOOL            | BOOL       | YES (handler missing)
355             | DIFF        | BOOL            | BOOL       | YES (handler missing)
410-413         | Level       | 0.0-1.0         | FLOAT      | NO (IMMUNE)
```

---

## Known Issues & Limitations

### Current Issues
1. **CC Parameter Loop Limit:** Line 557 in main.cpp only checks parameters 0-50
   - Effect: Parameters 54+ won't respond to MIDI CC even if mapped
   - Fix: Change loop to check all 500+ parameters

2. **Missing CC Handlers:** 354, 355, 410-413 lack switch cases
   - Effect: FAST, DIFF, Chaos levels can't be controlled via MIDI CC
   - Fix: Add 6 case statements in applyMIDICCToParameter()

3. **Chaos Not Modulatable:** No modulation destinations for chaos parameters
   - Effect: Can't route LFO/ENV/etc to chaos controls
   - Fix: Add 16 destinations for 4 chaos generators

### Design Limitations
1. All oscillators share same parameters (can't MIDI learn OSC 2, 3, 4 separately)
   - Current: Each parameter maps to single UI selector or defaults to OSC 1
   - Would need indexed parameter IDs to fix (e.g., 11a, 11b, 11c, 11d for OSC freqs)

2. All LFOs share same parameters for MIDI CC
   - Current: CC applies to LFO 0 only
   - Same solution as above

3. DIFF mode is global, not per-generator
   - Could be changed if desired

---

## Testing the Implementation

### After adding chaos modulation destinations:

```bash
# Test 1: Chaos as destination
1. Go to MOD page
2. Create modulation slot
3. Source: LFO 1
4. Destination: should see "Chaos 1", "Chaos 2", etc.
5. Select "Chaos 1" → "Parameter"
6. Verify modulation is applied

# Test 2: Chaos parameter MIDI learn
1. Go to CHAOS page
2. Press LEARN on FAST parameter
3. Move MIDI fader
4. Verify CC is learned
5. Move fader again
6. Verify FAST mode toggles
```

---

## Future Enhancements

1. **Per-OSC Parameter Mapping:** Allow independent MIDI CC for OSC 2-4, SAMP 2-4
2. **Chaos Running as Modulatable:** Currently immune, could expose with proper handling
3. **DIFF Mode Per-Generator:** Make chaosDiff into chaosPerGenDiff[4]
4. **Chaos Outputs as Visible Destinations:** Allow routing chaos X/Y outputs to parameters
5. **Multi-Chaos Sequencing:** Chaos generators could be sequencer-synced like samplers
6. **Automation Recording:** Capture MIDI/Modulation changes for preset morphing

---

## References

- **Modulation Matrix UI:** `/src/ui/pages/ui_page_mod.cpp`
- **Chaos Implementation:** `/src/chaos.h`
- **Chaos UI Page:** `/src/ui/pages/ui_page_chaos.cpp`
- **Parameter List:** `/src/ui/ui_parameters.cpp` (initializeParameters)
- **MIDI Handler:** `/src/midi.cpp` + callbacks in `/src/main.cpp`

---

**Document Last Updated:** Oct 29, 2025
**Status:** Complete Analysis Ready for Implementation
