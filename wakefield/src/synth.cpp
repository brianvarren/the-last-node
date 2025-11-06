#include "synth.h"
#include "clock.h"
#include "ui.h"
#include "fast_math.h"
#include <algorithm>
#include <iostream>
#include <thread>

Synth::Synth(float sampleRate)
    : sampleRate(sampleRate)
    , masterVolume(0.7f)  // Increased from 0.5 due to conservative normalization
    , reverbEnabled(false)
    , filterEnabled(false)
    , compressorEnabled(true)
    , currentFilterType(0)
    , ui(nullptr)
    , params(nullptr)
    , clock(nullptr)
    , reverb(sampleRate)
    , compressor(sampleRate)
    , filterL(sampleRate)
    , filterR(sampleRate) {
    
    // Initialize shelf filters
    highShelfL.setSampleRate(sampleRate);
    highShelfR.setSampleRate(sampleRate);
    lowShelfL.setSampleRate(sampleRate);
    lowShelfR.setSampleRate(sampleRate);

    ladderFilterL.setSampleRate(sampleRate);
    ladderFilterR.setSampleRate(sampleRate);
    diodeFilterL.setSampleRate(sampleRate);
    diodeFilterR.setSampleRate(sampleRate);
    bandpassFilterL.setSampleRate(sampleRate);
    bandpassFilterR.setSampleRate(sampleRate);
    bandpass8FilterL.setSampleRate(sampleRate);
    bandpass8FilterR.setSampleRate(sampleRate);
    bandpass2FilterL.setSampleRate(sampleRate);
    bandpass2FilterR.setSampleRate(sampleRate);
    notchFilterL.setSampleRate(sampleRate);
    notchFilterR.setSampleRate(sampleRate);

    // Initialize fast math lookup tables
    FastMath::initSinTable();

    // Initialize voices with sample rate
    for (int i = 0; i < MAX_VOICES; ++i) {
        voices.emplace_back(sampleRate);
    }

    // Phase 5: Initialize sampler levels to 0.6 (reduced from 0.8 for better headroom)
    for (auto& voice : voices) {
        for (int s = 0; s < SAMPLERS_PER_VOICE; ++s) {
            voice.samplers[s].setLevel(0.6f);
        }
    }
    for (int s = 0; s < SAMPLERS_PER_VOICE; ++s) {
        freeSamplers[s].setLevel(0.6f);
    }

    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        freeSamplers[i].setKeyMode(false);
    }

    // Initialize chaos generators with sample rate
    for (int i = 0; i < 4; ++i) {
        chaos[i].setSampleRate(sampleRate);
    }

    compressor.setAutoMakeup(false);
    compressor.setManualMakeup(12.0f);
}

float Synth::midiNoteToFrequency(int midiNote) {
    // MIDI note 69 = A4 = 440 Hz
    // Formula: f = 440 * 2^((n-69)/12)
    return 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
}

int Synth::findFreeVoice() {
    // Priority 1: Find truly inactive voice (OFF stage)
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!voices[i].active) {
            return i;
        }
    }

    // All voices are active - need to steal one
    // Priority 2: Steal voice already in RELEASE stage (fading out)
    int releaseVoiceIndex = -1;
    uint64_t oldestReleaseTime = UINT64_MAX;

    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].envelopes[0].getStage() == EnvelopeStage::RELEASE) {
            // Prefer the oldest voice in release
            if (voices[i].startTime < oldestReleaseTime) {
                oldestReleaseTime = voices[i].startTime;
                releaseVoiceIndex = i;
            }
        }
    }

    if (releaseVoiceIndex != -1) {
        return releaseVoiceIndex;
    }

    // Priority 3: Steal oldest voice in SUSTAIN stage
    int oldestVoiceIndex = -1;
    uint64_t oldestTime = UINT64_MAX;

    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].envelopes[0].getStage() == EnvelopeStage::SUSTAIN) {
            if (voices[i].startTime < oldestTime) {
                oldestTime = voices[i].startTime;
                oldestVoiceIndex = i;
            }
        }
    }

    if (oldestVoiceIndex != -1) {
        return oldestVoiceIndex;
    }

    // Priority 4: Steal voice with lowest envelope level (quietest)
    // This handles ATTACK and DECAY stages
    int quietestVoiceIndex = 0;
    float lowestLevel = voices[0].getEnvelopeValue();

    for (int i = 1; i < MAX_VOICES; ++i) {
        float level = voices[i].getEnvelopeValue();
        if (level < lowestLevel) {
            lowestLevel = level;
            quietestVoiceIndex = i;
        }
    }

    return quietestVoiceIndex;
}

void Synth::updateEnvelopeParameters(float attack, float decay, float sustain, float release) {
    // Update ENV1 with smoothed parameters and apply bends from params
    for (auto& voice : voices) {
        voice.envelopes[0].setAttack(attack);
        voice.envelopes[0].setDecay(decay);
        voice.envelopes[0].setSustain(sustain);
        voice.envelopes[0].setRelease(release);
        // Bends
        if (params) {
            voice.envelopes[0].setAttackBend(params->getEnvAttackBend(0));
            voice.envelopes[0].setReleaseBend(params->getEnvReleaseBend(0));
        }
    }

    // Update ENV2-ENV4 directly from params (unsmoothed)
    if (params) {
        for (auto& voice : voices) {
            for (int i = 1; i < 4; ++i) {
                voice.envelopes[i].setAttack(params->getEnvAttack(i));
                voice.envelopes[i].setDecay(params->getEnvDecay(i));
                voice.envelopes[i].setSustain(params->getEnvSustain(i));
                voice.envelopes[i].setRelease(params->getEnvRelease(i));
                voice.envelopes[i].setAttackBend(params->getEnvAttackBend(i));
                voice.envelopes[i].setReleaseBend(params->getEnvReleaseBend(i));
            }
        }
    }
}

void Synth::setOscillatorState(int index, BrainwaveMode mode, int shape,
                               float baseFreq, float morph, float duty,
                               float ratio, float offsetHz, float amp, float level) {
    if (index < 0 || index >= OSCILLATORS_PER_VOICE) {
        return;
    }

    // No change detection - always update for real-time control
    // The oscillator setters are simple and cheap to call every frame
    // Map old discrete shape (0=SAW, 1=PULSE) to new continuous shape
    // 0 -> 0.5 (Saw: 0.4-0.6), 1 -> 0.9 (Pulse: 0.8-1.0)
    float shapeFloat = (shape == 0) ? 0.5f : 0.9f;

    for (auto& voice : voices) {
        BrainwaveOscillator& osc = voice.oscillators[index];
        osc.setMode(mode);
        osc.setShape(shapeFloat);
        osc.setFrequency(baseFreq);
        // Morph and duty are deprecated - shape is now the single control
        // Keep setters for backward compatibility but they do nothing
        osc.setMorph(morph);
        osc.setDuty(duty);
        osc.setRatio(ratio);
        osc.setOffset(offsetHz);
    }

    // Store base amp and level at control rate (used in voice mixing)
    oscillatorBaseAmps[index] = amp;
    oscillatorBaseLevels[index] = level;

    // Check if ANY oscillator or sampler is in FREE mode
    bool anyFreeMode = false;

    // Check all oscillators
    for (const auto& voice : voices) {
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            if (voice.oscillators[i].getMode() == BrainwaveMode::FREE) {
                anyFreeMode = true;
                break;
            }
        }
        if (anyFreeMode) break;
    }

    // Note: FREE sampler playback is handled by freeSamplers path directly.
    // Do not spawn a free-running voice based on sampler FREE mode alone.

    // Spawn or kill free-running voice based on mode
    if (anyFreeMode && !freeRunningVoiceActive) {
        spawnFreeRunningVoice();
    } else if (!anyFreeMode && freeRunningVoiceActive) {
        killFreeRunningVoice();
    }
}

void Synth::updateReverbParameters(float delayTime, float size, float damping, float mix, float decay,
                                   float diffusion, float modDepth, float modFreq) {
    reverb.setDelayTime(delayTime);
    reverb.setSize(size);
    reverb.setDamping(damping);
    reverb.setMix(mix);
    reverb.setDecay(decay);
    reverb.setDiffusion(diffusion);
    reverb.setModDepth(modDepth);
    reverb.setModFreq(modFreq);
}

void Synth::updateCompressorParameters(float threshold, float ratio, float attack,
                                       float release, float knee, float mix,
                                       bool autoMakeup, float manualMakeup, bool rmsMode) {
    compressor.setThreshold(threshold);
    compressor.setRatio(ratio);
    compressor.setAttack(attack);
    compressor.setRelease(release);
    compressor.setKnee(knee);
    compressor.setMix(mix);
    compressor.setAutoMakeup(autoMakeup);
    compressor.setManualMakeup(manualMakeup);
    compressor.setDetectionMode(rmsMode);
}

float Synth::getCompressorGainReduction() const {
    return compressor.getGainReduction();
}

