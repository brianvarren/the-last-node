// ============================================================================
// SYNTH PROCESS - Audio Pipeline and Parallel Effects Processing
// ============================================================================
//
// RESPONSIBILITIES:
// - Main audio callback: process() orchestrates voice rendering and effects
// - Sequential voice rendering: renderVoicesSequential() processes all voices
//   on a single thread with sample-accurate global FM (no synchronization overhead)
// - Parallel effects thread: effectsThreadFunc() runs filter/reverb/compressor
//   on a separate core while voices render the next buffer (pipeline parallelism)
// - Double-buffered pipeline: voices write to one buffer while effects process another
//
// ARCHITECTURE:
//   Core 1 (audio thread):  Render voices → Copy to effects → Return prev buffer
//   Core 2 (effects thread): Filter → Reverb → Compressor (runs in parallel)
//   Latency: 1 buffer (~5ms @ 256 samples) for effects only, voices are real-time
//
// DOES NOT CONTAIN:
// - Voice management (see synth_voices.cpp)
// - Parameter updates (see synth_parameters.cpp)
// - DSP components (see voice.cpp, filters.hpp, reverb.cpp, etc.)
//
// PERFORMANCE:
// - Zero barrier synchronization during voice rendering (was causing 6.3x slowdown)
// - Sequential voices with global FM: ~500 μs per buffer with 2 voices
// - Parallel effects: overlapped with next buffer's voice rendering
//
// ============================================================================

#include "../synth.h"
#include "../clock.h"
#include "../ui.h"
#include "../fast_math.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>

// ============================================================================
// MAIN AUDIO PROCESS - Pipeline Entry Point
// ============================================================================

void Synth::process(float* output, unsigned int nFrames, unsigned int nChannels, float tempo) {
    // === PIPELINE ARCHITECTURE ===
    // Core 1: Render voices sequentially (this thread)
    // Core 2: Process effects in parallel (effectsThread)

    // Initialize buffers on first call
    size_t bufferSize = static_cast<size_t>(nFrames) * nChannels;
    if (voiceRenderBuffer.size() != bufferSize) {
        voiceRenderBuffer.resize(bufferSize);
        effectsBuffer[0].resize(bufferSize);
        effectsBuffer[1].resize(bufferSize);

        // Initialize with silence to avoid glitches on first buffer
        std::fill(effectsBuffer[0].begin(), effectsBuffer[0].end(), 0.0f);
        std::fill(effectsBuffer[1].begin(), effectsBuffer[1].end(), 0.0f);
    }

    // Step 1: Render voices to temp buffer (single-threaded, sequential)
    renderVoicesSequential(voiceRenderBuffer.data(), nFrames, nChannels, tempo);

    // Step 2: Copy rendered voices to effects pipeline write buffer
    int writeIdx = effectsWriteIndex.load();
    std::copy(voiceRenderBuffer.begin(), voiceRenderBuffer.end(), effectsBuffer[writeIdx].begin());

    // Step 3: Signal effects thread and swap buffers
    {
        std::lock_guard<std::mutex> lock(effectsMutex);
        effectsReadIndex = writeIdx;       // Effects thread will read from this buffer
        effectsWriteIndex = 1 - writeIdx;  // Next time we write to the other buffer
        effectsBufferReady = true;
    }
    effectsCV.notify_one();

    // Step 4: Copy previous buffer's PROCESSED output to caller
    // (This introduces 1-buffer latency for effects, but voices are real-time)
    int readIdx = 1 - writeIdx;  // The buffer that effects thread just finished
    std::copy(effectsBuffer[readIdx].begin(), effectsBuffer[readIdx].end(), output);
}

// ============================================================================
// SEQUENTIAL VOICE RENDERING - All voices on single thread
// ============================================================================

