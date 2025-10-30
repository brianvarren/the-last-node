# Wakefield Modulation Matrix and MIDI Learn Implementation Analysis

## Overview

This document provides a complete analysis of how the modulation matrix system and MIDI learn implementation work in Wakefield, with specific focus on adding chaos generators as modulation destinations and identifying missing MIDI learn support.

---

## 1. MODULATION MATRIX STRUCTURE

### 1.1 Core Data Structure
**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/modulation.h` (Lines 1-40)

```cpp
constexpr int kModulationSlotCount = 16;
constexpr int kClockModSourceIndex = 12;
constexpr int kClockTargetSequencerBase = 69;
constexpr int kClockTargetSamplerBase = 73;

struct ModulationSlot {
    int8_t source;       // -1 = empty, 0-12 = source index
    int8_t curve;        // -1 = empty, 0-3 = curve type
    int8_t amount;       // -99 to +99
    int8_t destination;  // -1 = empty, 0-76 = destination index
    int8_t type;         // -1 = empty, 0 = unidirectional, 1 = bidirectional
};
```

Key Facts:
- **16 modulation slots** available in the matrix
- Each slot contains source, curve, amount, destination, and type
- Destinations are indexed 0-82 (83 total destinations currently)
- Curve types: 0=Linear, 1=Exponential, 2=Logarithmic, 3=S-Curve

### 1.2 Modulation Sources
**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/ui/ui_mod_data.cpp` (Lines 12-24)

Current sources (13 total):
```
0-3:   LFO 1-4
4-7:   ENV 1-4  
8:     Velocity
9:     Aftertouch
10:    Mod Wheel
11:    Pitch Bend
12:    Clock
13-20: Chaos 1-4 X,Y outputs (8 sources)
```

**ALREADY COMPLETE** - Chaos X/Y outputs are already modulation sources!

---

## 2. MODULATION DESTINATIONS STRUCTURE

### 2.1 Destination Definition System
**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/ui/ui_mod_data.cpp` (Lines 40-192)

Destinations are organized by module with parameters:

```cpp
{"Oscillator 1-4", {Pitch, Morph, Duty, Ratio, Offset, Amp}}  // 24 destinations (0-23)
{"Filter", {Cutoff, Resonance, Drive, Width, Notch FB, Spread, Dry/Wet}}  // 7 destinations (24-30)
{"Reverb", {Mix, Size}}  // 2 destinations (31-32)
{"Sampler 1-4", {Pitch, Loop Start, Loop Length, Crossfade, Level}}  // 20 destinations (33-52)
{"LFO 1-4", {Rate, Morph, Duty}}  // 12 destinations (53-64)
{"Mixer", {Master, OSC 1-4, SAMP 1-4}}  // 9 destinations (65-73)
{"Clock Targets", {Seq Track 1-4, Sampler 1-4}}  // 8 destinations (74-81)
{"FM", {Global Depth}}  // 1 destination (82)
```

**TOTAL CURRENT:** 83 destinations (0-82)

### 2.2 Destination Application in Synth
**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/synth.cpp` (Lines 1071-1173)

The `processModulationMatrix()` method applies modulation using a switch statement:
- Cases 0-5: OSC 1 parameters
- Cases 6-11: OSC 2 parameters
- Cases 12-17: OSC 3 parameters
- Cases 18-23: OSC 4 parameters
- Cases 24-30: Filter parameters
- Cases 31-32: Reverb parameters
- Cases 33-37: Sampler 1 parameters
- Cases 38-42: Sampler 2 parameters
- Cases 43-47: Sampler 3 parameters
- Cases 48-52: Sampler 4 parameters
- Cases 53-55: LFO 1 parameters
- Cases 56-58: LFO 2 parameters
- Cases 59-61: LFO 3 parameters
- Cases 62-64: LFO 4 parameters
- Cases 65-73: Mixer parameters
- Cases 74-81: Clock targets (sequencer and sampler phase)
- Case 82: FM Global Depth

---

## 3. CHAOS PARAMETERS ANALYSIS

