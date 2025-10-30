# Implementation Code Snippets

## 1. ADD CHAOS DESTINATIONS TO ui_mod_data.cpp

**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/ui/ui_mod_data.cpp`

**Location:** After the FM module definition (around line 109), add:

```cpp
        {"Chaos 1", {
            {"Parameter", "C1:Param"},
            {"Clock", "C1:Clock"},
            {"Interp", "C1:Interp"},
            {"FAST", "C1:FAST"}
        }},
        {"Chaos 2", {
            {"Parameter", "C2:Param"},
            {"Clock", "C2:Clock"},
            {"Interp", "C2:Interp"},
            {"FAST", "C2:FAST"}
        }},
        {"Chaos 3", {
            {"Parameter", "C3:Param"},
            {"Clock", "C3:Clock"},
            {"Interp", "C3:Interp"},
            {"FAST", "C3:FAST"}
        }},
        {"Chaos 4", {
            {"Parameter", "C4:Param"},
            {"Clock", "C4:Clock"},
            {"Interp", "C4:Interp"},
            {"FAST", "C4:FAST"}
        }}
```

This adds 4 modules x 4 parameters = 16 new destinations (indices 83-98)

---

## 2. ADD MODULATION OUTPUT FIELDS TO synth.h

**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/synth.h`

**Location:** After the FM depth field in ModulationOutputs struct (around line 147), add:

```cpp
        // Chaos 1 modulation
        float chaos1Parameter = 0.0f;
        float chaos1Clock = 0.0f;
        float chaos1Interp = 0.0f;
        float chaos1Fast = 0.0f;
        // Chaos 2 modulation
        float chaos2Parameter = 0.0f;
        float chaos2Clock = 0.0f;
        float chaos2Interp = 0.0f;
        float chaos2Fast = 0.0f;
        // Chaos 3 modulation
        float chaos3Parameter = 0.0f;
        float chaos3Clock = 0.0f;
        float chaos3Interp = 0.0f;
        float chaos3Fast = 0.0f;
        // Chaos 4 modulation
        float chaos4Parameter = 0.0f;
        float chaos4Clock = 0.0f;
        float chaos4Interp = 0.0f;
        float chaos4Fast = 0.0f;
```

---

## 3. ADD SWITCH CASES TO synth.cpp processModulationMatrix()

**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/synth.cpp`

**Location:** After case 82 (FM Global Depth), around line 1172, add:

```cpp
            // Chaos 1
            case 83: outputs.chaos1Parameter += modValue; break;
            case 84: outputs.chaos1Clock += modValue; break;
            case 85: outputs.chaos1Interp += modValue; break;
            case 86: outputs.chaos1Fast += modValue; break;
            // Chaos 2
            case 87: outputs.chaos2Parameter += modValue; break;
            case 88: outputs.chaos2Clock += modValue; break;
            case 89: outputs.chaos2Interp += modValue; break;
            case 90: outputs.chaos2Fast += modValue; break;
            // Chaos 3
            case 91: outputs.chaos3Parameter += modValue; break;
            case 92: outputs.chaos3Clock += modValue; break;
            case 93: outputs.chaos3Interp += modValue; break;
            case 94: outputs.chaos3Fast += modValue; break;
            // Chaos 4
            case 95: outputs.chaos4Parameter += modValue; break;
            case 96: outputs.chaos4Clock += modValue; break;
            case 97: outputs.chaos4Interp += modValue; break;
            case 98: outputs.chaos4Fast += modValue; break;
```

---

## 4. ADD MODULATION APPLICATION IN synth.cpp (optional but recommended)

**Location:** Where modulation outputs are applied to parameters (in voice render or control update)

This should clamp and apply the modulation:

```cpp
// Apply chaos modulation
for (int i = 0; i < 4; ++i) {
    switch (i) {
        case 0: {
            float param = std::clamp(params->getChaosParameter(0) + lastGlobalModOutputs.chaos1Parameter, 0.0f, 1.0f);
            float clock = std::clamp(params->getChaosClockFreq(0) + lastGlobalModOutputs.chaos1Clock, 0.00001f, 20000.0f);
            int interp = std::clamp(static_cast<int>(params->getChaosInterpMode(0) + lastGlobalModOutputs.chaos1Interp), 0, 2);
            bool fast = (params->getChaosFastMode(0) + lastGlobalModOutputs.chaos1Fast) > 0.5f;
            
            setChaosParameter(0, param);
            setChaosClockFreq(0, clock);
            setChaosInterpMode(0, interp);
            setChaosFastMode(0, fast);
            break;
        }
        // ... repeat for 1, 2, 3
    }
}
```

---

## 5. FIX MIDI CC PARAMETER CHECK IN main.cpp

**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/main.cpp`