void Synth::renderVoicesSequential(float* outputBuffer, unsigned int nFrames, unsigned int nChannels, float tempo) {
    // Clear output buffer
    for (unsigned int i = 0; i < nFrames * nChannels; ++i) {
        outputBuffer[i] = 0.0f;
    }

    // Store tempo for voice access
    currentTempo = tempo;

    // Ensure sampler note sources map to correct key/free modes
    if (params) {
        for (int s = 0; s < SAMPLERS_PER_VOICE; ++s) {
            bool shouldKeyMode = params->getSamplerNoteSource(s) != static_cast<int>(NoteSource::FREE);
            if (samplerKeyModes[s] != shouldKeyMode) {
                setSamplerKeyMode(s, shouldKeyMode);
            }
            if (shouldKeyMode) {
                killFreeRunningSamplerVoice(s);
            } else {
                spawnFreeRunningSamplerVoice(s);
            }
        }
    }

    // Process modulation matrix
    ModulationOutputs globalModOutputs = processModulationMatrix();
    lastGlobalModOutputs = globalModOutputs;
    refreshSamplerPhaseDrivers();
    float masterGain = std::max(0.0f, masterVolume + lastGlobalModOutputs.mixerMasterVolume);  // Allow boost up to +12dB (4.0x)

    constexpr float kVoiceHeadroomGain = 0.35f;
    float voiceGain = kVoiceHeadroomGain;

    // Prepare FM routing structures
    std::vector<float> perFrameOscFm;
    std::vector<float> perFrameSamplerFm;

    const float baseFmDepth = params ? params->fmGlobalDepth.load() : 0.0f;
    float fmGlobalDepth = std::clamp(baseFmDepth + lastGlobalModOutputs.fmGlobalDepth, 0.0f, 1.0f);
    float resolvedFmDepth[kFMTargetCount][kFMSourceCount];
    bool oscTargetHasFm[OSCILLATORS_PER_VOICE] = {false};
    bool samplerTargetHasFm[SAMPLERS_PER_VOICE] = {false};
    bool oscSourceHasFm[OSCILLATORS_PER_VOICE] = {false};
    bool samplerSourceHasFm[SAMPLERS_PER_VOICE] = {false};

    // Resolve FM depths with modulation
    for (int target = 0; target < kFMTargetCount; ++target) {
        for (int source = 0; source < kFMSourceCount; ++source) {
            float baseDepth = params ? params->getFMDepth(target, source) : 0.0f;
            float depth = baseDepth + lastGlobalModOutputs.fmDepth[target][source];
            resolvedFmDepth[target][source] = std::clamp(depth, -0.99f, 0.99f);
            if (resolvedFmDepth[target][source] != 0.0f) {
                if (target < OSCILLATORS_PER_VOICE) {
                    oscTargetHasFm[target] = true;
                } else {
                    samplerTargetHasFm[target - OSCILLATORS_PER_VOICE] = true;
                }
                if (source < OSCILLATORS_PER_VOICE) {
                    oscSourceHasFm[source] = true;
                } else {
                    samplerSourceHasFm[source - OSCILLATORS_PER_VOICE] = true;
                }
            }
        }
    }

    bool fmRoutingActive = fmGlobalDepth > 0.0f;
    if (fmRoutingActive) {
        fmRoutingActive = false;
        for (bool v : oscTargetHasFm) { fmRoutingActive = fmRoutingActive || v; }
        for (bool v : samplerTargetHasFm) { fmRoutingActive = fmRoutingActive || v; }
    }

    if (fmRoutingActive) {
        perFrameOscFm.assign(static_cast<size_t>(nFrames) * OSCILLATORS_PER_VOICE, 0.0f);
        perFrameSamplerFm.assign(static_cast<size_t>(nFrames) * SAMPLERS_PER_VOICE, 0.0f);
    }

    // Set modulation for active voices
    for (int v = 0; v < MAX_VOICES; ++v) {
        Voice& voice = voices[v];
        if (!voice.active) {
            voice.clearGlobalFmInputs();
            continue;
        }

        voice.currentBufferSize = nFrames;
        ModulationOutputs modOutputs = processModulationMatrix(&voice);

        // Store pitch modulation (octave offset is applied in setOscillatorState)
        voice.pitchMod[0] = modOutputs.osc1Pitch;
        voice.pitchMod[1] = modOutputs.osc2Pitch;
        voice.pitchMod[2] = modOutputs.osc3Pitch;
        voice.pitchMod[3] = modOutputs.osc4Pitch;

        voice.morphMod[0] = modOutputs.osc1Morph;
        voice.morphMod[1] = modOutputs.osc2Morph;
        voice.morphMod[2] = modOutputs.osc3Morph;
        voice.morphMod[3] = modOutputs.osc4Morph;

        voice.ratioMod[0] = modOutputs.osc1Ratio;
        voice.ratioMod[1] = modOutputs.osc2Ratio;
        voice.ratioMod[2] = modOutputs.osc3Ratio;
        voice.ratioMod[3] = modOutputs.osc4Ratio;

        voice.offsetMod[0] = modOutputs.osc1Offset;
        voice.offsetMod[1] = modOutputs.osc2Offset;
        voice.offsetMod[2] = modOutputs.osc3Offset;
        voice.offsetMod[3] = modOutputs.osc4Offset;

        voice.ampMod[0] = modOutputs.osc1Amp;
        voice.ampMod[1] = modOutputs.osc2Amp;
        voice.ampMod[2] = modOutputs.osc3Amp;
        voice.ampMod[3] = modOutputs.osc4Amp;

        voice.samplerPitchMod[0] = modOutputs.samp1Pitch;
        voice.samplerPitchMod[1] = modOutputs.samp2Pitch;
        voice.samplerPitchMod[2] = modOutputs.samp3Pitch;
        voice.samplerPitchMod[3] = modOutputs.samp4Pitch;

        voice.samplerLoopStartMod[0] = modOutputs.samp1LoopStart;
        voice.samplerLoopStartMod[1] = modOutputs.samp2LoopStart;
        voice.samplerLoopStartMod[2] = modOutputs.samp3LoopStart;
        voice.samplerLoopStartMod[3] = modOutputs.samp4LoopStart;

        voice.samplerLoopLengthMod[0] = modOutputs.samp1LoopLength;
        voice.samplerLoopLengthMod[1] = modOutputs.samp2LoopLength;
        voice.samplerLoopLengthMod[2] = modOutputs.samp3LoopLength;
        voice.samplerLoopLengthMod[3] = modOutputs.samp4LoopLength;

        voice.samplerCrossfadeMod[0] = modOutputs.samp1Crossfade;
        voice.samplerCrossfadeMod[1] = modOutputs.samp2Crossfade;
        voice.samplerCrossfadeMod[2] = modOutputs.samp3Crossfade;
        voice.samplerCrossfadeMod[3] = modOutputs.samp4Crossfade;

        voice.samplerLevelMod[0] = modOutputs.samp1Amp;
        voice.samplerLevelMod[1] = modOutputs.samp2Amp;
        voice.samplerLevelMod[2] = modOutputs.samp3Amp;
        voice.samplerLevelMod[3] = modOutputs.samp4Amp;

        // Calculate linear interpolation increments for critical modulation
        // This eliminates block-rate zipper noise
        float blockSize = static_cast<float>(std::max(1u, nFrames));
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            voice.pitchModIncrement[i] = (voice.pitchMod[i] - voice.smoothedPitchMod[i]) / blockSize;
            voice.ampModIncrement[i] = (voice.ampMod[i] - voice.smoothedAmpMod[i]) / blockSize;
            voice.oscLevelIncrement[i] = (voice.cachedOscLevel[i] - voice.smoothedOscLevel[i]) / blockSize;
        }
        for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
            voice.samplerPitchModIncrement[i] = (voice.samplerPitchMod[i] - voice.smoothedSamplerPitchMod[i]) / blockSize;
            voice.samplerLevelModIncrement[i] = (voice.samplerLevelMod[i] - voice.smoothedSamplerLevelMod[i]) / blockSize;
        }

        for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
            if (samplerPhaseSource[i] != kClockModSourceIndex) {
                voice.samplerPhaseDriver[i] = normalizePhaseForDriver(modOutputs.samplerPhase[i], samplerPhaseType[i]);
            } else {
                voice.samplerPhaseDriver[i] = -1.0f;
            }
        }

        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            float baseLevel = oscillatorBaseLevels[i];
            float offset = modOutputs.mixerOscLevel[i];
            voice.cachedOscLevel[i] = std::clamp(baseLevel + offset, 0.0f, 1.0f);
        }
        for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
            voice.cachedSamplerLevelMod[i] = modOutputs.mixerSamplerLevel[i];
        }
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            voice.oscAmpControllerActive[i] = modOutputs.oscAmpControllerActive[i];
            voice.oscAmpControllerValue[i] = std::clamp(modOutputs.oscAmpController[i], 0.0f, 1.0f);
        }
        for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
            voice.samplerAmpControllerActive[i] = modOutputs.samplerAmpControllerActive[i];
            voice.samplerAmpControllerValue[i] = std::clamp(modOutputs.samplerAmpController[i], 0.0f, 1.0f);
        }

        const float* oscFmPtr = fmRoutingActive ? perFrameOscFm.data() : nullptr;
        const float* samplerFmPtr = fmRoutingActive ? perFrameSamplerFm.data() : nullptr;
        voice.setGlobalFmInputs(oscFmPtr, samplerFmPtr);
    }

    // Collect active voices
    std::vector<int> activeVoices;
    for (int v = 0; v < MAX_VOICES; ++v) {
        if (voices[v].active) activeVoices.push_back(v);
    }

    // FM computation lambda
    auto computeFmFromSources = [&](const float* oscSources,
                                    const float* samplerSources,
                                    float* oscTargets,
                                    float* samplerTargets) {
        if (!oscTargets || !samplerTargets || fmGlobalDepth <= 0.0f) {
            return;
        }
        for (int target = 0; target < OSCILLATORS_PER_VOICE; ++target) {
            if (!oscTargetHasFm[target]) {
                oscTargets[target] = 0.0f;
                continue;
            }
            float totalFM = 0.0f;
            for (int source = 0; source < OSCILLATORS_PER_VOICE; ++source) {
                if (!oscSourceHasFm[source]) continue;
                float depth = resolvedFmDepth[target][source];
                if (depth != 0.0f) {
                    totalFM += oscSources[source] * (depth * 100.0f);
                }
            }
            for (int source = 0; source < SAMPLERS_PER_VOICE; ++source) {
                if (!samplerSourceHasFm[source]) continue;
                int sourceIdx = kFMOscillatorTargetCount + source;
                float depth = resolvedFmDepth[target][sourceIdx];
                if (depth != 0.0f) {
                    totalFM += samplerSources[source] * (depth * 100.0f);
                }
            }
            oscTargets[target] = totalFM * fmGlobalDepth;
        }

        for (int target = 0; target < SAMPLERS_PER_VOICE; ++target) {
            if (!samplerTargetHasFm[target]) {
                samplerTargets[target] = 0.0f;
                continue;
            }
            int targetIdx = kFMOscillatorTargetCount + target;
            float totalFM = 0.0f;
            for (int source = 0; source < OSCILLATORS_PER_VOICE; ++source) {
                if (!oscSourceHasFm[source]) continue;
                float depth = resolvedFmDepth[targetIdx][source];
                if (depth != 0.0f) {
                    totalFM += oscSources[source] * (depth * 100.0f);
                }
            }
            for (int source = 0; source < SAMPLERS_PER_VOICE; ++source) {
                if (!samplerSourceHasFm[source]) continue;
                int sourceIdx = kFMOscillatorTargetCount + source;
                float depth = resolvedFmDepth[targetIdx][sourceIdx];
                if (depth != 0.0f) {
                    totalFM += samplerSources[source] * (depth * 100.0f);
                }
            }
            samplerTargets[target] = totalFM * fmGlobalDepth;
        }
    };

    // Initialize FM with previous buffer's outputs
    std::array<float, OSCILLATORS_PER_VOICE> accumOsc{};
    std::array<float, SAMPLERS_PER_VOICE> accumSampler{};
    std::copy(std::begin(globalOscOutputsPrev), std::end(globalOscOutputsPrev), accumOsc.begin());
    std::copy(std::begin(globalSamplerOutputsPrev), std::end(globalSamplerOutputsPrev), accumSampler.begin());

    if (fmRoutingActive) {
        computeFmFromSources(accumOsc.data(),
                             accumSampler.data(),
                             perFrameOscFm.data(),
                             perFrameSamplerFm.data());
    }

    // ===  SEQUENTIAL VOICE RENDERING (Single-threaded, no barriers!) ===
    for (unsigned int frame = 0; frame < nFrames; ++frame) {
        accumOsc.fill(0.0f);
        accumSampler.fill(0.0f);

        // Process all active voices for this sample
        for (int v : activeVoices) {
            Voice& voice = voices[v];
            float sample = voice.generateSample(frame);

            // Mix to stereo output
            for (unsigned int ch = 0; ch < nChannels; ++ch) {
                outputBuffer[static_cast<size_t>(frame) * nChannels + ch] += sample * voiceGain * masterGain;
            }

            // Accumulate FM sources
            const float* osc = voice.getLastOscOutputs();
            for (int o = 0; o < OSCILLATORS_PER_VOICE; ++o) {
                accumOsc[o] += osc[o];
            }
            const float* samp = voice.getLastSamplerOutputs();
            for (int s = 0; s < SAMPLERS_PER_VOICE; ++s) {
                accumSampler[s] += samp[s];
            }
        }

        // Compute FM for next frame
        if (frame + 1 < nFrames) {
            if (fmRoutingActive) {
                computeFmFromSources(
                    accumOsc.data(),
                    accumSampler.data(),
                    perFrameOscFm.data() + static_cast<size_t>(frame + 1) * OSCILLATORS_PER_VOICE,
                    perFrameSamplerFm.data() + static_cast<size_t>(frame + 1) * SAMPLERS_PER_VOICE);
            }
        } else {
            // Save last frame's output for next buffer
            std::copy(accumOsc.begin(), accumOsc.end(), std::begin(globalOscOutputsPrev));
            std::copy(accumSampler.begin(), accumSampler.end(), std::begin(globalSamplerOutputsPrev));
        }
    }

    // Mix chaos generators directly to output
    if (nChannels >= 2 && params) {
        bool anyChaosSolo = false;
        for (int c = 0; c < 4; ++c) {
            if (params->chaosSolo[c].load()) { anyChaosSolo = true; break; }
        }
        for (unsigned int i = 0; i < nFrames; ++i) {
            float chaosL = 0.0f;
            float chaosR = 0.0f;
            for (int c = 0; c < 4; ++c) {
                bool muted = params->chaosMuted[c].load();
                bool solo = params->chaosSolo[c].load();
                if (anyChaosSolo) {
                    if (!solo) continue;
                } else if (muted) {
                    continue;
                }
                float level = params->getChaosLevel(c);
                float modulatedLevel = std::clamp(level + lastGlobalModOutputs.chaosLevel[c], 0.0f, 1.0f);
                float x = (i < chaosBufferX[c].size()) ? chaosBufferX[c][i] : chaosOutputs[c];
                float y = (i < chaosBufferY[c].size()) ? chaosBufferY[c][i] : chaos[c].getY();
                if (chaosDiffMode) {
                    float prevX = (i > 0 && i-1 < chaosBufferX[c].size()) ? chaosBufferX[c][i-1] : chaosLastX[c];
                    float prevY = (i > 0 && i-1 < chaosBufferY[c].size()) ? chaosBufferY[c][i-1] : chaosLastY[c];
                    x -= prevX;
                    y -= prevY;
                }
                chaosL += x * modulatedLevel;
                chaosR += y * modulatedLevel;
            }
            if (chaosL != 0.0f || chaosR != 0.0f) {
                outputBuffer[i * nChannels + 0] += chaosL * voiceGain * masterGain;
                if (nChannels > 1) {
                    outputBuffer[i * nChannels + 1] += chaosR * voiceGain * masterGain;
                }
            }
        }
        if (chaosDiffMode) {
            for (int c = 0; c < 4; ++c) {
                if (!chaosBufferX[c].empty()) chaosLastX[c] = chaosBufferX[c].back();
                if (!chaosBufferY[c].empty()) chaosLastY[c] = chaosBufferY[c].back();
            }
        }
    }

    // Apply soft clipping if compressor is disabled
    if (!compressorEnabled) {
        for (unsigned int i = 0; i < nFrames * nChannels; ++i) {
            if (!std::isfinite(outputBuffer[i])) {
                outputBuffer[i] = 0.0f;
            }
            outputBuffer[i] = softClip(outputBuffer[i], 0.7f);
        }
    } else {
        // Still sanitize
        for (unsigned int i = 0; i < nFrames * nChannels; ++i) {
            if (!std::isfinite(outputBuffer[i])) {
                outputBuffer[i] = 0.0f;
            }
        }
    }

    // Write UI waveform
    if (ui && !activeVoices.empty()) {
        for (unsigned int i = 0; i < nFrames; ++i) {
            float displaySample = outputBuffer[static_cast<size_t>(i) * nChannels];
            ui->writeToWaveformBuffer(displaySample);
        }
    }
}