### 3.1 Chaos Generator Hardware Parameters
**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/ui.h` (Lines 265-299)

Per-generator parameters (x4):
```cpp
std::atomic<float> chaos1Parameter{0.918f};    // "Chaos" - u parameter (0.0-1.0)
std::atomic<float> chaos1ClockFreq{1.0f};      // "Clock" (Hz, 0.00001-20000)
std::atomic<int> chaos1InterpMode{0};          // "Interp" (0=LINEAR, 1=CUBIC, 2=HOLD)
std::atomic<bool> chaos1FastMode{false};       // "FAST" toggle (boolean)
std::atomic<bool> chaos1Running{true};         // "Running" toggle (boolean)
std::atomic<float> chaos1VisualX{0.0f};        // Visualization only
std::atomic<float> chaos1VisualY{0.0f};        // Visualization only
```

Global parameter:
```cpp
std::atomic<bool> chaosDiff{false};            // "DIFF" mode (boolean)
```

Mixer levels:
```cpp
std::atomic<float> chaosLevel[4]{0.8f, 0.8f, 0.8f, 0.8f};  // Mixer levels for chaos outputs
```

### 3.2 Chaos Parameter Definitions in UI
**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/ui/ui_parameters.cpp` (Lines 168-173)

```cpp
// CHAOS PAGE - RANDOMIZABLE (except running state)
case 350: "Chaos" (0.0-1.0, randomizable)           // Parameter (u value)
case 351: "Clock" (0.00001-20000 Hz, randomizable) // Clock frequency
case 352: "Interp" (ENUM 0-2, randomizable)        // Interpolation mode
case 353: "Running" (BOOL, IMMUNE)                 // Running state (not randomizable)
case 354: "FAST" (BOOL, randomizable)              // Fast mode toggle
case 355: "DIFF" (BOOL, randomizable)              // Diff mode toggle
```

Mixer levels:
```cpp
case 410-413: "CHAOS 1-4 Level" (0.0-1.0, IMMUNE) // Mixer levels
```

---

## 4. MISSING MODULATION DESTINATIONS FOR CHAOS

**Current State:** Chaos parameters are NOT modulation destinations.

### 4.1 Needed Modulation Destinations

Per chaos generator (x4), we need 6 destinations each = **24 new destinations**:

```
Destination Indices 83-106 (or 83-82+24):

CHAOS 1 (83-88):
- 83: Chaos Parameter (0.0-1.0)
- 84: Clock Frequency (logarithmic, 0.00001-20000 Hz)
- 85: Interp Mode (ENUM 0-2)
- 86: Running (BOOL, should be immutable - skip?)
- 87: FAST Mode (BOOL)
- 88: DIFF Mode (global, should skip per-generator?)

CHAOS 2 (89-94):
- 89-94: Same as CHAOS 1

CHAOS 3 (95-100):
- 95-100: Same as CHAOS 1

CHAOS 4 (101-106):
- 101-106: Same as CHAOS 1
```

**Design Decision:** Should DIFF mode be global or per-generator? Currently it's global (`chaosDiff`).

### 4.2 Implementation Pattern

Looking at oscillators (6 destinations per OSC) in `ui_mod_data.cpp`:
```cpp
{"Oscillator 1", {
    {"Pitch", "O1:Pitch"},
    {"Morph", "O1:Morph"},
    {"Duty", "O1:Duty"},
    {"Ratio", "O1:Ratio"},
    {"Offset", "O1:Offset"},
    {"Amp", "O1:Amp"}
}}
```

---

## 5. MIDI LEARN IMPLEMENTATION

### 5.1 Parameter CC Mapping System
**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/ui.h` (Lines 92-100)

```cpp
// Generic MIDI CC Learn for new parameter system
std::atomic<bool> midiLearnActive{false};
std::atomic<int> midiLearnParameterId{-1};
std::atomic<double> midiLearnStartTime{0.0};

// MIDI CC mappings for all parameters (parameter ID -> CC number, -1 means not mapped)
static constexpr int kMaxParamMap = 1024;
std::atomic<int> parameterCCMap[kMaxParamMap];  // Broad range for parameter IDs
```

### 5.2 MIDI Learn Learning Flow
**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/main.cpp` (Lines 500-521)

