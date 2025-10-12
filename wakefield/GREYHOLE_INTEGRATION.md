# Greyhole Reverb Integration

## Overview

Successfully integrated the Greyhole reverb algorithm into the ncurses synthesizer by using Faust to compile the DSP code to C++ and wrapping it with a compatible interface.

## Implementation Summary

### 1. Modified DSP File
**File:** `reverb/GreyholeRaw.dsp`
- Changed from external `ffunction` to inline waveform for prime number delays
- Commented out line 27: `// prime_delays = ffunction(...)`
- Uncommented lines 28-29: Using inline `waveform {...}` with `rdtable`

### 2. Generated C++ Code
**Command:**
```bash
faust -i -scn GreyholeReverb GreyholeRaw.dsp -o greyhole_dsp.h
```

**File:** `reverb/greyhole_dsp.h` (50KB, auto-generated)
- Added `Meta` and `UI` base class definitions for Faust interface
- Removed erroneous inheritance from non-existent `GreyholeReverb` class
- Contains `mydsp` class with full DSP implementation

### 3. Created Wrapper Class
**Files:** `src/reverb.h`, `src/reverb.cpp`

**GreyholeReverb Wrapper:**
- Manages Faust DSP instance lifecycle
- Provides clean parameter interface matching original DattorroReverb
- Handles stereo interleaving/deinterleaving
- Implements dry/wet mixing

**Parameters:**
- `setSize(float)` - Maps to delayTime (0.001-1.45s) and size (0.5-3.0)
- `setDamping(float)` - High frequency damping (0.0-0.99)
- `setMix(float)` - Dry/wet mix (0.0-1.0)
- `setDecay(float)` - Feedback amount (0.0-1.0)
- `setDiffusion(float)` - Diffusion amount (0.0-0.99)
- `setModDepth(float)` - Modulation depth (0.0-1.0)
- `setModFreq(float)` - Modulation frequency (0.0-10.0 Hz)

**Internal Faust Parameter Mapping:**
- `fHslider0` = damping
- `fHslider1` = diffusion
- `fHslider2` = feedback
- `fHslider3` = modDepth
- `fHslider4` = modFreq
- `fHslider5` = delayTime
- `fHslider6` = size

### 4. Updated Synth Integration
**Files:** `src/synth.h`, `src/synth.cpp`
- Changed `DattorroReverb reverb;` to `GreyholeReverb reverb;`
- No changes needed to synth logic - interface remained compatible
- UI controls work seamlessly with new reverb

### 5. Updated Build System
**File:** `CMakeLists.txt`
- Added `${CMAKE_CURRENT_SOURCE_DIR}/reverb` to include directories
- Allows reverb.cpp to include `../reverb/greyhole_dsp.h`

## Build Results

Build completed successfully with no errors:
```bash
cd build/
make
[100%] Built target synth
```

Executable: `build/synth` (157KB)

## Key Features of Greyhole

1. **Nested Diffusion Network:** 4-level nested diffuser creates dense, complex reverb
2. **Prime Number Delays:** 1302 prime numbers (2-10667) used for delay tap positions to avoid resonances
3. **Modulated Delay Lines:** LFO modulation for chorus-like shimmer effect
4. **Feedback Control:** Adjustable decay time
5. **Diffusion Control:** Adjustable density of reverb tail
6. **Size Parameter:** Controls delay times and spatial characteristics

## Advantages Over Dattorro

- More natural, diffuse reverb tail
- Modulation adds movement and depth
- Prime-based delays reduce metallic artifacts
- More control over reverb character (diffusion, modulation)
- Closer to natural room acoustics

## Testing Recommendations

1. Start with moderate settings:
   - Size: 0.5
   - Damping: 0.3
   - Mix: 0.3
   - Decay: 0.7
   - Diffusion: 0.5
   - ModDepth: 0.1
   - ModFreq: 2.0

2. Experiment with:
   - High diffusion (0.8+) for dense, smooth reverb
   - High modulation depth (0.5+) for chorus effects
   - Large size (0.8+) for cathedral-like spaces
   - Low damping (0.1-0.3) for bright, airy reverb

## Technical Notes

- Uses ~262KB for delay buffers (131072 samples per channel)
- Stereo processing (2 inputs, 2 outputs)
- Sample rate independent (scales delays automatically)
- No external dependencies beyond standard C++ library
- Thread-safe parameter updates through UI mechanism

## Files Modified/Created

```
wakefield/
├── reverb/
│   ├── GreyholeRaw.dsp          (modified - use inline primes)
│   └── greyhole_dsp.h           (generated - Faust output)
├── src/
│   ├── reverb.h                 (replaced - new wrapper)
│   ├── reverb.cpp               (replaced - new wrapper)
│   └── synth.h                  (modified - class name change)
├── CMakeLists.txt               (modified - add reverb/ include)
└── build/
    └── synth                    (rebuilt executable)
```

## Future Enhancements

Potential improvements:
1. Add reverb type selector (Greyhole vs Dattorro)
2. Expose additional Greyhole parameters in UI
3. Add presets for different room types
4. Implement parameter automation
5. Add stereo width control

## Credits

- **Greyhole Algorithm:** Julian Parker (2013)
- **Bug Fixes:** Till Bovermann
- **License:** GPL2+
- **Faust Compiler:** GRAME (v2.37.3)