// ============================================================================
// EFFECTS THREAD - Parallel Processing on Separate Core
// ============================================================================

void Synth::effectsThreadFunc() {
    while (effectsThreadRunning) {
        // Wait for a new buffer to be ready
        {
            std::unique_lock<std::mutex> lock(effectsMutex);
            effectsCV.wait(lock, [this]() {
                return !effectsThreadRunning || effectsBufferReady.load();
            });

            // Exit if shutting down
            if (!effectsThreadRunning) {
                return;
            }

            // Mark buffer as consumed
            effectsBufferReady = false;
        }

        // Get the current read buffer index
        int readIdx = effectsReadIndex.load();
        float* buffer = effectsBuffer[readIdx].data();
        unsigned int nFrames = effectsBuffer[readIdx].size() / 2;  // Stereo
        unsigned int nChannels = 2;

        // Apply filter if enabled (stereo processing)
        if (filterEnabled && nChannels == 2) {
            // Apply filter modulation and smoothing
            if (params) {
                float baseCutoff = params->filterCutoff.load();
                float baseResonance = params->filterResonance.load();
                float baseDrive = params->filterDrive.load();
                float baseWidth = params->filterBandWidth.load();
                float baseNotchFeedback = params->filterNotchFeedback.load();
                float baseSpread = params->filterSpread.load();
                float baseDryWet = params->filterDryWet.load();

                // Cutoff: multiplicative modulation in octaves
                float cutoffOctaves = lastGlobalModOutputs.filterCutoff * 4.0f;
                float targetCutoff = std::clamp(baseCutoff * std::pow(2.0f, cutoffOctaves), 20.0f, 20000.0f);

                float targetResonance = std::clamp(baseResonance + lastGlobalModOutputs.filterResonance * 0.6f, 0.0f, 1.2f);
                float targetDrive = std::clamp(baseDrive + lastGlobalModOutputs.filterDrive * 7.0f, 0.1f, 15.0f);
                float targetWidth = std::clamp(baseWidth + lastGlobalModOutputs.filterWidth, 0.0f, 1.0f);
                float targetSpread = std::clamp(baseSpread + lastGlobalModOutputs.filterSpread, 0.0f, 1.0f);
                float targetDryWet = std::clamp(baseDryWet + lastGlobalModOutputs.filterDryWet, 0.0f, 1.0f);
                float targetNotchFeedback = std::clamp(baseNotchFeedback + lastGlobalModOutputs.filterNotchFeedback * 0.5f, 0.0f, 0.98f);

                // Apply smoothing
                constexpr float alpha = 0.05f;
                smoothedFilterCutoff += alpha * (targetCutoff - smoothedFilterCutoff);
                smoothedFilterResonance += alpha * (targetResonance - smoothedFilterResonance);
                smoothedFilterDrive += alpha * (targetDrive - smoothedFilterDrive);
                smoothedFilterWidth += alpha * (targetWidth - smoothedFilterWidth);
                smoothedFilterSpread += alpha * (targetSpread - smoothedFilterSpread);
                smoothedFilterDryWet += alpha * (targetDryWet - smoothedFilterDryWet);
                smoothedFilterNotchFeedback += alpha * (targetNotchFeedback - smoothedFilterNotchFeedback);

                // Apply modulated parameters to all filter types
                filterL.setCutoff(smoothedFilterCutoff);
                filterR.setCutoff(smoothedFilterCutoff);
                ladderFilterL.setCutoff(smoothedFilterCutoff);
                ladderFilterR.setCutoff(smoothedFilterCutoff);
                ladderFilterL.setResonance(smoothedFilterResonance);
                ladderFilterR.setResonance(smoothedFilterResonance);
                ladderFilterL.setDrive(smoothedFilterDrive);
                ladderFilterR.setDrive(smoothedFilterDrive);
                diodeFilterL.setCutoff(smoothedFilterCutoff);
                diodeFilterR.setCutoff(smoothedFilterCutoff);
                diodeFilterL.setResonance(smoothedFilterResonance);
                diodeFilterR.setResonance(smoothedFilterResonance);
                diodeFilterL.setDrive(smoothedFilterDrive);
                diodeFilterR.setDrive(smoothedFilterDrive);
                bandpassFilterL.setCutoff(smoothedFilterCutoff);
                bandpassFilterR.setCutoff(smoothedFilterCutoff);
                bandpassFilterL.setResonance(smoothedFilterResonance);
                bandpassFilterR.setResonance(smoothedFilterResonance);
                bandpassFilterL.setDrive(smoothedFilterDrive);
                bandpassFilterR.setDrive(smoothedFilterDrive);
                bandpassFilterL.setWidth(smoothedFilterWidth);
                bandpassFilterR.setWidth(smoothedFilterWidth);
                bandpass8FilterL.setCutoff(smoothedFilterCutoff);
                bandpass8FilterR.setCutoff(smoothedFilterCutoff);
                bandpass8FilterL.setResonance(smoothedFilterResonance);
                bandpass8FilterR.setResonance(smoothedFilterResonance);
                bandpass8FilterL.setDrive(smoothedFilterDrive);
                bandpass8FilterR.setDrive(smoothedFilterDrive);
                bandpass8FilterL.setWidth(smoothedFilterWidth);
                bandpass8FilterR.setWidth(smoothedFilterWidth);
                bandpass2FilterL.setCutoff(smoothedFilterCutoff);
                bandpass2FilterR.setCutoff(smoothedFilterCutoff);
                bandpass2FilterL.setResonance(smoothedFilterResonance);
                bandpass2FilterR.setResonance(smoothedFilterResonance);
                bandpass2FilterL.setDrive(smoothedFilterDrive);
                bandpass2FilterR.setDrive(smoothedFilterDrive);
                bandpass2FilterL.setWidth(smoothedFilterWidth);
                bandpass2FilterR.setWidth(smoothedFilterWidth);
                notchFilterL.setCutoff(smoothedFilterCutoff);
                notchFilterR.setCutoff(smoothedFilterCutoff);
                notchFilterL.setResonance(smoothedFilterResonance);
                notchFilterR.setResonance(smoothedFilterResonance);
                notchFilterL.setNotchFeedback(smoothedFilterNotchFeedback);
                notchFilterR.setNotchFeedback(smoothedFilterNotchFeedback);
                notchFilterL.setSpread(smoothedFilterSpread);
                notchFilterR.setSpread(smoothedFilterSpread);
                notchFilterL.setDryWet(smoothedFilterDryWet);
                notchFilterR.setDryWet(smoothedFilterDryWet);
            }

            for (unsigned int i = 0; i < nFrames; ++i) {
                float left = buffer[i * 2];
                float right = buffer[i * 2 + 1];

                // Apply selected filter type
                if (currentFilterType == 0) {  // Lowpass
                    auto [lp_l, hp_l] = filterL.process(left);
                    auto [lp_r, hp_r] = filterR.process(right);
                    buffer[i * 2] = lp_l;
                    buffer[i * 2 + 1] = lp_r;
                } else if (currentFilterType == 1) {  // Highpass
                    auto [lp_l, hp_l] = filterL.process(left);
                    auto [lp_r, hp_r] = filterR.process(right);
                    buffer[i * 2] = hp_l;
                    buffer[i * 2 + 1] = hp_r;
                } else if (currentFilterType == 2) {  // High shelf
                    buffer[i * 2] = highShelfL.process(left);
                    buffer[i * 2 + 1] = highShelfR.process(right);
                } else if (currentFilterType == 3) {  // Low shelf
                    buffer[i * 2] = lowShelfL.process(left);
                    buffer[i * 2 + 1] = lowShelfR.process(right);
                } else if (currentFilterType == 4) {  // Ladder LP (8-pole)
                    buffer[i * 2] = ladderFilterL.process(left);
                    buffer[i * 2 + 1] = ladderFilterR.process(right);
                } else if (currentFilterType == 5) {  // Diode ladder LP
                    buffer[i * 2] = diodeFilterL.process(left);
                    buffer[i * 2 + 1] = diodeFilterR.process(right);
                } else if (currentFilterType == 6) {  // Bandpass ladder
                    buffer[i * 2] = bandpassFilterL.process(left);
                    buffer[i * 2 + 1] = bandpassFilterR.process(right);
                } else if (currentFilterType == 7) {  // Dual Notch
                    buffer[i * 2] = notchFilterL.process(left);
                    buffer[i * 2 + 1] = notchFilterR.process(right);
                } else if (currentFilterType == 8) {  // 2-pole Bandpass
                    buffer[i * 2] = bandpass2FilterL.process(left);
                    buffer[i * 2 + 1] = bandpass2FilterR.process(right);
                } else if (currentFilterType == 9) {  // 8-pole Bandpass ladder
                    buffer[i * 2] = bandpass8FilterL.process(left);
                    buffer[i * 2 + 1] = bandpass8FilterR.process(right);
                }
            }
        }

        // Apply reverb if enabled (stereo processing)
        if (reverbEnabled && nChannels == 2) {
            // Apply modulation to reverb parameters
            // The base parameters are set via updateReverbParameters(), modulation is applied on top
            if (params) {
                float baseMix = params->reverbMix.load();
                float baseSize = params->reverbSize.load();
                float baseDelayTime = params->reverbDelayTime.load();
                float baseDamping = params->reverbDamping.load();
                float baseDecay = params->reverbDecay.load();
                float baseDiffusion = params->reverbDiffusion.load();
                float baseModDepth = params->reverbModDepth.load();
                float baseModFreq = params->reverbModFreq.load();

                // Apply modulation (clamped to valid ranges)
                reverb.setMix(std::clamp(baseMix + lastGlobalModOutputs.reverbMix, 0.0f, 1.0f));
                reverb.setSize(std::clamp(baseSize + lastGlobalModOutputs.reverbSize, 0.0f, 1.0f));
                reverb.setDelayTime(std::clamp(baseDelayTime + lastGlobalModOutputs.reverbDelayTime, 0.0f, 1.0f));
                reverb.setDamping(std::clamp(baseDamping + lastGlobalModOutputs.reverbDamping, 0.0f, 1.0f));
                reverb.setDecay(std::clamp(baseDecay + lastGlobalModOutputs.reverbDecay, 0.0f, 1.0f));
                reverb.setDiffusion(std::clamp(baseDiffusion + lastGlobalModOutputs.reverbDiffusion, 0.0f, 1.0f));
                reverb.setModDepth(std::clamp(baseModDepth + lastGlobalModOutputs.reverbModDepth, 0.0f, 1.0f));
                reverb.setModFreq(std::clamp(baseModFreq + lastGlobalModOutputs.reverbModFreq, 0.0f, 10.0f));
            }

            std::vector<float> leftChannel(nFrames);
            std::vector<float> rightChannel(nFrames);

            // De-interleave
            for (unsigned int i = 0; i < nFrames; ++i) {
                leftChannel[i] = buffer[i * 2];
                rightChannel[i] = buffer[i * 2 + 1];
            }

            // Process reverb
            reverb.process(leftChannel.data(), rightChannel.data(), nFrames);

            // Re-interleave
            for (unsigned int i = 0; i < nFrames; ++i) {
                buffer[i * 2] = leftChannel[i];
                buffer[i * 2 + 1] = rightChannel[i];
            }
        }

        // Apply compressor if enabled (stereo processing, in-place)
        if (compressorEnabled && nChannels == 2) {
            compressor.process(buffer, buffer, nFrames * nChannels);
        }
    }
}