void Synth::updateFilterParameters(int type, float cutoff, float gain,
                                   float resonance, float drive, float feedbackHP,
                                   float spread, float notchFeedback, float bandWidth) {
    currentFilterType = type;
    
    // Update all filter types (the active one will be used during processing)
    filterL.setCutoff(cutoff);
    filterR.setCutoff(cutoff);
    
    highShelfL.setCutoff(cutoff);
    highShelfR.setCutoff(cutoff);
    highShelfL.setGainDb(gain);
    highShelfR.setGainDb(gain);
    
    lowShelfL.setCutoff(cutoff);
    lowShelfR.setCutoff(cutoff);
    lowShelfL.setGainDb(gain);
    lowShelfR.setGainDb(gain);

    ladderFilterL.setCutoff(cutoff);
    ladderFilterR.setCutoff(cutoff);
    ladderFilterL.setResonance(resonance);
    ladderFilterR.setResonance(resonance);
    ladderFilterL.setDrive(drive);
    ladderFilterR.setDrive(drive);
    ladderFilterL.setFeedbackHighpass(feedbackHP);
    ladderFilterR.setFeedbackHighpass(feedbackHP);

    diodeFilterL.setCutoff(cutoff);
    diodeFilterR.setCutoff(cutoff);
    diodeFilterL.setResonance(resonance);
    diodeFilterR.setResonance(resonance);
    diodeFilterL.setDrive(drive);
    diodeFilterR.setDrive(drive);
    diodeFilterL.setFeedbackHighpass(feedbackHP);
    diodeFilterR.setFeedbackHighpass(feedbackHP);

    bandpassFilterL.setCutoff(cutoff);
    bandpassFilterR.setCutoff(cutoff);
    bandpassFilterL.setResonance(resonance);
    bandpassFilterR.setResonance(resonance);
    bandpassFilterL.setDrive(drive);
    bandpassFilterR.setDrive(drive);
    bandpassFilterL.setFeedbackHighpass(feedbackHP);
    bandpassFilterR.setFeedbackHighpass(feedbackHP);
    bandpassFilterL.setWidth(bandWidth);
    bandpassFilterR.setWidth(bandWidth);

    bandpass8FilterL.setCutoff(cutoff);
    bandpass8FilterR.setCutoff(cutoff);
    bandpass8FilterL.setResonance(resonance);
    bandpass8FilterR.setResonance(resonance);
    bandpass8FilterL.setDrive(drive);
    bandpass8FilterR.setDrive(drive);
    bandpass8FilterL.setWidth(bandWidth);
    bandpass8FilterR.setWidth(bandWidth);

    bandpass2FilterL.setCutoff(cutoff);
    bandpass2FilterR.setCutoff(cutoff);
    bandpass2FilterL.setResonance(resonance);
    bandpass2FilterR.setResonance(resonance);
    bandpass2FilterL.setDrive(drive);
    bandpass2FilterR.setDrive(drive);
    bandpass2FilterL.setFeedbackHighpass(feedbackHP);
    bandpass2FilterR.setFeedbackHighpass(feedbackHP);
    bandpass2FilterL.setWidth(bandWidth);
    bandpass2FilterR.setWidth(bandWidth);

    notchFilterL.setCutoff(cutoff);
    notchFilterR.setCutoff(cutoff);
    notchFilterL.setSpread(spread);
    notchFilterR.setSpread(spread);
    notchFilterL.setResonance(resonance);
    notchFilterR.setResonance(resonance);
    notchFilterL.setDrive(drive);
    notchFilterR.setDrive(drive);
    notchFilterL.setNotchFeedback(notchFeedback);
    notchFilterR.setNotchFeedback(notchFeedback);
    // Note: DryWet is applied with modulation in processAudio, not here
}

void Synth::noteOn(int midiNote, int velocity) {
    // Find a free voice (or steal one if all are busy)
    int voiceIndex = findFreeVoice();

    // Voice stealing now happens intelligently in findFreeVoice()
    // Priority: OFF stage > RELEASE stage > oldest SUSTAIN > quietest ATTACK/DECAY
    // This ensures smooth transitions by preferring voices already fading out
    Voice& voice = voices[voiceIndex];

    // Detect if we're stealing an active voice (for smooth envelope retriggering)
    bool isStealingVoice = voice.active;

    // Activate the voice with new note
    voice.active = true;
    voice.note = midiNote;
    voice.velocity = velocity;
    voice.startTime = voiceCounter++;  // Assign timestamp for voice stealing priority

    float frequency = midiNoteToFrequency(midiNote);
    // Update note frequency for oscillators in KEY mode and reset phase
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        voice.oscillators[i].setNoteFrequency(frequency);
        voice.oscillators[i].reset();  // Reset phase for new note
        voice.pitchMod[i] = 0.0f;
        voice.morphMod[i] = 0.0f;
        voice.dutyMod[i] = 0.0f;
        voice.ratioMod[i] = 0.0f;
        voice.offsetMod[i] = 0.0f;
        voice.ampMod[i] = 0.0f;
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        voice.samplerPitchMod[i] = 0.0f;
        voice.samplerLoopStartMod[i] = 0.0f;
       voice.samplerLoopLengthMod[i] = 0.0f;
        voice.samplerCrossfadeMod[i] = 0.0f;
        voice.samplerLevelMod[i] = 0.0f;
        voice.samplers[i].setKeyMode(samplerKeyModes[i]);
        if (samplerKeyModes[i]) {
            // Note Reset ON: restart from beginning
            // Note Reset OFF: restore saved phase and continue
            if (samplerNoteResets[i]) {
                voice.samplers[i].requestRestart();
            } else {
                voice.samplers[i].restorePhase(samplerLastPhases[i]);
                voice.samplers[i].requestRestart();  // Still need to ensure voice is active
            }
        } else {
            voice.samplers[i].stopPlayback();
        }
    }
    voice.resetFMHistory();

    // Trigger envelope attack on all envelopes
    // If stealing an active voice, start from current level to avoid discontinuity clicks
    for (int ei = 0; ei < 4; ++ei) {
        voice.envelopes[ei].noteOn(isStealingVoice);
    }
}

void Synth::noteOff(int midiNote) {
    // Find all voices playing this note and trigger their release
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].active && voices[i].note == midiNote) {
            for (int ei = 0; ei < 4; ++ei) {
                voices[i].envelopes[ei].noteOff();  // Trigger release
            }
        }
    }

    fmSourceBufferPrev = fmSourceBuffer;
}

void Synth::spawnFreeRunningVoice() {
    // Kill existing free-running voice first
    if (freeRunningVoiceActive) {
        killFreeRunningVoice();
    }

    // Find a free voice (prefer voice 0 for consistency)
    int voiceIndex = 0;
    if (voices[voiceIndex].active) {
        // If voice 0 is busy, find any free voice
        voiceIndex = findFreeVoice();
        if (voiceIndex == -1) {
            // No free voices available
            return;
        }
    }

    // Activate the free-running voice
    Voice& voice = voices[voiceIndex];
    voice.active = true;
    voice.note = 60;  // Middle C as default note
    voice.velocity = 100;  // Default velocity

    // Set note frequency for oscillators (used in KEY mode, ignored in FREE mode)
    float frequency = midiNoteToFrequency(60);
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        voice.oscillators[i].setNoteFrequency(frequency);
        voice.oscillators[i].reset();  // Reset phase
        voice.pitchMod[i] = 0.0f;
        voice.morphMod[i] = 0.0f;
        voice.dutyMod[i] = 0.0f;
        voice.ratioMod[i] = 0.0f;
        voice.offsetMod[i] = 0.0f;
        voice.ampMod[i] = 0.0f;
    }

    // Initialize samplers
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        voice.samplerPitchMod[i] = 0.0f;
        voice.samplerLoopStartMod[i] = 0.0f;
        voice.samplerLoopLengthMod[i] = 0.0f;
        voice.samplerCrossfadeMod[i] = 0.0f;
        voice.samplerLevelMod[i] = 0.0f;
        voice.samplers[i].setKeyMode(samplerKeyModes[i]);

        // Always restart samplers in FREE mode
        if (!samplerKeyModes[i]) {
            voice.samplers[i].requestRestart();
        }
    }

    voice.resetFMHistory();

    // Trigger envelopes with sustain at max (infinite hold)
    for (int ei = 0; ei < 4; ++ei) {
        voice.envelopes[ei].noteOn();
    }

    // Mark as free-running voice
    freeRunningVoiceActive = true;
    freeRunningVoiceIndex = voiceIndex;
}

void Synth::killFreeRunningVoice() {
    if (!freeRunningVoiceActive || freeRunningVoiceIndex < 0 || freeRunningVoiceIndex >= MAX_VOICES) {
        return;
    }

    Voice& voice = voices[freeRunningVoiceIndex];

    // Immediately deactivate the voice (no release envelope)
    voice.active = false;
    for (int ei = 0; ei < 4; ++ei) {
        voice.envelopes[ei].noteOff();
    }
    voice.resetFMHistory();

    // Stop all samplers
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        voice.samplers[i].stopPlayback();
    }

    freeRunningVoiceActive = false;
    freeRunningVoiceIndex = -1;
}

int Synth::getActiveVoiceCount() const {
    int count = 0;
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].active) {
            count++;
        }
    }
    return count;
}

bool Synth::isVoiceActive(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return false;
    }
    return voices[voiceIndex].active;
}

float Synth::getVoiceEnvelopeValue(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return 0.0f;
    }
    return voices[voiceIndex].getEnvelopeValue();
}

float Synth::getVoiceEnvelopeValue(int voiceIndex, int envIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return 0.0f;
    }
    return voices[voiceIndex].getEnvelopeValue(envIndex);
}

int Synth::getVoiceNote(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) {
        return -1;
    }
    return voices[voiceIndex].note;
}

