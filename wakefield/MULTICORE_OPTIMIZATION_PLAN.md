# Multi-Core Voice Processing Optimization Plan

## Professional Softsynth Approach

Professional synthesizers (like Serum, Massive, etc.) use these patterns:

### 1. **Lock-Free Per-Voice Processing**
- Each voice is independent and can be processed in parallel
- No shared state during voice generation
- Each voice writes to its own buffer
- Final mix happens sequentially (simple sum, very fast)

### 2. **Work Distribution Strategies**

**Option A: Static Partitioning (Simplest, Good for Fixed Polyphony)**
- Divide voices evenly across cores
- With 8 voices and 4 cores: 2 voices per core
- Pros: Predictable, low overhead
- Cons: Load imbalance if voices have different complexity

**Option B: Work Stealing (Best for Variable Polyphony)**
- Each thread has a queue of voices to process
- Threads steal work when their queue is empty
- Pros: Better load balancing
- Cons: More complex, requires atomic operations

**Option C: Dynamic Task Pool (Good Middle Ground)**
- Create tasks for each voice
- Thread pool processes tasks
- Pros: Good load balancing, easier than work stealing
- Cons: Some task queue overhead

### 3. **Recommended Implementation: Static Partitioning**

For 8 voices max and 4 cores, static partitioning is ideal:
- **Core 1 (Main)**: Audio callback coordination, MIDI, parameter updates, final mix
- **Core 2**: Voices 0-1
- **Core 3**: Voices 2-3  
- **Core 4**: Voices 4-5
- **Core 1 (after processing)**: Voices 6-7 + final mix

### 4. **Implementation Details**

#### Thread Safety Requirements

**Safe to Access Concurrently:**
- `SynthParameters` (atomic reads)
- `oscillatorBaseLevels`, `oscillatorBaseAmps` (read-only per buffer)
- Voice-specific modulation arrays (each voice has its own)

**Must Be Sequential:**
- Final mixing (simple sum, very fast)
- FM source buffer accumulation (needs coordination)
- UI oscilloscope buffer writes (only first voice)

#### Voice Processing Pipeline

```
Per-Buffer (Main Thread):
1. Process modulation matrix (sequential)
2. Copy modulation values to voice structures
3. Pre-compute FM depths (sequential, but fast)

Per-Sample (Parallel):
4. Generate samples for each voice in parallel
   - Each voice writes to temporary buffer
   - No shared state, completely independent

Post-Processing (Main Thread):
5. Accumulate FM source buffer (sequential)
6. Mix all voice outputs (simple sum)
7. Apply effects (reverb, filter)
```

#### Code Structure

```cpp
// Per-voice processing function (thread-safe)
void processVoiceBuffer(Voice& voice, float* output, 
                       unsigned int nFrames, unsigned int nChannels) {
    // Generate samples - completely independent
    for (unsigned int i = 0; i < nFrames; ++i) {
        float sample = voice.generateSample(i);
        // Write to voice-local buffer
        for (unsigned int ch = 0; ch < nChannels; ++ch) {
            output[i * nChannels + ch] = sample;
        }
    }
}

// Main process function
void Synth::process(float* output, unsigned int nFrames, unsigned int nChannels) {
    // 1. Sequential: modulation matrix, FM depth pre-compute
    ModulationOutputs globalModOutputs = processModulationMatrix();
    // ... copy to voices ...
    
    // 2. Parallel: voice processing
    std::vector<float> voiceBuffers[MAX_VOICES];
    for (int v = 0; v < MAX_VOICES; ++v) {
        if (voices[v].active) {
            voiceBuffers[v].resize(nFrames * nChannels);
        }
    }
    
    // Process voices in parallel (C++17 parallel algorithms or thread pool)
    std::for_each(std::execution::par_unseq, 
                  activeVoices.begin(), activeVoices.end(),
                  [&](int v) {
                      processVoiceBuffer(voices[v], voiceBuffers[v].data(), 
                                        nFrames, nChannels);
                  });
    
    // 3. Sequential: mixing and effects
    // Sum all voice buffers
    // Apply reverb, filter
    // Write to output
}
```

## Implementation Steps

### Phase 1: Thread Pool Setup
1. Add thread pool (or use C++17 parallel algorithms)
2. Detect CPU core count: `std::thread::hardware_concurrency()`
3. Create worker threads (num_cores - 1, keep one for main)

### Phase 2: Voice Buffer Allocation
1. Allocate per-voice temporary buffers
2. Ensure alignment for SIMD (16-byte for AVX, 32-byte for AVX2)

### Phase 3: Parallel Voice Processing
1. Split voice processing into parallel tasks
2. Use static partitioning: voices / (cores - 1)
3. Process voices concurrently
4. Accumulate FM source buffer after voice processing

### Phase 4: Sequential Post-Processing
1. Sum voice buffers (sequential, but fast - O(n))
2. Apply global effects (reverb, filter)
3. Final output write

## Safety Considerations

### Memory Alignment
- Voice buffers should be aligned for SIMD
- Use `alignas(32)` for AVX2 compatibility

### Cache Coherency
- Keep voice data structures cache-friendly
- Minimize false sharing (pad structures if needed)

### Atomic Operations
- Parameter reads are already atomic
- No additional synchronization needed

### FM Source Buffer
- Currently accumulates during voice processing
- Solution: Accumulate in parallel per-voice, then sum sequentially

## Expected Performance

**Current (Single Core):**
- Voice processing: ~11.84 seconds for 20k buffers
- Per buffer: ~0.59ms

**With 4-Core Parallelization:**
- Voice processing: ~3-4 seconds (60-70% reduction)
- Per buffer: ~0.15-0.20ms
- **Total improvement: ~66% faster**

## Code Files to Modify

1. `src/synth.cpp` - Main process function
2. `src/synth.h` - Add thread pool, voice buffer storage
3. `src/voice.cpp` - Extract per-voice processing function
4. `CMakeLists.txt` - Add threading library dependency

## Dependencies

- C++17 `<execution>` header (parallel algorithms)
- OR: Thread pool library (e.g., `std::thread`, `std::mutex`)
- OR: OpenMP (simpler, but less control)

## Recommendation

Start with **C++17 parallel algorithms** (`std::execution::par_unseq`) as it's:
- Standard library (no dependencies)
- Compiler-optimized
- Easy to implement
- Good performance

If compiler doesn't support it, fall back to manual thread pool with `std::thread`.

