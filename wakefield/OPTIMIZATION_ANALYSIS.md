# Performance Optimization Analysis

Based on gprof profiling data from `profile_report.txt`

## Critical Issues (Highest Priority)

### 1. **Synth::getFMDepthMod() - 39.75% CPU time** ⚠️ CRITICAL
- **Calls:** 2,621,440,000 (2.6 billion)
- **Time:** 4.85 seconds / 12.20 seconds total
- **Problem:** Called once per FM matrix lookup, which happens for every oscillator target × source combination per sample
- **Calculation:** 
  - 40.96M voice samples × 4 targets × 8 sources × 2 (osc + sampler) = ~2.6B calls
  - Each call does bounds checking and array lookup: `lastGlobalModOutputs.fmDepth[target][source]`
- **Root Cause:** FM depth lookup happens per-sample instead of being cached/pre-computed
- **Optimization Strategy:**
  - **IMMEDIATE:** Cache FM depth values at control rate (per buffer or per voice)
  - The `lastGlobalModOutputs` is already computed per-frame, but we're still doing array lookups per-sample
  - Move FM depth calculation to voice-level modulation storage (already has `fmDepthMod` array)
  - Pre-compute all FM depths once per buffer for each voice instead of looking up 64 times per sample
  
### 2. **Voice::generateSample() - 34.75% CPU time** ⚠️ HIGH
- **Calls:** 40,960,000 (40.96 million)
- **Time:** 4.24 seconds
- **Breakdown within:**
  - FM depth lookups (via getFMDepthMod): 4.85s
  - BrainwaveOscillator::process: 1.27s  
  - Sampler::process: 0.32s
- **Problem:** This is the core synthesis loop - any optimization here has huge impact
- **Optimization Strategy:**
  - Cache FM matrix lookups (see #1 above)
  - Consider SIMD for oscillator processing (4 oscillators could use AVX)
  - Reduce function call overhead - inline hot paths

### 3. **BrainwaveOscillator::process() - 10.41% CPU time** ⚠️ MEDIUM
- **Calls:** 163,840,000 (163.84 million)
- **Time:** 1.27 seconds
- **Optimization Strategy:**
  - Vectorize oscillator generation (process multiple oscillators at once)
  - Cache frequently accessed values
  - Consider lookup tables for expensive operations

### 4. **BrainwaveOscillator::generateSample() - 5.98% CPU time** ⚠️ MEDIUM  
- **Calls:** 163,840,000
- **Time:** 0.73 seconds
- **Optimization Strategy:**
  - Inline this function if not already
  - Use fast math approximations where audio quality allows
  - SIMD operations for waveform generation

## Secondary Issues

### 5. **Sampler::process() - 2.62% CPU time**
- **Calls:** 163,840,000
- **Time:** 0.32 seconds
- **Optimization Strategy:**
  - Optimize sample playback interpolation
  - Cache sample pointers/offsets

### 6. **Synth::getMixerSamplerLevelMod() - 1.72% CPU time**
- **Calls:** 163,840,000  
- **Time:** 0.21 seconds
- **Problem:** Simple array lookup called 4× per voice sample
- **Optimization Strategy:**
  - Cache in voice modulation storage (like FM depth)
  - Pre-compute per buffer

### 7. **Synth::getModulatedOscLevel() - 1.31% CPU time**
- **Calls:** 163,840,000
- **Time:** 0.16 seconds  
- **Problem:** Called 4× per voice sample (once per oscillator)
- **Optimization Strategy:**
  - Cache in voice modulation storage
  - Pre-compute per buffer

## Multi-Core Delegation Strategy

Based on the profile, here's what can be parallelized:

### **Core 1 (Main Thread):**
- Audio callback coordination
- MIDI processing
- Control parameter updates
- Buffer management

### **Core 2-4 (Worker Threads):**
Each voice can be processed independently in parallel:

1. **Voice Processing (97% of CPU time)**
   - `Voice::generateSample()` can run in parallel for each voice
   - With 8 voices, distribute: 2-3 voices per core
   - Each voice is ~4.24s / 40.96M = ~0.1μs per sample
   - Parallelization: Process 2-3 voices per thread

2. **Oscillator Processing**
   - Each voice has 4 oscillators that could be SIMD'd
   - But oscillators within a voice have dependencies (FM), so keep per-voice

3. **Modulation Matrix**
   - Pre-compute modulation outputs per-frame (already done)
   - Copy to voice-local storage per buffer
   - Eliminates per-sample lookups

## Immediate Action Items (Priority Order)

### Phase 1: Quick Wins (1-2 days)
1. **Cache FM depth lookups in Voice structure**
   - Pre-compute all `fmDepthMod[target][source]` values once per buffer
   - Store in `Voice::fmDepthMod` array (already exists!)
   - Eliminates 2.6B function calls → 39.75% CPU reduction

2. **Cache level/mixer mod values in Voice**
   - Pre-compute `getModulatedOscLevel()` and `getMixerSamplerLevelMod()` per buffer
   - Store in voice modulation arrays
   - Eliminates 327M function calls → ~3% CPU reduction

3. **Reduce bounds checking overhead**
   - Use assertions in debug, remove in release
   - Or use template specialization for hot paths

### Phase 2: Voice Parallelization (3-5 days)
4. **Thread pool for voice processing**
   - Create worker threads (1 per CPU core - 1)
   - Distribute voices across threads
   - Lock-free communication via per-voice buffers
   - Expected: 60-70% reduction in voice processing time

### Phase 3: SIMD Optimization (1 week)
5. **Vectorize oscillator processing**
   - Use AVX/SSE for processing 4 oscillators simultaneously
   - Requires restructuring oscillator storage
   - Expected: 30-40% reduction in oscillator time

6. **Vectorize sample mixing**
   - SIMD for summing oscillator outputs
   - SIMD for envelope application

## Expected Performance Gains

**Current:** 12.20 seconds for 20000 buffers = ~0.61ms per buffer (worst case)

**After Phase 1 (caching):**
- Remove 39.75% + 3% = ~43% CPU overhead
- New time: ~7 seconds = ~0.35ms per buffer
- **43% improvement**

**After Phase 2 (parallelization):**
- Voice processing (97% of remaining): 60% reduction
- New time: ~4 seconds = ~0.20ms per buffer  
- **67% total improvement**

**After Phase 3 (SIMD):**
- Additional 10-15% reduction
- Final: ~3.5 seconds = ~0.18ms per buffer
- **71% total improvement**

## Code Locations for Optimization

1. **FM Depth Caching:**
   - `src/voice.cpp:65-75` - `getModulatedDepth` lambda
   - `src/voice.cpp:82-100` - FM input calculation loop
   - `src/synth.cpp:1676-1682` - `getFMDepthMod` implementation

2. **Modulation Pre-computation:**
   - `src/synth.cpp:425-550` - `processModulationMatrix` (already per-frame)
   - `src/voice.cpp:26-42` - Voice modulation storage arrays

3. **Voice Parallelization:**
   - `src/synth.cpp:process()` - Main synthesis loop
   - `src/voice.cpp:generateSample()` - Per-voice processing

## Notes

- The FM matrix is 8×8 (64 cells), processed 4× per sample per voice
- With 8 voices × 256 samples = 2,048 voice samples per buffer
- Each voice sample processes 4 oscillators with up to 8 FM sources each
- Total FM lookups per buffer: 2,048 × 4 × 8 = 65,536 lookups
- Currently doing array bounds checks + lookup for each = expensive