void Synth::process(float* output, unsigned int nFrames, unsigned int nChannels, float tempo) {
    // Clear the output buffer first
    for (unsigned int i = 0; i < nFrames * nChannels; ++i) {
        output[i] = 0.0f;
    }

    // Store tempo for voice access (used by samplers for tempo sync)
    currentTempo = tempo;

    const size_t frameSourceCount = static_cast<size_t>(nFrames) * kFMSourceCount;
    if (fmSourceBuffer.size() != frameSourceCount) {
        fmSourceBuffer.assign(frameSourceCount, 0.0f);
    } else {
        std::fill(fmSourceBuffer.begin(), fmSourceBuffer.end(), 0.0f);
    }

    // Process modulation matrix once per buffer for global (voice-agnostic) targets
    ModulationOutputs globalModOutputs = processModulationMatrix();
    lastGlobalModOutputs = globalModOutputs;
    refreshSamplerPhaseDrivers();
    float masterGain = std::clamp(masterVolume + lastGlobalModOutputs.mixerMasterVolume, 0.0f, 1.0f);

    // Phase 1: fixed headroom voice gain
    // Choose a conservative mix gain that accommodates worst-case polyphony without clipping.
    constexpr float kVoiceHeadroomGain = 0.35f;  // tuned for 8 voices with internal source normalization
    float voiceGain = kVoiceHeadroomGain;

        // Copy modulation values to active voices (re-evaluated per voice for voice-specific sources)
        for (int v = 0; v < MAX_VOICES; ++v) {
            if (!voices[v].active) {
                continue;
            }

            Voice& voice = voices[v];
            voice.currentBufferSize = nFrames;  // Store for audio-rate interpolation

            ModulationOutputs modOutputs = processModulationMatrix(&voice);

        // Set modulation values for all oscillators (in octaves for pitch)
        // One-pole smoothing handles zipper prevention automatically
        voice.pitchMod[0] = modOutputs.osc1Pitch;
        voice.pitchMod[1] = modOutputs.osc2Pitch;
        voice.pitchMod[2] = modOutputs.osc3Pitch;
        voice.pitchMod[3] = modOutputs.osc4Pitch;

        voice.morphMod[0] = modOutputs.osc1Morph;
        voice.morphMod[1] = modOutputs.osc2Morph;
        voice.morphMod[2] = modOutputs.osc3Morph;
        voice.morphMod[3] = modOutputs.osc4Morph;

        voice.dutyMod[0] = modOutputs.osc1Duty;
        voice.dutyMod[1] = modOutputs.osc2Duty;
        voice.dutyMod[2] = modOutputs.osc3Duty;
        voice.dutyMod[3] = modOutputs.osc4Duty;

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

        // FM global depth modulation
        voice.fmGlobalDepthMod = modOutputs.fmGlobalDepth;

        // Pre-compute final FM depths once per buffer: base + global mod
        // This eliminates 2.6 billion per-sample function calls (getFMDepthMod, getFMDepth)
        // Store previous buffer's values for audio-rate interpolation to prevent zippering
        if (params) {
            for (int target = 0; target < kFMTargetCount; ++target) {
                for (int source = 0; source < kFMSourceCount; ++source) {
                    // Base depth (static parameter)
                    float baseDepth = params->getFMDepth(target, source);
                    // Global modulation (computed per buffer)
                    float globalMod = lastGlobalModOutputs.fmDepth[target][source];
                    // Final depth = base + global modulation, clamped
                    voice.fmDepthMod[target][source] = std::clamp(
                        baseDepth + globalMod, -0.99f, 0.99f);
                }
            }
        } else {
            // No params: zero out FM depths
            for (int target = 0; target < kFMTargetCount; ++target) {
                for (int source = 0; source < kFMSourceCount; ++source) {
                    voice.fmDepthMod[target][source] = 0.0f;
                }
            }
        }

        for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
            if (samplerPhaseSource[i] != kClockModSourceIndex) {
                voice.samplerPhaseDriver[i] = normalizePhaseForDriver(modOutputs.samplerPhase[i], samplerPhaseType[i]);
            } else {
                voice.samplerPhaseDriver[i] = -1.0f;
            }
        }

        // Cache oscillator and sampler levels to avoid per-sample function calls
        // These are computed once per buffer instead of millions of times per sample
        // Previous values already saved above
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            float baseLevel = oscillatorBaseLevels[i];
            float offset = modOutputs.mixerOscLevel[i];
            voice.cachedOscLevel[i] = std::clamp(baseLevel + offset, 0.0f, 1.0f);
        }
        for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
            voice.cachedSamplerLevelMod[i] = modOutputs.mixerSamplerLevel[i];
        }
    }

    // Parallel voice processing: generate into per-voice buffers
    std::vector<int> activeVoices;
    activeVoices.reserve(MAX_VOICES);
    for (int v = 0; v < MAX_VOICES; ++v) {
        if (voices[v].active) activeVoices.push_back(v);
    }

    for (int v : activeVoices) {
        auto &buf = voiceBuffers[v];
        auto &trace = voiceFmTraces[v];
        buf.resize(static_cast<size_t>(nFrames) * nChannels);
        trace.resize(static_cast<size_t>(nFrames) * kFMSourceCount);
        std::fill(buf.begin(), buf.end(), 0.0f);
        std::fill(trace.begin(), trace.end(), 0.0f);
    }

    unsigned int hwc = std::thread::hardware_concurrency();
    if (hwc == 0) hwc = 1;
    unsigned int threadsToUse = std::min<unsigned int>(hwc, std::max<size_t>(1, activeVoices.size()));

    auto worker = [&](size_t beginIdx, size_t endIdx) {
        for (size_t idx = beginIdx; idx < endIdx; ++idx) {
            int v = activeVoices[idx];
            Voice &voice = voices[v];
            float *buf = voiceBuffers[v].data();
            float *trace = voiceFmTraces[v].data();
            for (unsigned int i = 0; i < nFrames; ++i) {
                float sample = voice.generateSample(i);
                // write voice-local buffer (mono to all channels)
                for (unsigned int ch = 0; ch < nChannels; ++ch) {
                    buf[static_cast<size_t>(i) * nChannels + ch] = sample;
                }
                // capture FM sources per frame (oscillators + samplers)
                float *frameTrace = trace + static_cast<size_t>(i) * kFMSourceCount;
                const float *osc = voice.getLastOscOutputs();
                for (int o = 0; o < OSCILLATORS_PER_VOICE; ++o) frameTrace[o] = osc[o];
                const float *samp = voice.getLastSamplerOutputs();
                for (int s = 0; s < SAMPLERS_PER_VOICE; ++s) frameTrace[kFMOscillatorTargetCount + s] = samp[s];
            }
        }
    };

    if (threadsToUse <= 1 || activeVoices.size() <= 1) {
        worker(0, activeVoices.size());
    } else {
        std::vector<std::thread> threads;
        threads.reserve(threadsToUse);
        size_t total = activeVoices.size();
        size_t chunk = (total + threadsToUse - 1) / threadsToUse;
        size_t begin = 0;
        for (unsigned int t = 0; t < threadsToUse && begin < total; ++t) {
            size_t end = std::min(begin + chunk, total);
            threads.emplace_back(worker, begin, end);
            begin = end;
        }
        for (auto &th : threads) th.join();
    }

    // After parallel stage: accumulate FM sources and mix with gains; write UI waveform
    // First, accumulate fmSourceBuffer from per-voice traces
    for (unsigned int i = 0; i < nFrames; ++i) {
        float *frameSources = fmSourceBuffer.data() + static_cast<size_t>(i) * kFMSourceCount;
        for (int v : activeVoices) {
            const float *frameTrace = voiceFmTraces[v].data() + static_cast<size_t>(i) * kFMSourceCount;
            for (int s = 0; s < kFMSourceCount; ++s) {
                frameSources[s] += frameTrace[s];
            }
        }
    }

    // UI waveform from the first active voice (if any)
    if (ui && !activeVoices.empty()) {
        int v0 = activeVoices.front();
        for (unsigned int i = 0; i < nFrames; ++i) {
            const float *frameTrace = voiceFmTraces[v0].data() + static_cast<size_t>(i) * kFMSourceCount;
            float displaySample = 0.0f;
            for (int o = 0; o < OSCILLATORS_PER_VOICE; ++o) {
                if (frameTrace[o] != 0.0f) { displaySample = frameTrace[o]; break; }
            }
            ui->writeToWaveformBuffer(displaySample);
        }
    }

    // Mix per-voice buffers into final output with gains
    for (unsigned int i = 0; i < nFrames; ++i) {
        for (int v : activeVoices) {
            const float *buf = voiceBuffers[v].data();
            for (unsigned int ch = 0; ch < nChannels; ++ch) {
                output[static_cast<size_t>(i) * nChannels + ch] +=
                    buf[static_cast<size_t>(i) * nChannels + ch] * voiceGain * masterGain;
            }
        }
    }
    
    bool anyFreeSamplers = false;
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        if (!samplerKeyModes[i]) {
            anyFreeSamplers = true;
            break;
        }
    }

    if (anyFreeSamplers) {
        int activeSamp = params ? std::clamp(params->activeSamplerCount.load(), 1, SAMPLERS_PER_VOICE) : SAMPLERS_PER_VOICE;
        // Current buffer's modulation values
        float currSamplerPitchMods[SAMPLERS_PER_VOICE] = {
            globalModOutputs.samp1Pitch, globalModOutputs.samp2Pitch,
            globalModOutputs.samp3Pitch, globalModOutputs.samp4Pitch
        };
        float currSamplerLoopStartMods[SAMPLERS_PER_VOICE] = {
            globalModOutputs.samp1LoopStart, globalModOutputs.samp2LoopStart,
            globalModOutputs.samp3LoopStart, globalModOutputs.samp4LoopStart
        };
        float currSamplerLoopLengthMods[SAMPLERS_PER_VOICE] = {
            globalModOutputs.samp1LoopLength, globalModOutputs.samp2LoopLength,
            globalModOutputs.samp3LoopLength, globalModOutputs.samp4LoopLength
        };
        float currSamplerCrossfadeMods[SAMPLERS_PER_VOICE] = {
            globalModOutputs.samp1Crossfade, globalModOutputs.samp2Crossfade,
            globalModOutputs.samp3Crossfade, globalModOutputs.samp4Crossfade
        };
        float currSamplerLevelMods[SAMPLERS_PER_VOICE] = {
            globalModOutputs.samp1Amp, globalModOutputs.samp2Amp,
            globalModOutputs.samp3Amp, globalModOutputs.samp4Amp
        };
        float currSamplerLevelOffsets[SAMPLERS_PER_VOICE] = {
            lastGlobalModOutputs.mixerSamplerLevel[0], lastGlobalModOutputs.mixerSamplerLevel[1],
            lastGlobalModOutputs.mixerSamplerLevel[2], lastGlobalModOutputs.mixerSamplerLevel[3]
        };
        float samplerPhaseDrivers[SAMPLERS_PER_VOICE] = {
            samplerPhaseSource[0] != kClockModSourceIndex ? normalizePhaseForDriver(globalModOutputs.samplerPhase[0], samplerPhaseType[0]) : -1.0f,
            samplerPhaseSource[1] != kClockModSourceIndex ? normalizePhaseForDriver(globalModOutputs.samplerPhase[1], samplerPhaseType[1]) : -1.0f,
            samplerPhaseSource[2] != kClockModSourceIndex ? normalizePhaseForDriver(globalModOutputs.samplerPhase[2], samplerPhaseType[2]) : -1.0f,
            samplerPhaseSource[3] != kClockModSourceIndex ? normalizePhaseForDriver(globalModOutputs.samplerPhase[3], samplerPhaseType[3]) : -1.0f
        };

        // Process free samplers with per-sample interpolation (prevents crackling)
        for (unsigned int i = 0; i < nFrames; ++i) {
            // Calculate interpolation factor for smooth audio-rate transition
            float interpFactor = 0.0f;
            if (nFrames > 1) {
                interpFactor = static_cast<float>(i) / static_cast<float>(nFrames - 1);
                interpFactor = std::clamp(interpFactor, 0.0f, 1.0f);
            }

            float freeMix = 0.0f;
            // Determine if any sampler channel is soloed
            bool anySamplerSolo = false;
            for (int ss = 0; ss < SAMPLERS_PER_VOICE; ++ss) {
                if (params->samplerSolo[ss].load()) { anySamplerSolo = true; break; }
            }
            for (int s = 0; s < SAMPLERS_PER_VOICE; ++s) {
                if (s >= activeSamp) continue;
                if (samplerKeyModes[s]) {
                    continue;
                }

                // Apply mute/solo logic for free samplers
                if (anySamplerSolo) {
                    if (!params->samplerSolo[s].load()) {
                        continue;
                    }
                } else if (params->samplerMuted[s].load()) {
                    continue;
                }

                // Interpolate modulation values per-sample to prevent zippering
                float interpPitchMod = prevFreeSamplerPitchMod[s] +
                    (currSamplerPitchMods[s] - prevFreeSamplerPitchMod[s]) * interpFactor;
                float interpLoopStartMod = prevFreeSamplerLoopStartMod[s] +
                    (currSamplerLoopStartMods[s] - prevFreeSamplerLoopStartMod[s]) * interpFactor;
                float interpLoopLengthMod = prevFreeSamplerLoopLengthMod[s] +
                    (currSamplerLoopLengthMods[s] - prevFreeSamplerLoopLengthMod[s]) * interpFactor;
                float interpCrossfadeMod = prevFreeSamplerCrossfadeMod[s] +
                    (currSamplerCrossfadeMods[s] - prevFreeSamplerCrossfadeMod[s]) * interpFactor;
                float interpLevelMod = prevFreeSamplerLevelMod[s] +
                    (currSamplerLevelMods[s] - prevFreeSamplerLevelMod[s]) * interpFactor;
                float interpLevelOffset = prevFreeSamplerLevelOffset[s] +
                    (currSamplerLevelOffsets[s] - prevFreeSamplerLevelOffset[s]) * interpFactor;

                float samplerOut = freeSamplers[s].process(
                    sampleRate,
                    0.0f,                            // No FM input
                    interpPitchMod,
                    interpLoopStartMod,
                    interpLoopLengthMod,
                    interpCrossfadeMod,
                    interpLevelMod,
                    interpLevelOffset,
                    samplerPhaseDrivers[s],
                    60                                // Reference MIDI note (ignored in FREE mode)
                );
                freeMix += samplerOut;
                float* frameSources = fmSourceBuffer.data() + static_cast<size_t>(i) * kFMSourceCount;
                frameSources[kFMOscillatorTargetCount + s] += samplerOut;
            }

            if (freeMix != 0.0f) {
                for (unsigned int ch = 0; ch < nChannels; ++ch) {
                    output[i * nChannels + ch] += freeMix * voiceGain * masterGain;
                }
            }
        }

        // Save current as previous for next buffer
        for (int s = 0; s < SAMPLERS_PER_VOICE; ++s) {
            prevFreeSamplerPitchMod[s] = currSamplerPitchMods[s];
            prevFreeSamplerLoopStartMod[s] = currSamplerLoopStartMods[s];
            prevFreeSamplerLoopLengthMod[s] = currSamplerLoopLengthMods[s];
            prevFreeSamplerCrossfadeMod[s] = currSamplerCrossfadeMods[s];
            prevFreeSamplerLevelMod[s] = currSamplerLevelMods[s];
            prevFreeSamplerLevelOffset[s] = currSamplerLevelOffsets[s];
        }
    }

    // Mix chaos generators directly to output (post-voices, pre-filter)
    if (nChannels >= 2 && params) {
        // Determine solo state for chaos
        bool anyChaosSolo = false;
        for (int c = 0; c < 4; ++c) {
            if (params->chaosSolo[c].load()) { anyChaosSolo = true; break; }
        }
        for (unsigned int i = 0; i < nFrames; ++i) {
            float chaosL = 0.0f;
            float chaosR = 0.0f;
            float* frameSources = fmSourceBuffer.data() + static_cast<size_t>(i) * kFMSourceCount;
            for (int c = 0; c < 4; ++c) {
                bool muted = params->chaosMuted[c].load();
                bool solo = params->chaosSolo[c].load();
                if (anyChaosSolo) {
                    if (!solo) continue;
                } else if (muted) {
                    continue;
                }
                float level = params->getChaosLevel(c);
                // Apply modulation to level
                float modulatedLevel = std::clamp(level + lastGlobalModOutputs.chaosLevel[c], 0.0f, 1.0f);
                // Use per-frame chaos traces if available (populated by processChaos)
                float x = (i < chaosBufferX[c].size()) ? chaosBufferX[c][i] : chaosOutputs[c];
                float y = (i < chaosBufferY[c].size()) ? chaosBufferY[c][i] : chaos[c].getY();
                if (chaosDiffMode) {
                    float prevX = (i > 0 && i-1 < chaosBufferX[c].size()) ? chaosBufferX[c][i-1] : chaosLastX[c];
                    float prevY = (i > 0 && i-1 < chaosBufferY[c].size()) ? chaosBufferY[c][i-1] : chaosLastY[c];
                    x -= prevX;
                    y -= prevY;
                }
                frameSources[kFMOscillatorTargetCount + SAMPLERS_PER_VOICE + c * 2] = x;
                frameSources[kFMOscillatorTargetCount + SAMPLERS_PER_VOICE + c * 2 + 1] = y;
                chaosL += x * modulatedLevel;
                chaosR += y * modulatedLevel;
            }
            if (chaosL != 0.0f || chaosR != 0.0f) {
                output[i * nChannels + 0] += chaosL * voiceGain * masterGain;
                if (nChannels > 1) {
                    output[i * nChannels + 1] += chaosR * voiceGain * masterGain;
                }
            }
        }
        // Update last-sample state for diff mode
        if (chaosDiffMode) {
            for (int c = 0; c < 4; ++c) {
                if (!chaosBufferX[c].empty()) chaosLastX[c] = chaosBufferX[c].back();
                if (!chaosBufferY[c].empty()) chaosLastY[c] = chaosBufferY[c].back();
            }
        }
    }

    // Phase 3: Apply soft clipping and NaN/Inf sanitization
    // Process after all mixing (voices + samplers + chaos) but before filter/reverb
    // If the compressor is enabled (final stage), avoid double-clipping here.
    if (!compressorEnabled) {
        // Lowered threshold from 0.9 to 0.7 to catch peaks earlier
        for (unsigned int i = 0; i < nFrames * nChannels; ++i) {
            // Sanitize before soft clipping
            if (!std::isfinite(output[i])) {
                output[i] = 0.0f;
            }
            output[i] = softClip(output[i], 0.7f);
        }
    } else {
        // Still sanitize to avoid propagating NaNs/Infs
        for (unsigned int i = 0; i < nFrames * nChannels; ++i) {
            if (!std::isfinite(output[i])) {
                output[i] = 0.0f;
            }
        }
    }

    // Apply filter if enabled (stereo processing)
    if (filterEnabled && nChannels == 2) {
        // Apply filter modulation and smoothing (using lastGlobalModOutputs from previous buffer)
        if (params) {
            float baseCutoff = params->filterCutoff.load();
            float baseResonance = params->filterResonance.load();
            float baseDrive = params->filterDrive.load();
            float baseWidth = params->filterBandWidth.load();
            float baseNotchFeedback = params->filterNotchFeedback.load();
            float baseSpread = params->filterSpread.load();
            float baseDryWet = params->filterDryWet.load();

            // Cutoff: multiplicative modulation in octaves (±4 octaves max)
            // modValue of +1 = +4 octaves, -1 = -4 octaves
            float cutoffOctaves = lastGlobalModOutputs.filterCutoff * 4.0f;
            float targetCutoff = std::clamp(baseCutoff * std::pow(2.0f, cutoffOctaves), 20.0f, 20000.0f);

            // Resonance: scale to ±0.6 for good range without too much instability
            float targetResonance = std::clamp(baseResonance + lastGlobalModOutputs.filterResonance * 0.6f, 0.0f, 1.2f);

            // Drive: scale to ±7 for useful range
            float targetDrive = std::clamp(baseDrive + lastGlobalModOutputs.filterDrive * 7.0f, 0.1f, 15.0f);

            // Width, Spread, DryWet: full 0-1 range is good
            float targetWidth = std::clamp(baseWidth + lastGlobalModOutputs.filterWidth, 0.0f, 1.0f);
            float targetSpread = std::clamp(baseSpread + lastGlobalModOutputs.filterSpread, 0.0f, 1.0f);
            float targetDryWet = std::clamp(baseDryWet + lastGlobalModOutputs.filterDryWet, 0.0f, 1.0f);

            // Notch Feedback: scale to ±0.5 to avoid hitting stability limits too easily
            float targetNotchFeedback = std::clamp(baseNotchFeedback + lastGlobalModOutputs.filterNotchFeedback * 0.5f, 0.0f, 0.98f);

            // Apply one-pole smoothing for musical UI control
            constexpr float alpha = kFilterSmoothingAlpha;
            smoothedFilterCutoff += alpha * (targetCutoff - smoothedFilterCutoff);
            smoothedFilterResonance += alpha * (targetResonance - smoothedFilterResonance);
            smoothedFilterDrive += alpha * (targetDrive - smoothedFilterDrive);
            smoothedFilterWidth += alpha * (targetWidth - smoothedFilterWidth);
            smoothedFilterSpread += alpha * (targetSpread - smoothedFilterSpread);
            smoothedFilterDryWet += alpha * (targetDryWet - smoothedFilterDryWet);
            smoothedFilterNotchFeedback += alpha * (targetNotchFeedback - smoothedFilterNotchFeedback);

            // Use smoothed values for filter parameters
            float modulatedCutoff = smoothedFilterCutoff;
            float modulatedResonance = smoothedFilterResonance;
            float modulatedDrive = smoothedFilterDrive;
            float modulatedWidth = smoothedFilterWidth;
            float modulatedSpread = smoothedFilterSpread;
            float modulatedDryWet = smoothedFilterDryWet;
            float modulatedNotchFeedback = smoothedFilterNotchFeedback;

            // Apply modulated parameters to all filter types
            filterL.setCutoff(modulatedCutoff);
            filterR.setCutoff(modulatedCutoff);
            ladderFilterL.setCutoff(modulatedCutoff);
            ladderFilterR.setCutoff(modulatedCutoff);
            ladderFilterL.setResonance(modulatedResonance);
            ladderFilterR.setResonance(modulatedResonance);
            ladderFilterL.setDrive(modulatedDrive);
            ladderFilterR.setDrive(modulatedDrive);
            diodeFilterL.setCutoff(modulatedCutoff);
            diodeFilterR.setCutoff(modulatedCutoff);
            diodeFilterL.setResonance(modulatedResonance);
            diodeFilterR.setResonance(modulatedResonance);
            diodeFilterL.setDrive(modulatedDrive);
            diodeFilterR.setDrive(modulatedDrive);
            bandpassFilterL.setCutoff(modulatedCutoff);
            bandpassFilterR.setCutoff(modulatedCutoff);
            bandpassFilterL.setResonance(modulatedResonance);
            bandpassFilterR.setResonance(modulatedResonance);
            bandpassFilterL.setDrive(modulatedDrive);
            bandpassFilterR.setDrive(modulatedDrive);
            bandpassFilterL.setWidth(modulatedWidth);
            bandpassFilterR.setWidth(modulatedWidth);
            bandpass8FilterL.setCutoff(modulatedCutoff);
            bandpass8FilterR.setCutoff(modulatedCutoff);
            bandpass8FilterL.setResonance(modulatedResonance);
            bandpass8FilterR.setResonance(modulatedResonance);
            bandpass8FilterL.setDrive(modulatedDrive);
            bandpass8FilterR.setDrive(modulatedDrive);
            bandpass8FilterL.setWidth(modulatedWidth);
            bandpass8FilterR.setWidth(modulatedWidth);
            bandpass2FilterL.setCutoff(modulatedCutoff);
            bandpass2FilterR.setCutoff(modulatedCutoff);
            bandpass2FilterL.setResonance(modulatedResonance);
            bandpass2FilterR.setResonance(modulatedResonance);
            bandpass2FilterL.setDrive(modulatedDrive);
            bandpass2FilterR.setDrive(modulatedDrive);
            bandpass2FilterL.setWidth(modulatedWidth);
            bandpass2FilterR.setWidth(modulatedWidth);
            notchFilterL.setCutoff(modulatedCutoff);
            notchFilterR.setCutoff(modulatedCutoff);
            notchFilterL.setResonance(modulatedResonance);
            notchFilterR.setResonance(modulatedResonance);
            notchFilterL.setNotchFeedback(modulatedNotchFeedback);
            notchFilterR.setNotchFeedback(modulatedNotchFeedback);
            notchFilterL.setSpread(modulatedSpread);
            notchFilterR.setSpread(modulatedSpread);
            notchFilterL.setDryWet(modulatedDryWet);
            notchFilterR.setDryWet(modulatedDryWet);
        }

        for (unsigned int i = 0; i < nFrames; ++i) {
            float left = output[i * 2];
            float right = output[i * 2 + 1];
            
            // Apply selected filter type
            if (currentFilterType == 0) {  // Lowpass
                auto [lp_l, hp_l] = filterL.process(left);
                auto [lp_r, hp_r] = filterR.process(right);
                output[i * 2] = lp_l;
                output[i * 2 + 1] = lp_r;
            } else if (currentFilterType == 1) {  // Highpass
                auto [lp_l, hp_l] = filterL.process(left);
                auto [lp_r, hp_r] = filterR.process(right);
                output[i * 2] = hp_l;
                output[i * 2 + 1] = hp_r;
            } else if (currentFilterType == 2) {  // High shelf
                output[i * 2] = highShelfL.process(left);
                output[i * 2 + 1] = highShelfR.process(right);
            } else if (currentFilterType == 3) {  // Low shelf
                output[i * 2] = lowShelfL.process(left);
                output[i * 2 + 1] = lowShelfR.process(right);
            } else if (currentFilterType == 4) {  // Ladder LP (8-pole)
                output[i * 2] = ladderFilterL.process(left);
                output[i * 2 + 1] = ladderFilterR.process(right);
            } else if (currentFilterType == 5) {  // Diode ladder LP
                output[i * 2] = diodeFilterL.process(left);
                output[i * 2 + 1] = diodeFilterR.process(right);
            } else if (currentFilterType == 6) {  // Bandpass ladder
                output[i * 2] = bandpassFilterL.process(left);
                output[i * 2 + 1] = bandpassFilterR.process(right);
            } else if (currentFilterType == 7) {  // Dual Notch
                output[i * 2] = notchFilterL.process(left);
                output[i * 2 + 1] = notchFilterR.process(right);
            } else if (currentFilterType == 8) {  // 2-pole Bandpass
                output[i * 2] = bandpass2FilterL.process(left);
                output[i * 2 + 1] = bandpass2FilterR.process(right);
            } else if (currentFilterType == 9) {  // 8-pole Bandpass ladder
                output[i * 2] = bandpass8FilterL.process(left);
                output[i * 2 + 1] = bandpass8FilterR.process(right);
            }
        }
    }
    
    // Apply reverb if enabled (stereo processing)
    if (reverbEnabled && nChannels == 2) {
        // Create temporary buffers for left and right channels
        std::vector<float> leftChannel(nFrames);
        std::vector<float> rightChannel(nFrames);

        // De-interleave
        for (unsigned int i = 0; i < nFrames; ++i) {
            leftChannel[i] = output[i * 2];
            rightChannel[i] = output[i * 2 + 1];
        }

        // Process reverb
        reverb.process(leftChannel.data(), rightChannel.data(), nFrames);

        // Re-interleave
        for (unsigned int i = 0; i < nFrames; ++i) {
            output[i * 2] = leftChannel[i];
            output[i * 2 + 1] = rightChannel[i];
        }
    }

    // Apply compressor if enabled (stereo processing, in-place)
    // Compressor is the final stage: provides automatic gain control,
    // soft limiting, and tasteful saturation to prevent clipping
    if (compressorEnabled && nChannels == 2) {
        // Process interleaved stereo buffer in-place
        compressor.process(output, output, nFrames * nChannels);
    }
}