```cpp
void onControlChange(int controller, int value) {
    if (!synthParams) return;
    
    // Check if we're in the new unified MIDI learn mode
    if (synthParams->midiLearnActive.load()) {
        int paramId = synthParams->midiLearnParameterId.load();
        
        // Learn CC for any parameter
        if (paramId >= 0 && paramId < SynthParameters::kMaxParamMap) {
            synthParams->parameterCCMap[paramId] = controller;
            synthParams->midiLearnActive = false;
            synthParams->midiLearnParameterId = -1;
            
            // Also update legacy filterCutoffCC if it's the filter cutoff parameter
            if (paramId == 32) {
                synthParams->filterCutoffCC = controller;
            }
            
            if (ui) {
                std::string paramName = ui->getParameterName(paramId);
                ui->addConsoleMessage("Learned CC#" + std::to_string(controller) + " for " + paramName);
            }
            return;  // Exit early after learning
        }
    }
}
```

### 5.3 MIDI CC Application
**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/main.cpp` (Lines 554-562)

```cpp
// Process CC messages - check if any parameter is mapped to this controller
for (int paramId = 0; paramId < 50; ++paramId) {
    int mappedCC = synthParams->parameterCCMap[paramId].load();
    if (mappedCC >= 0 && mappedCC == controller) {
        applyMIDICCToParameter(paramId, value);
    }
}
```

**ISSUE:** Loop only checks parameters 0-49! Should check all 1024.

### 5.4 Parameter CC Mapping Function
**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/main.cpp` (Lines 108-481)

Large switch statement mapping parameter IDs to CC values. Currently covers:
- Parameters 1-42 (Oscillator, Reverb, Filter, Looper, Mixer)
- Parameters 50-68 (Mixer Sampler levels, Sampler parameters)
- Parameters 200-206 (LFO parameters)
- Parameters 300-323 (Envelope parameters - all 4 envelopes)
- **Parameters 350-353, 354-355 PARTIALLY** (Chaos parameters - BUT...)
- Parameter 360 (FM Global Depth)
- Parameters 410-413 (Chaos mixer levels) **MISSING**

**MISSING CHAOS CC MAPPINGS:**
- Parameter 354: FAST mode (not in applyMIDICCToParameter switch)
- Parameter 355: DIFF mode (not in applyMIDICCToParameter switch)
- Parameters 410-413: Chaos mixer levels (not in applyMIDICCToParameter switch)

---

## 6. MIDI LEARN SUPPORT STATUS

### 6.1 Parameters WITH MIDI Learn Support

**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/ui/ui_parameters.cpp`

Looking at the parameter initialization (supports_midi_learn flag):
- OSC page parameters: YES (mode, freq, morph, duty, ratio, offset, amp)
- LFO page parameters: YES
- Reverb page parameters: YES (mostly)
- Filter page parameters: YES
- Looper page parameters: NO (immune)
- Mixer page parameters: NO (immune - prevents silencing)
- Sampler page parameters: YES (except key mode, sample selector)
- Envelope page parameters: YES
- **Chaos page parameters: PARTIAL** - 350-353 have support, 354-355 added but not in CC switch

### 6.2 Parameters MISSING MIDI Learn Support

From code analysis, these have `supports_midi_learn = false`:

```cpp
// MAIN PAGE
6:  Master Volume (false - IMMUNE but should be learn-able!)

// OSCILLATOR
10: Mode KEY/FREE (false - IMMUNE)

// REVERB
21: Reverb Enabled (false - IMMUNE)

// FILTER
31: Filter Enabled (false - IMMUNE)

// LOOPER
40: Current Loop (false - IMMUNE)
41: Overdub Mix (false - IMMUNE)

// MIXER
50-57: OSC/SAMP Levels (false - IMMUNE, prevents silencing)

// SAMPLER
60: Key Mode (false - IMMUNE)
69: Sample selector (false - special UI control)

// CONFIG
400: CPU Monitor (false - IMMUNE)