**Location:** Around line 557 in onControlChange(), change:

**FROM:**
```cpp
    // Process CC messages - check if any parameter is mapped to this controller
    for (int paramId = 0; paramId < 50; ++paramId) {
        int mappedCC = synthParams->parameterCCMap[paramId].load();
        if (mappedCC >= 0 && mappedCC == controller) {
            applyMIDICCToParameter(paramId, value);
        }
    }
```

**TO:**
```cpp
    // Process CC messages - check if any parameter is mapped to this controller
    for (int paramId = 0; paramId < SynthParameters::kMaxParamMap; ++paramId) {
        int mappedCC = synthParams->parameterCCMap[paramId].load();
        if (mappedCC >= 0 && mappedCC == controller) {
            applyMIDICCToParameter(paramId, value);
        }
    }
```

Or more efficiently, with early exit:
```cpp
    // Process CC messages - check if any parameter is mapped to this controller
    for (int paramId = 0; paramId < 500; ++paramId) {  // Check up to 500
        int mappedCC = synthParams->parameterCCMap[paramId].load();
        if (mappedCC >= 0 && mappedCC == controller) {
            applyMIDICCToParameter(paramId, value);
            return;  // Only one parameter per CC
        }
    }
```

---

## 6. ADD MISSING MIDI CC CASES IN main.cpp

**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/main.cpp`

**Location:** In applyMIDICCToParameter() switch statement, add after case 353:

```cpp
        case 354:  // FAST mode (BOOL)
            {
                bool fast = ccValue > 63;
                synthParams->setChaosFastMode(0, fast);
                if (synth) synth->setChaosFastMode(0, fast);
            }
            break;
        case 355:  // DIFF mode (BOOL) - applies globally to all chaos
            {
                bool diff = ccValue > 63;
                synthParams->chaosDiff = diff;
                if (synth) synth->setChaosDiffMode(diff);
            }
            break;

        // CHAOS mixer levels (410-413)
        case 410:  // CHAOS 1 Level (linear)
            synthParams->setChaosLevel(0, mapCCToParameter(ccValue, 0.0f, 1.0f));
            break;
        case 411:  // CHAOS 2 Level (linear)
            synthParams->setChaosLevel(1, mapCCToParameter(ccValue, 0.0f, 1.0f));
            break;
        case 412:  // CHAOS 3 Level (linear)
            synthParams->setChaosLevel(2, mapCCToParameter(ccValue, 0.0f, 1.0f));
            break;
        case 413:  // CHAOS 4 Level (linear)
            synthParams->setChaosLevel(3, mapCCToParameter(ccValue, 0.0f, 1.0f));
            break;
```

---

## 7. VERIFY SETHPARAM METHODS EXIST

**File:** `/srv/storage/Dropbox/_brian-varren/the-last-node/wakefield/src/ui.h`

Verify these methods exist in SynthParameters struct (they should already):

```cpp
void setChaosParameter(int index, float value);
void setChaosClockFreq(int index, float freq);
void setChaosFastMode(int index, bool fast);
void setChaosInterpMode(int index, int mode);
void setChaosRunning(int index, bool running);
void setChaosLevel(int index, float level);
```

If not present, add them to the SynthParameters class.

---

## 8. SUMMARY OF CHANGES BY FILE

### ui_mod_data.cpp
- **Add:** 4 Chaos modules (16 destinations total)
- **Lines:** After line 109

### synth.h
- **Add:** 16 chaos modulation output fields to ModulationOutputs
- **Lines:** After line 147

### synth.cpp
- **Add:** 16 switch cases for chaos modulation destinations (lines 1173)
- **Optional:** Apply modulation logic to chaos parameters

### main.cpp
- **Fix:** Parameter CC loop range (line 557) - change `50` to `500` or `kMaxParamMap`
- **Add:** 6 case statements in applyMIDICCToParameter() (354, 355, 410-413)
- **Location:** After line 466, add the new cases

---

## 9. TESTING CHECKLIST

After implementation:

- [ ] Modulation matrix accepts Chaos modules as destinations
- [ ] Can route LFO to Chaos Clock for clock modulation
- [ ] Can route LFO to Chaos Parameter for chaotic variation
- [ ] Can route ENV to Chaos FAST mode
- [ ] Parameter 354 (FAST) responds to MIDI learn
- [ ] Parameter 355 (DIFF) responds to MIDI learn
- [ ] Parameters 410-413 (Chaos levels) respond to MIDI learn
- [ ] Chaos modulation values are properly clamped to valid ranges
- [ ] UI modulation matrix page displays chaos modules correctly