void Synth::updateLFOParameters(int lfoIndex, float period, int syncMode, int shape, float morph,
                                 float duty, bool flip, bool resetOnNote, float tempo) {
    if (lfoIndex < 0 || lfoIndex >= 4) return;

    lfos[lfoIndex].setPeriod(period);
    lfos[lfoIndex].setSyncMode(static_cast<LFOSyncMode>(syncMode));
    lfos[lfoIndex].setShape(shape);
    lfos[lfoIndex].setMorph(morph);
    lfos[lfoIndex].setDuty(duty);
    lfos[lfoIndex].setFlip(flip);
    lfos[lfoIndex].setResetOnNote(resetOnNote);
    lfos[lfoIndex].setTempo(tempo);
}

void Synth::processLFOs(float sampleRate, unsigned int nFrames) {
    // Process LFOs once per audio buffer (respect active count)
    int activeCount = params ? std::clamp(params->activeLfoCount.load(), 1, 4) : 4;
    for (int i = 0; i < 4; ++i) {
        if (i >= activeCount) {
            // Still update UI visual to zero out stale values
            if (params) {
                params->setLfoVisualState(i, 0.0f, lfos[i].getPhase());
            }
            continue;
        }
        // Apply modulation to LFO parameters (uses last buffer's outputs)
        float periodBase = params ? params->getLfoPeriod(i) : lfos[i].getPeriod();
        float morphBase = params ? params->getLfoMorph(i) : lfos[i].getMorph();
        float dutyBase = params ? params->getLfoDuty(i) : lfos[i].getDuty();

        float periodMod = lastGlobalModOutputs.lfoPeriod[i];
        float morphMod = lastGlobalModOutputs.lfoMorph[i];
        float dutyMod = lastGlobalModOutputs.lfoDuty[i];

        float modulatedPeriod = std::max(0.001f, periodBase + periodMod);
        float modulatedMorph = std::clamp(morphBase + morphMod, 0.0f, 1.0f);
        float modulatedDuty = std::clamp(dutyBase + dutyMod, 0.0f, 1.0f);

        lfos[i].setPeriod(modulatedPeriod);
        lfos[i].setMorph(modulatedMorph);
        lfos[i].setDuty(modulatedDuty);

        float value = lfos[i].process(sampleRate, nFrames);
        if (params) {
            params->setLfoVisualState(i, value, lfos[i].getPhase());
        }
        // Write to UI's LFO history buffer for rolling scope view
        if (ui) {
            ui->writeToLFOHistory(i, value);
        }
    }
}