// CHAOS
353: Running (false - IMMUNE)
354: FAST (true - but not in applyMIDICCToParameter!)
355: DIFF (true - but not in applyMIDICCToParameter!)

// MIXER (Chaos)
410-413: CHAOS Levels (false - IMMUNE)
```

---

## 7. CODE IMPLEMENTATION PATTERNS

### 7.1 Adding a New Modulation Destination - Pattern

**Step 1:** Add to `ui_mod_data.cpp` buildDestinationModules():
```cpp
{"Chaos 1", {
    {"Parameter", "C1:Param"},
    {"Clock", "C1:Clock"},
    {"Interp", "C1:Interp"},
    {"FAST", "C1:FAST"},
}}
```

**Step 2:** Create struct field in `synth.h` ModulationOutputs:
```cpp
struct ModulationOutputs {
    // ... existing fields
    float chaos1Parameter = 0.0f;
    float chaos1Clock = 0.0f;
    float chaos1Interp = 0.0f;
    float chaos1Fast = 0.0f;
    // ... repeat for chaos 2-4
};
```

**Step 3:** Add switch cases in `synth.cpp` processModulationMatrix():
```cpp
// Chaos 1 (assuming indices 83-86)
case 83: outputs.chaos1Parameter += modValue; break;
case 84: outputs.chaos1Clock += modValue; break;
case 85: outputs.chaos1Interp += modValue; break;
case 86: outputs.chaos1Fast += modValue; break;
// Repeat for Chaos 2-4...
```

**Step 4:** Apply the modulation values somewhere in voice processing or synth update.

### 7.2 Adding MIDI CC Support for a Parameter - Pattern

**Step 1:** Add to parameter CC loop in `main.cpp` onControlChange():
```cpp
// Extend loop from 50 to include new parameters
for (int paramId = 0; paramId < 500; ++paramId) {  // Increased range
    int mappedCC = synthParams->parameterCCMap[paramId].load();
    if (mappedCC >= 0 && mappedCC == controller) {
        applyMIDICCToParameter(paramId, value);
    }
}
```

**Step 2:** Add case in applyMIDICCToParameter() switch:
```cpp
case 354:  // FAST mode (BOOL)
    synthParams->setChaosFastMode(0, ccValue > 63);  // or similar
    if (synth) synth->setChaosFastMode(0, ccValue > 63);
    break;
case 355:  // DIFF mode (BOOL)
    synthParams->chaosDiff = (ccValue > 63);
    if (synth) synth->setChaosDiffMode(ccValue > 63);
    break;
case 410:  // CHAOS 1 Level (linear, 0.0-1.0)
    synthParams->setChaosLevel(0, mapCCToParameter(ccValue, 0.0f, 1.0f));
    break;
// ... repeat for 411-413
```

**Step 3:** Ensure SynthParameters has appropriate setter methods:
```cpp
void setChaosParameter(int index, float value);
void setChaosClockFreq(int index, float freq);
void setChaosFastMode(int index, bool fast);
void setChaosLevel(int index, float level);
```

---

## 8. MODULATION SOURCE GETTER METHODS

**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/synth.cpp`

The `getModulationSource()` method returns values for each source index:

```cpp
float Synth::getModulationSource(int sourceIndex, const Voice* voiceContext) {
    switch (sourceIndex) {
        case 0-3:  // LFO 1-4
            return getLFOOutput(sourceIndex);
        case 4-7:  // ENV 1-4
            return voiceContext ? voiceContext->envelopes[sourceIndex-4].output : 0.0f;
        case 8:    // Velocity
            return voiceContext ? voiceContext->velocity / 127.0f : 0.0f;
        case 9:    // Aftertouch
            return 0.0f;  // Not implemented
        case 10:   // Mod Wheel
            return 0.0f;  // Not implemented
        case 11:   // Pitch Bend
            return 0.0f;  // Not implemented
        case 12:   // Clock
            return clock ? clock->getPhase() : 0.0f;
        case 13-20: // Chaos X/Y
            return getChaosOutput(...) or getChaosOutputY(...);
    }
}
```

---

## 9. SUMMARY OF NEEDED CHANGES

### 9.1 To Add Chaos Parameters as Modulation Destinations:

**Location: `ui_mod_data.cpp`**
1. Add Chaos modules to buildDestinationModules() (4 modules x 5 params = 20 destinations)
   - Suggested indices: 83-102

**Location: `synth.h`**
2. Add fields to ModulationOutputs struct:
   - chaos1Parameter, chaos1Clock, chaos1Interp, chaos1Fast
   - chaos2Parameter, chaos2Clock, chaos2Interp, chaos2Fast
   - chaos3Parameter, chaos3Clock, chaos3Interp, chaos3Fast
   - chaos4Parameter, chaos4Clock, chaos4Interp, chaos4Fast
   - (16 new fields total)

**Location: `synth.cpp`**
3. Add 16 switch cases in processModulationMatrix():
   - Cases 83-102 applying modulation to chaos parameters

4. Apply the modulation values in voice processing or synth update loop:
   - Clamp values appropriately
   - Apply modulation to the respective chaos parameters

### 9.2 To Add MIDI CC Support for Missing Parameters:

**Location: `main.cpp` onControlChange() function (line ~557)**
1. Fix loop range from 50 to at least 500:
   ```cpp
   for (int paramId = 0; paramId < 500; ++paramId) {
   ```

**Location: `main.cpp` applyMIDICCToParameter() function**
2. Add missing case statements:
   ```cpp
   case 354:  // FAST mode
   case 355:  // DIFF mode
   case 410:  // CHAOS 1 Level
   case 411:  // CHAOS 2 Level
   case 412:  // CHAOS 3 Level
   case 413:  // CHAOS 4 Level
   ```

3. Implement case handlers similar to existing patterns (use mapCCToParameter)

### 9.3 Optional Improvements:

**In ui.h:** Add setter methods to SynthParameters if not already present:
```cpp
void setChaosParameter(int index, float value);
void setChaosClockFreq(int index, float freq);
void setChaosFastMode(int index, bool fast);
void setChaosInterpMode(int index, int mode);
void setChaosRunning(int index, bool running);
void setChaosLevel(int index, float level);
```

**In ui_parameters.cpp:** Update parameter initialization to mark FAST/DIFF as supports_midi_learn=true (already done).

---

## 10. KEY FILES AND LINE REFERENCES

| File | Purpose | Lines |
|------|---------|-------|
| modulation.h | Modulation slot structure | 1-40 |
| ui_mod_data.cpp | Destination definitions | 40-192 |
| synth.cpp | Destination application | 1071-1173 |
| synth.h | ModulationOutputs struct | 77-148 |
| ui.h | Parameter definitions | 51-54, 92-100 |
| ui_parameters.cpp | Parameter list & getters/setters | 32-187, 189-413 |
| main.cpp | MIDI CC mapping & learning | 92-481, 496-573 |
| chaos.h | Chaos generator class | 1-179 |
| ui_page_chaos.cpp | Chaos UI page | 1-150+ |

---

## 11. IMPLEMENTATION NOTES

1. **Modulation Update Flow:**
   - User defines routing in modulation matrix (16 slots)
   - During audio processing, `processModulationMatrix()` is called
   - For each slot, source value is fetched, curve is applied, and destination is updated
   - Modulation outputs are applied to parameters by voice/synth processing

2. **MIDI CC Learning Flow:**
   - User presses "Learn CC" on a parameter
   - `midiLearnActive` is set to true, `midiLearnParameterId` stores target
   - Next CC message stores the CC number in `parameterCCMap[paramId]`
   - User message confirms learning

3. **MIDI CC Application Flow:**
   - Every CC message triggers `onControlChange()`
   - Loop checks if any learned parameter is mapped to this CC
   - `applyMIDICCToParameter()` converts CC value (0-127) to parameter range
   - Uses `mapCCToParameter()` helper which supports linear/logarithmic mapping

4. **Chaos-Specific Consideration:**
   - Chaos X/Y outputs are already sources (indices 13-20)
   - Chaos parameters (350-355) need to be modulation destinations
   - Chaos mixer levels (410-413) need MIDI CC support
   - FAST/DIFF modes should support modulation since they're randomizable