void Synth::processChaos(unsigned int nFrames) {
    // Apply modulation to chaos parameters before processing
    float baseClockFreqs[4];
    if (params) {
        for (int i = 0; i < 4; ++i) {
            float baseClockFreq = params->getChaosClockFreq(i);
            float baseParameter = params->getChaosParameter(i);

            float clockOctaves = lastGlobalModOutputs.chaosClockFreq[i] * 4.0f;
            float modulatedClock = std::clamp(baseClockFreq * std::pow(2.0f, clockOctaves), 0.00001f, 20000.0f);
            chaos[i].setClockFrequency(modulatedClock);
            baseClockFreqs[i] = modulatedClock;

            float modulatedParam = std::clamp(baseParameter + lastGlobalModOutputs.chaosParameter[i] * 0.2f, 0.6f, 0.99f);
            chaos[i].setChaosParameter(modulatedParam);
        }
    } else {
        for (int i = 0; i < 4; ++i) {
            baseClockFreqs[i] = chaos[i].getClockFrequency();
        }
    }

    // Process chaos generators (respect active count)
    int activeCount = params ? std::clamp(params->activeChaosCount.load(), 1, 4) : 4;
    for (int i = 0; i < 4; ++i) {
        if (i >= activeCount) {
            chaosBufferX[i].clear();
            chaosBufferY[i].clear();
            chaosOutputs[i] = 0.0f;
            continue;
        }
        bool running = params ? params->getChaosRunning(i) : true;
        if (running) {
            // Clear and reserve space for per-sample values
            chaosBufferX[i].clear();
            chaosBufferY[i].clear();
            chaosBufferX[i].reserve(nFrames);
            chaosBufferY[i].reserve(nFrames);

            // Generate per-sample chaos values
            for (unsigned int frame = 0; frame < nFrames; ++frame) {
                chaosBufferX[i].push_back(chaos[i].process());
                chaosBufferY[i].push_back(chaos[i].getY());
            }

            // Update cached output (last sample)
            chaosOutputs[i] = chaosBufferX[i].back();
        } else {
            // If not running, clear buffers so fallback to chaosOutputs is used
            chaosBufferX[i].clear();
            chaosBufferY[i].clear();
        }
        chaos[i].setClockFrequency(baseClockFreqs[i]);
    }

    // Update visual state for UI (once per buffer, using last frame's values)
    if (params) {
        params->chaos1VisualX.store(chaosOutputs[0]);
        params->chaos1VisualY.store(chaos[0].getY());
        params->chaos2VisualX.store(chaosOutputs[1]);
        params->chaos2VisualY.store(chaos[1].getY());
        params->chaos3VisualX.store(chaosOutputs[2]);
        params->chaos3VisualY.store(chaos[2].getY());
        params->chaos4VisualX.store(chaosOutputs[3]);
        params->chaos4VisualY.store(chaos[3].getY());
    }
}

float Synth::getLFOOutput(int lfoIndex) const {
    if (lfoIndex < 0 || lfoIndex >= 4) return 0.0f;
    if (params) {
        int activeCount = std::clamp(params->activeLfoCount.load(), 1, 4);
        if (lfoIndex >= activeCount) return 0.0f;
    }
    return lfos[lfoIndex].getCurrentValue();
}

float Synth::getChaosOutput(int chaosIndex) const {
    if (chaosIndex < 0 || chaosIndex >= 4) return 0.0f;
    if (params) {
        int activeCount = std::clamp(params->activeChaosCount.load(), 1, 4);
        if (chaosIndex >= activeCount) return 0.0f;
    }
    return chaosOutputs[chaosIndex];
}

float Synth::getChaosOutputY(int chaosIndex) const {
    if (chaosIndex < 0 || chaosIndex >= 4) return 0.0f;
    if (params) {
        int activeCount = std::clamp(params->activeChaosCount.load(), 1, 4);
        if (chaosIndex >= activeCount) return 0.0f;
    }
    return chaos[chaosIndex].getY();
}

float Synth::getChaosOutputAtFrame(int chaosIndex, unsigned int frameIndex) const {
    if (chaosIndex < 0 || chaosIndex >= 4) return 0.0f;
    if (params) {
        int activeCount = std::clamp(params->activeChaosCount.load(), 1, 4);
        if (chaosIndex >= activeCount) return 0.0f;
    }
    // Use per-sample value from buffer if available, otherwise fall back to cached value
    if (frameIndex < chaosBufferX[chaosIndex].size()) {
        return chaosBufferX[chaosIndex][frameIndex];
    }
    return chaosOutputs[chaosIndex];
}

float Synth::getChaosOutputYAtFrame(int chaosIndex, unsigned int frameIndex) const {
    if (chaosIndex < 0 || chaosIndex >= 4) return 0.0f;
    if (params) {
        int activeCount = std::clamp(params->activeChaosCount.load(), 1, 4);
        if (chaosIndex >= activeCount) return 0.0f;
    }
    // Use per-sample value from buffer if available, otherwise fall back to cached value
    if (frameIndex < chaosBufferY[chaosIndex].size()) {
        return chaosBufferY[chaosIndex][frameIndex];
    }
    return chaos[chaosIndex].getY();
}

const ModulationSlot* Synth::getModulationSlot(int index) const {
    if (!ui || index < 0 || index >= kModulationSlotCount) {
        return nullptr;
    }
    return &ui->modulationSlots[index];
}

void Synth::refreshSamplerPhaseDrivers() {
    std::fill(std::begin(samplerPhaseSource), std::end(samplerPhaseSource), kClockModSourceIndex);
    std::fill(std::begin(samplerPhaseType), std::end(samplerPhaseType), 0);

    for (int slotIdx = 0; slotIdx < kModulationSlotCount; ++slotIdx) {
        const ModulationSlot* slot = getModulationSlot(slotIdx);
        if (!slot || !slot->isComplete()) {
            continue;
        }

        if (slot->destination >= kClockTargetSamplerBase &&
            slot->destination < kClockTargetSamplerBase + SAMPLERS_PER_VOICE) {
            int samplerIdx = slot->destination - kClockTargetSamplerBase;
            samplerPhaseSource[samplerIdx] = slot->source >= 0 ? slot->source : kClockModSourceIndex;
            samplerPhaseType[samplerIdx] = slot->type >= 0 ? slot->type : 0;
        }
    }
}

float Synth::normalizePhaseForDriver(float value, int type) const {
    if (type == 0) {
        return std::clamp(value, 0.0f, 1.0f);
    }
    return std::clamp((value + 1.0f) * 0.5f, 0.0f, 1.0f);
}

float Synth::getModulationSource(int sourceIndex, const Voice* voiceContext) {
    // Source indices from ui_mod_data.cpp:
    // 0-3: LFO 1-4
    // 4-7: ENV 1-4
    // 8: MIDI Note, 9: Velocity, 10: Aftertouch, 11: Mod Wheel, 12: Pitch Bend
    // 13: Clock
    // 14-21: Chaos 1-4 X/Y pairs (14=C1X, 15=C1Y, 16=C2X, 17=C2Y, etc.)

    if (sourceIndex >= 0 && sourceIndex <= 3) {
        // LFO 1-4
        return getLFOOutput(sourceIndex);
    } else if (sourceIndex >= 4 && sourceIndex <= 7) {
        // ENV 1-4 (per-voice)
        int envIdx = sourceIndex - 4; // 0..3
        if (params) {
            int activeEnv = std::clamp(params->activeEnvCount.load(), 1, 4);
            if (envIdx >= activeEnv) return 0.0f;
        }
        if (voiceContext) {
            return voiceContext->getEnvelopeValue(envIdx);
        }
        // Fallback: max of active voices for this envelope index
        float maxEnv = 0.0f;
        for (int i = MAX_VOICES - 1; i >= 0; --i) {
            if (voices[i].active) {
                float env = voices[i].getEnvelopeValue(envIdx);
                if (env > maxEnv) maxEnv = env;
            }
        }
        return maxEnv;
    } else if (sourceIndex == 8) {
        // MIDI Note (0-127 mapped to -1 to +1, with 64 as center)
        if (voiceContext && voiceContext->active) {
            return (voiceContext->note - 64.0f) / 64.0f;
        }
        // If no voice context, use most recent active voice
        for (int i = MAX_VOICES - 1; i >= 0; --i) {
            if (voices[i].active) {
                return (voices[i].note - 64.0f) / 64.0f;
            }
        }
        return 0.0f;
    } else if (sourceIndex == 9) {
        // Velocity (0-127 mapped to 0 to +1)
        if (voiceContext && voiceContext->active) {
            return voiceContext->velocity / 127.0f;
        }
        // If no voice context, use most recent active voice
        for (int i = MAX_VOICES - 1; i >= 0; --i) {
            if (voices[i].active) {
                return voices[i].velocity / 127.0f;
            }
        }
        return 0.0f;
    } else if (sourceIndex == 10) {
        // Aftertouch - TODO: implement
        return 0.0f;
    } else if (sourceIndex == 11) {
        // Mod Wheel - TODO: implement
        return 0.0f;
    } else if (sourceIndex == 12) {
        // Pitch Bend - TODO: implement
        return 0.0f;
    } else if (sourceIndex == 13) {
        // Clock
        if (clock) {
            float phase = static_cast<float>(clock->getPhase(Subdivision::SIXTEENTH));
            return phase * 2.0f - 1.0f;
        }
        return -1.0f;
    } else if (sourceIndex >= 14 && sourceIndex <= 21) {
        // Chaos 1-4 X/Y pairs
        int chaosIndex = (sourceIndex - 14) / 2;  // 14,15->0, 16,17->1, 18,19->2, 20,21->3
        bool isY = ((sourceIndex - 14) % 2) == 1;  // Odd indices are Y
        if (isY) {
            return getChaosOutputY(chaosIndex);
        } else {
            return getChaosOutput(chaosIndex);
        }
    }

    return 0.0f;
}

float Synth::applyModCurve(float input, int curveType) {
    // Curve types from ui_mod.cpp:
    // 0: Linear, 1: Exponential, 2: Logarithmic, 3: S-Curve

    switch (curveType) {
        case 0: // Linear
            return input;

        case 1: // Exponential
            if (input >= 0.0f) {
                return (std::exp(input) - 1.0f) / (std::exp(1.0f) - 1.0f);
            } else {
                return -(std::exp(-input) - 1.0f) / (std::exp(1.0f) - 1.0f);
            }

        case 2: // Logarithmic
            if (input >= 0.0f) {
                return std::log(1.0f + input) / std::log(2.0f);
            } else {
                return -std::log(1.0f - input) / std::log(2.0f);
            }

        case 3: // S-Curve (tanh)
            return std::tanh(input * 2.0f);

        default:
            return input;
    }
}

Synth::ModulationOutputs Synth::processModulationMatrix(const Voice* voiceContext) {
    ModulationOutputs outputs;

    if (!ui) return outputs;

    // Process all 16 modulation slots
    for (int i = 0; i < 16; ++i) {
        const ModulationSlot& slot = ui->modulationSlots[i];

        // Skip empty or incomplete slots
        if (!slot.isComplete()) continue;

        // Get source value (-1 to +1)
        float sourceValue = getModulationSource(slot.source, voiceContext);

        // Apply curve shaping
        float shapedValue = applyModCurve(sourceValue, slot.curve);

        // Amount scaling (-99..+99 -> -1..+1 approx)
        float amount = static_cast<float>(slot.amount) / 99.0f;
        float modValue;

        // Type 0 = Unidirectional (-->) maps source -1..+1 to 0..1, then scales by amount
        // Type 1 = Bidirectional (<->) scales -1..+1 directly by amount
        if (slot.type == 0) {
            if (slot.source >= 4 && slot.source <= 7) {
                float uniSource = std::clamp(shapedValue, 0.0f, 1.0f);
                modValue = uniSource * amount;
            } else {
                float mapped01 = (shapedValue + 1.0f) * 0.5f;  // 0..1
                modValue = mapped01 * amount;
            }
        } else {
            modValue = shapedValue * amount;
        }

        int destination = slot.destination;

        // Apply to destination
        // Destination indices from ui_mod_data:
        // 0-5: OSC 1 Pitch/Morph/Duty/Ratio/Offset/Amp
        // 6-11: OSC 2 Pitch/Morph/Duty/Ratio/Offset/Amp
        // 12-17: OSC 3 Pitch/Morph/Duty/Ratio/Offset/Amp
        // 18-23: OSC 4 Pitch/Morph/Duty/Ratio/Offset/Amp
        // 24-30: Filter Cutoff/Resonance/Drive/Width/NotchFeedback/Spread/DryWet
        // 31-32: Reverb Mix/Size
        // 33-37: SAMP 1 Pitch/LoopStart/LoopLength/Crossfade/Level
        // 38-42: SAMP 2 Pitch/LoopStart/LoopLength/Crossfade/Level
        // 43-47: SAMP 3 Pitch/LoopStart/LoopLength/Crossfade/Level
        // 48-52: SAMP 4 Pitch/LoopStart/LoopLength/Crossfade/Level
        // 53-64: LFO 1-4 Rate/Morph/Duty
        // 65: Mixer Master Volume
        // 66-69: Mixer Oscillator Levels
        // 70-73: Mixer Sampler Levels
        // 74-77: Sequencer Track 1-4 Phase Drivers
        // 78-81: Sampler 1-4 Phase Drivers
        // 82: FM Global Depth
        // 83- (83 + kFMTargetCount*kFMSourceCount - 1): Individual FM depths
        // Remaining: Chaos parameters (Clock/U/Level)

        const int fmGlobalIndex = 82;
        const int fmCellStart = fmGlobalIndex + 1;
        const int fmCellEnd = fmCellStart + kFMTargetCount * kFMSourceCount;
        const int chaosBase = fmCellEnd;

        if (destination == fmGlobalIndex) {
            outputs.fmGlobalDepth += modValue;
            continue;
        }

        if (destination >= fmCellStart && destination < fmCellEnd) {
            // Skip per-voice sources (ENV1-4) in global pass
            if (!voiceContext && slot.source >= 4 && slot.source <= 7) {
                continue;
            }
            int cellIndex = destination - fmCellStart;
            int target = cellIndex / kFMSourceCount;
            int source = cellIndex % kFMSourceCount;
            outputs.fmDepth[target][source] += modValue;
            continue;
        }

        if (destination >= chaosBase) {
            int chaosIndex = (destination - chaosBase) / 3;
            int chaosParam = (destination - chaosBase) % 3;
            if (chaosIndex >= 0 && chaosIndex < 4) {
                switch (chaosParam) {
                    case 0: outputs.chaosClockFreq[chaosIndex] += modValue; break;
                    case 1: outputs.chaosParameter[chaosIndex] += modValue; break;
                    case 2: outputs.chaosLevel[chaosIndex] += modValue; break;
                }
                continue;
            }
        }

        auto adjustAmpMod = [&](float value) -> float {
            if (slot.type == 1 && slot.source >= 4 && slot.source <= 7 && amount > 0.0f) {
                return value - amount;
            }
            return value;
        };

        switch (destination) {
            // OSC 1
            case 0: outputs.osc1Pitch += modValue; break;
            case 1: outputs.osc1Morph += modValue; break;
            case 2: outputs.osc1Duty += modValue; break;
            case 3: outputs.osc1Ratio += modValue; break;
            case 4: outputs.osc1Offset += modValue; break;
            case 5: outputs.osc1Amp += adjustAmpMod(modValue); break;  // Changed from Level to Amp
            // OSC 2
            case 6: outputs.osc2Pitch += modValue; break;
            case 7: outputs.osc2Morph += modValue; break;
            case 8: outputs.osc2Duty += modValue; break;
            case 9: outputs.osc2Ratio += modValue; break;
            case 10: outputs.osc2Offset += modValue; break;
            case 11: outputs.osc2Amp += adjustAmpMod(modValue); break;  // Changed from Level to Amp
            // OSC 3
            case 12: outputs.osc3Pitch += modValue; break;
            case 13: outputs.osc3Morph += modValue; break;
            case 14: outputs.osc3Duty += modValue; break;
            case 15: outputs.osc3Ratio += modValue; break;
            case 16: outputs.osc3Offset += modValue; break;
            case 17: outputs.osc3Amp += adjustAmpMod(modValue); break;  // Changed from Level to Amp
            // OSC 4
            case 18: outputs.osc4Pitch += modValue; break;
            case 19: outputs.osc4Morph += modValue; break;
            case 20: outputs.osc4Duty += modValue; break;
            case 21: outputs.osc4Ratio += modValue; break;
            case 22: outputs.osc4Offset += modValue; break;
            case 23: outputs.osc4Amp += adjustAmpMod(modValue); break;  // Changed from Level to Amp
            // Filter
            case 24: outputs.filterCutoff += modValue; break;
            case 25: outputs.filterResonance += modValue; break;
            case 26: outputs.filterDrive += modValue; break;
            case 27: outputs.filterWidth += modValue; break;
            case 28: outputs.filterNotchFeedback += modValue; break;
            case 29: outputs.filterSpread += modValue; break;
            case 30: outputs.filterDryWet += modValue; break;
            // Reverb
            case 31: outputs.reverbMix += modValue; break;
            case 32: outputs.reverbSize += modValue; break;
            // SAMP 1
            case 33: outputs.samp1Pitch += modValue; break;
            case 34: outputs.samp1LoopStart += modValue; break;
            case 35: outputs.samp1LoopLength += modValue; break;
            case 36: outputs.samp1Crossfade += modValue; break;
            case 37: outputs.samp1Amp += adjustAmpMod(modValue); break;
            // SAMP 2
            case 38: outputs.samp2Pitch += modValue; break;
            case 39: outputs.samp2LoopStart += modValue; break;
            case 40: outputs.samp2LoopLength += modValue; break;
            case 41: outputs.samp2Crossfade += modValue; break;
            case 42: outputs.samp2Amp += adjustAmpMod(modValue); break;
            // SAMP 3
            case 43: outputs.samp3Pitch += modValue; break;
            case 44: outputs.samp3LoopStart += modValue; break;
            case 45: outputs.samp3LoopLength += modValue; break;
            case 46: outputs.samp3Crossfade += modValue; break;
            case 47: outputs.samp3Amp += adjustAmpMod(modValue); break;
            // SAMP 4
            case 48: outputs.samp4Pitch += modValue; break;
            case 49: outputs.samp4LoopStart += modValue; break;
            case 50: outputs.samp4LoopLength += modValue; break;
            case 51: outputs.samp4Crossfade += modValue; break;
            case 52: outputs.samp4Amp += adjustAmpMod(modValue); break;
            // LFO 1
            case 53: outputs.lfoPeriod[0] += modValue; break;
            case 54: outputs.lfoMorph[0] += modValue; break;
            case 55: outputs.lfoDuty[0] += modValue; break;
            // LFO 2
            case 56: outputs.lfoPeriod[1] += modValue; break;
            case 57: outputs.lfoMorph[1] += modValue; break;
            case 58: outputs.lfoDuty[1] += modValue; break;
            // LFO 3
            case 59: outputs.lfoPeriod[2] += modValue; break;
            case 60: outputs.lfoMorph[2] += modValue; break;
            case 61: outputs.lfoDuty[2] += modValue; break;
            // LFO 4
            case 62: outputs.lfoPeriod[3] += modValue; break;
            case 63: outputs.lfoMorph[3] += modValue; break;
            case 64: outputs.lfoDuty[3] += modValue; break;
            // Mixer
            case 65: outputs.mixerMasterVolume += modValue; break;
            case 66: outputs.mixerOscLevel[0] += modValue; break;
            case 67: outputs.mixerOscLevel[1] += modValue; break;
            case 68: outputs.mixerOscLevel[2] += modValue; break;
            case 69: outputs.mixerOscLevel[3] += modValue; break;
            case 70: outputs.mixerSamplerLevel[0] += modValue; break;
            case 71: outputs.mixerSamplerLevel[1] += modValue; break;
            case 72: outputs.mixerSamplerLevel[2] += modValue; break;
            case 73: outputs.mixerSamplerLevel[3] += modValue; break;
            // Sequencer phase drivers
            case 74: outputs.sequencerPhase[0] += modValue; break;
            case 75: outputs.sequencerPhase[1] += modValue; break;
            case 76: outputs.sequencerPhase[2] += modValue; break;
            case 77: outputs.sequencerPhase[3] += modValue; break;
            // Sampler phase drivers
            case 78: outputs.samplerPhase[0] += modValue; break;
            case 79: outputs.samplerPhase[1] += modValue; break;
            case 80: outputs.samplerPhase[2] += modValue; break;
            case 81: outputs.samplerPhase[3] += modValue; break;
        }
    }

    return outputs;
}

void Synth::setParams(SynthParameters* params_ptr) {
    params = params_ptr;
    // Update all voice pointers for FM matrix access and base levels
    for (auto& voice : voices) {
        voice.params = params_ptr;
        voice.synth = this;
    }
}

// ── Sampler Control Methods ─────────────────────────────────────────────────

void Synth::setSamplerSample(int samplerIndex, int sampleIndex) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    const SampleData* sample = sampleBank.getSample(sampleIndex);
    currentSampleIndices[samplerIndex] = sampleIndex;

    // Apply to all voices
    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setSample(sample);
    }
    freeSamplers[samplerIndex].setSample(sample);
}

void Synth::setSamplerLoopStart(int samplerIndex, float normalized) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setLoopStart(normalized);
    }
    freeSamplers[samplerIndex].setLoopStart(normalized);
}

void Synth::setSamplerLoopLength(int samplerIndex, float normalized) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setLoopLength(normalized);
    }
    freeSamplers[samplerIndex].setLoopLength(normalized);
}

void Synth::setSamplerCrossfadeLength(int samplerIndex, float normalized) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setCrossfadeLength(normalized);
    }
    freeSamplers[samplerIndex].setCrossfadeLength(normalized);
}

void Synth::setSamplerPlaybackSpeed(int samplerIndex, float speed) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setPlaybackSpeed(speed);
    }
    freeSamplers[samplerIndex].setPlaybackSpeed(speed);
}

void Synth::setSamplerOctave(int samplerIndex, int octave) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    // Clamp octave to -5 to +5
    samplerOctaves[samplerIndex] = std::max(-5, std::min(5, octave));

    // Recalculate and update playback speed
    // Formula: 2^(octave + tune * 0.5)
    float speed = std::pow(2.0f, samplerOctaves[samplerIndex] + samplerTunes[samplerIndex] * 0.5f);
    setSamplerPlaybackSpeed(samplerIndex, speed);
}

void Synth::setSamplerTune(int samplerIndex, float tune) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    // Clamp tune to -1.0 to +1.0
    samplerTunes[samplerIndex] = std::max(-1.0f, std::min(1.0f, tune));

    // Recalculate and update playback speed
    // Formula: 2^(octave + tune * 0.5)
    float speed = std::pow(2.0f, samplerOctaves[samplerIndex] + samplerTunes[samplerIndex] * 0.5f);
    setSamplerPlaybackSpeed(samplerIndex, speed);
}

void Synth::setSamplerSyncMode(int samplerIndex, int mode) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    // Clamp sync mode to 0-3 (Off, On, Trip, Dot)
    samplerSyncModes[samplerIndex] = std::max(0, std::min(3, mode));

    // Tempo sync is implemented in sampler's calculateLoopBoundaries
}

void Synth::setSamplerNoteReset(int samplerIndex, bool enabled) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    samplerNoteResets[samplerIndex] = enabled;

    // TODO: Implement note reset functionality
}

void Synth::setSamplerTZFMDepth(int samplerIndex, float depth) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setTZFMDepth(depth);
    }
    freeSamplers[samplerIndex].setTZFMDepth(depth);
}

void Synth::setSamplerPlaybackMode(int samplerIndex, PlaybackMode mode) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setPlaybackMode(mode);
    }
    freeSamplers[samplerIndex].setPlaybackMode(mode);
}

void Synth::setSamplerLevel(int samplerIndex, float level) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setLevel(level);
    }
    freeSamplers[samplerIndex].setLevel(level);
}

void Synth::setSamplerKeyMode(int samplerIndex, bool enabled) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }

    samplerKeyModes[samplerIndex] = enabled;

    for (auto& voice : voices) {
        voice.samplers[samplerIndex].setKeyMode(enabled);
        voice.samplers[samplerIndex].stopPlayback();
    }

    if (enabled) {
        freeSamplers[samplerIndex].stopPlayback();
    } else {
        freeSamplers[samplerIndex].setKeyMode(false);
        freeSamplers[samplerIndex].requestRestart();
    }
    freeSamplers[samplerIndex].setKeyMode(false);

    // Check if ANY oscillator or sampler is in FREE mode
    bool anyFreeMode = false;

    // Check all oscillators
    for (const auto& voice : voices) {
        for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
            if (voice.oscillators[i].getMode() == BrainwaveMode::FREE) {
                anyFreeMode = true;
                break;
            }
        }
        if (anyFreeMode) break;
    }

    // Note: FREE sampler playback is handled by freeSamplers path directly.
    // Do not spawn a free-running voice based on sampler FREE mode alone.

    // Spawn or kill free-running voice based on mode
    if (anyFreeMode && !freeRunningVoiceActive) {
        spawnFreeRunningVoice();
    } else if (!anyFreeMode && freeRunningVoiceActive) {
        killFreeRunningVoice();
    }
}

void Synth::saveSamplerPhase(int samplerIndex, uint64_t phase) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return;
    }
    // Only save if Note Reset is OFF (so phase persists across notes)
    if (!samplerNoteResets[samplerIndex]) {
        std::lock_guard<std::mutex> lock(samplerPhaseMutex);
        samplerLastPhases[samplerIndex] = phase;
    }
}

// Get sampler state (from first voice as they're all synced)
int Synth::getSamplerSampleIndex(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return -1;
    }
    return currentSampleIndices[samplerIndex];
}

float Synth::getSamplerLoopStart(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE || voices.empty()) {
        return 0.0f;
    }
    return voices[0].samplers[samplerIndex].getLoopStart();
}

float Synth::getSamplerLoopLength(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE || voices.empty()) {
        return 1.0f;
    }
    return voices[0].samplers[samplerIndex].getLoopLength();
}

float Synth::getSamplerCrossfadeLength(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE || voices.empty()) {
        return 0.1f;
    }
    return voices[0].samplers[samplerIndex].getCrossfadeLength();
}

float Synth::getSamplerPlaybackSpeed(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE || voices.empty()) {
        return 1.0f;
    }
    return voices[0].samplers[samplerIndex].getPlaybackSpeed();
}

float Synth::getSamplerTZFMDepth(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE || voices.empty()) {
        return 0.0f;
    }
    return voices[0].samplers[samplerIndex].getTZFMDepth();
}

PlaybackMode Synth::getSamplerPlaybackMode(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE || voices.empty()) {
        return PlaybackMode::FORWARD;
    }
    return voices[0].samplers[samplerIndex].getPlaybackMode();
}

float Synth::getSamplerLevel(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE || voices.empty()) {
        return 1.0f;
    }
    return voices[0].samplers[samplerIndex].getLevel();
}

bool Synth::getSamplerKeyMode(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return true;
    }
    return samplerKeyModes[samplerIndex];
}

int Synth::getSamplerOctave(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return 0;
    }
    return samplerOctaves[samplerIndex];
}

float Synth::getSamplerTune(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return 0.0f;
    }
    return samplerTunes[samplerIndex];
}

int Synth::getSamplerSyncMode(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return 0;
    }
    return samplerSyncModes[samplerIndex];
}

bool Synth::getSamplerNoteReset(int samplerIndex) const {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) {
        return true;
    }
    return samplerNoteResets[samplerIndex];
}

float Synth::getModulatedOscLevel(int index) const {
    if (index < 0 || index >= OSCILLATORS_PER_VOICE) {
        return 0.0f;
    }
    float baseLevel = oscillatorBaseLevels[index];
    float offset = lastGlobalModOutputs.mixerOscLevel[index];
    return std::clamp(baseLevel + offset, 0.0f, 1.0f);
}

float Synth::getMixerSamplerLevelMod(int index) const {
    if (index < 0 || index >= SAMPLERS_PER_VOICE) {
        return 0.0f;
    }
    return lastGlobalModOutputs.mixerSamplerLevel[index];
}

float Synth::getMixerOscLevelMod(int index) const {
    if (index < 0 || index >= OSCILLATORS_PER_VOICE) {
        return 0.0f;
    }
    return lastGlobalModOutputs.mixerOscLevel[index];
}

float Synth::getFMDepthMod(int target, int source) const {
    if (target < 0 || target >= kFMTargetCount ||
        source < 0 || source >= kFMSourceCount) {
        return 0.0f;
    }
    return lastGlobalModOutputs.fmDepth[target][source];
}

// Chaos generator control methods
void Synth::setChaosParameter(int chaosIndex, float value) {
    if (chaosIndex < 0 || chaosIndex >= 4) return;
    chaos[chaosIndex].setChaosParameter(value);
}

void Synth::setChaosClockFreq(int chaosIndex, float freq) {
    if (chaosIndex < 0 || chaosIndex >= 4) return;
    chaos[chaosIndex].setClockFrequency(freq);
    // Auto-enable FAST mode at high rates so chaos runs per-sample
    if (freq >= (sampleRate * 0.5f)) {
        chaos[chaosIndex].setFastMode(true);
    }
}

void Synth::setChaosFastMode(int chaosIndex, bool fast) {
    if (chaosIndex < 0 || chaosIndex >= 4) return;
    chaos[chaosIndex].setFastMode(fast);
}

void Synth::setChaosInterpMode(int chaosIndex, int mode) {
    if (chaosIndex < 0 || chaosIndex >= 4) return;
    chaos[chaosIndex].setInterpMode(mode);
}

void Synth::resetChaosGenerator(int chaosIndex) {
    if (chaosIndex < 0 || chaosIndex >= 4) return;
    chaos[chaosIndex].reset();
}

void Synth::resetAudioState() {
    for (auto& voice : voices) {
        voice.forceSilence();
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        freeSamplers[i].stopPlayback();
    }
    std::fill(fmSourceBuffer.begin(), fmSourceBuffer.end(), 0.0f);
    std::fill(fmSourceBufferPrev.begin(), fmSourceBufferPrev.end(), 0.0f);
    for (int i = 0; i < 4; ++i) {
        chaosBufferX[i].clear();
        chaosBufferY[i].clear();
        chaosOutputs[i] = 0.0f;
    }
    freeRunningVoiceActive = false;
    freeRunningVoiceIndex = -1;
}
