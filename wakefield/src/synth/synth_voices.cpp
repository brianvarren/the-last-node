// ============================================================================
// SYNTH VOICES - Voice Management and Note Triggering
// ============================================================================
//
// RESPONSIBILITIES:
// - Voice allocation: findFreeVoice() with stealing priority
// - Note triggering: noteOn/noteOff for MIDI and sequencer tracks
// - Free-running voice management: spawn/kill for oscillators and samplers
// - Voice state queries: getActiveVoiceCount, isVoiceActive, getVoiceNote
//
// DOES NOT CONTAIN:
// - Audio rendering (see synth_process.cpp)
// - Parameter setters (see synth_parameters.cpp)
// - DSP processing (see voice.cpp, oscillator.cpp, etc.)
//
// ============================================================================

#include "../synth.h"
#include "../ui.h"
#include "../clock.h"
#include <algorithm>
#include <limits>

int Synth::findFreeVoice() {
    // Priority 1: Find truly inactive voice (OFF stage)
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!voices[i].active) {
            return i;
        }
    }

    // All voices are active - need to steal one
    // Priority 2: Steal voice already releasing (gate headed toward off)
    int releaseVoiceIndex = -1;
    uint64_t oldestReleaseTime = UINT64_MAX;

    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].freeRunningOscIndex >= 0 || voices[i].freeRunningSamplerIndex >= 0) {
            continue;
        }
        if (voices[i].isGateReleasing()) {
            if (voices[i].startTime < oldestReleaseTime) {
                oldestReleaseTime = voices[i].startTime;
                releaseVoiceIndex = i;
            }
        }
    }

    if (releaseVoiceIndex != -1) {
        return releaseVoiceIndex;
    }

    // Priority 3: Steal oldest active voice (still held)
    int oldestVoiceIndex = -1;
    uint64_t oldestTime = UINT64_MAX;

    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].freeRunningOscIndex >= 0 || voices[i].freeRunningSamplerIndex >= 0) {
            continue;
        }
        if (!voices[i].isGateReleasing()) {
            if (voices[i].startTime < oldestTime) {
                oldestTime = voices[i].startTime;
                oldestVoiceIndex = i;
            }
        }
    }

    if (oldestVoiceIndex != -1) {
        return oldestVoiceIndex;
    }

    // Priority 4: Steal voice with lowest current amplitude level
    int quietestVoiceIndex = -1;
    float lowestLevel = std::numeric_limits<float>::max();

    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].freeRunningOscIndex >= 0 || voices[i].freeRunningSamplerIndex >= 0) {
            continue;
        }
        float level = voices[i].getCurrentAmpLevel();
        if (level < lowestLevel) {
            lowestLevel = level;
            quietestVoiceIndex = i;
        }
    }

    return quietestVoiceIndex;
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
    voice.noteSource = static_cast<int>(NoteSource::EXTERNAL_MIDI);
    voice.startTime = voiceCounter++;  // Assign timestamp for voice stealing priority
    voice.freeRunningOscIndex = -1;
    voice.freeRunningSamplerIndex = -1;

    float frequency = midiNoteToFrequency(midiNote);
    // Update note frequency for oscillators in KEY mode and reset phase
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        voice.oscillators[i].setNoteFrequency(frequency);
        voice.oscillators[i].reset();  // Reset phase for new note
        voice.pitchMod[i] = 0.0f;
        voice.morphMod[i] = 0.0f;
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
    voice.resetAmpControllers();
    voice.resetFMHistory();

    // Trigger envelope attack on all envelopes
    // If stealing an active voice, start from current level to avoid discontinuity clicks
    for (int ei = 0; ei < 4; ++ei) {
        voice.envelopes[ei].noteOn(isStealingVoice);
    }

    // Reset LFOs if resetOnNote is enabled
    for (int li = 0; li < 4; ++li) {
        if (lfos[li].getResetOnNote()) {
            lfos[li].reset();
        }
    }
    voice.ampGateTarget = 1.0f;
    if (!isStealingVoice) {
        voice.ampGateValue = 0.0f;
    }
}

void Synth::noteOff(int midiNote) {
    // Find all voices playing this note and trigger their release
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].active && voices[i].note == midiNote &&
            voices[i].freeRunningOscIndex < 0 &&
            voices[i].freeRunningSamplerIndex < 0) {
            for (int ei = 0; ei < 4; ++ei) {
                voices[i].envelopes[ei].noteOff();  // Trigger release
            }
            voices[i].ampGateTarget = 0.0f;
        }
    }

    fmSourceBufferPrev = fmSourceBuffer;
}

void Synth::sequencerNoteOn(int trackIndex, int midiNote, int velocity, float gatePercent, float probability) {
    if (trackIndex < 0 || trackIndex >= 4 || !params) return;

    // Update track state for modulation sources
    trackState[trackIndex].note = midiNote;
    trackState[trackIndex].velocity = velocity;
    trackState[trackIndex].gatePercent = gatePercent;
    trackState[trackIndex].probability = probability;
    trackState[trackIndex].active = true;

    // Determine the NoteSource value for this track (TRACK_1 = 2, TRACK_2 = 3, etc.)
    int trackNoteSource = static_cast<int>(NoteSource::TRACK_1) + trackIndex;

    // Check if any oscillators or samplers are listening to this track
    bool anyListening = false;
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        if (params->getOscNoteSource(i) == trackNoteSource) {
            anyListening = true;
            break;
        }
    }
    if (!anyListening) {
        for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
            if (params->getSamplerNoteSource(i) == trackNoteSource) {
                anyListening = true;
                break;
            }
        }
    }

    // If no generators are listening to this track, skip voice allocation
    if (!anyListening) return;

    // Allocate voice for this track
    int voiceIndex = findFreeVoice();
    Voice& voice = voices[voiceIndex];

    bool isStealingVoice = voice.active;

    voice.active = true;
    voice.note = midiNote;
    voice.velocity = velocity;
    voice.noteSource = trackNoteSource;  // Set to the appropriate track
    voice.startTime = voiceCounter++;
    voice.freeRunningOscIndex = -1;
    voice.freeRunningSamplerIndex = -1;

    float frequency = midiNoteToFrequency(midiNote);
    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        voice.oscillators[i].setNoteFrequency(frequency);
        voice.oscillators[i].reset();
        voice.pitchMod[i] = 0.0f;
        voice.morphMod[i] = 0.0f;
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
            if (samplerNoteResets[i]) {
                voice.samplers[i].requestRestart();
            } else {
                voice.samplers[i].restorePhase(samplerLastPhases[i]);
                voice.samplers[i].requestRestart();
            }
        } else {
            voice.samplers[i].stopPlayback();
        }
    }
    voice.resetAmpControllers();
    voice.resetFMHistory();

    for (int ei = 0; ei < 4; ++ei) {
        voice.envelopes[ei].noteOn(isStealingVoice);
    }
    voice.ampGateTarget = 1.0f;
}

void Synth::sequencerNoteOff(int trackIndex, int midiNote) {
    if (trackIndex < 0 || trackIndex >= 4) return;

    // Mark track as inactive
    trackState[trackIndex].active = false;

    // Trigger note off for this MIDI note
    noteOff(midiNote);
}

int Synth::allocateFreeRunningVoiceSlot() {
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!voices[i].active) {
            return i;
        }
    }

    float lowestLevel = std::numeric_limits<float>::max();
    int quietestVoice = -1;
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].freeRunningOscIndex >= 0 || voices[i].freeRunningSamplerIndex >= 0) continue;
        float level = voices[i].getCurrentAmpLevel();
        if (level < lowestLevel) {
            lowestLevel = level;
            quietestVoice = i;
        }
    }
    if (quietestVoice >= 0) {
        return quietestVoice;
    }

    for (int i = 0; i < OSCILLATORS_PER_VOICE; ++i) {
        if (freeRunningVoiceActive[i]) {
            int donorIndex = freeRunningVoiceIndex[i];
            killFreeRunningVoice(i);
            return donorIndex;
        }
    }
    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        if (freeRunningSamplerActive[i]) {
            int donorIndex = freeRunningSamplerVoiceIndex[i];
            killFreeRunningSamplerVoice(i);
            return donorIndex;
        }
    }
    return -1;
}

void Synth::spawnFreeRunningVoice(int oscIndex) {
    if (oscIndex < 0 || oscIndex >= OSCILLATORS_PER_VOICE) return;
    if (freeRunningVoiceActive[oscIndex]) {
        int existingIndex = freeRunningVoiceIndex[oscIndex];
        if (existingIndex >= 0 && existingIndex < MAX_VOICES) {
            Voice& voice = voices[existingIndex];
            voice.ampGateTarget = 1.0f;
            voice.ampGateValue = 1.0f;
        }
        return;
    }

    int voiceIndex = allocateFreeRunningVoiceSlot();
    if (voiceIndex == -1) {
        return;
    }

    Voice& voice = voices[voiceIndex];
    voice.forceSilence();
    voice.active = true;
    voice.note = 60;
    voice.velocity = 100;
    voice.noteSource = static_cast<int>(NoteSource::FREE);
    voice.freeRunningOscIndex = oscIndex;
    voice.freeRunningSamplerIndex = -1;
    voice.startTime = voiceCounter++;

    voice.resetFMHistory();
    for (int ei = 0; ei < 4; ++ei) {
        voice.envelopes[ei].noteOn();
    }
    voice.ampGateTarget = 1.0f;
    voice.ampGateValue = 1.0f;

    freeRunningVoiceActive[oscIndex] = true;
    freeRunningVoiceIndex[oscIndex] = voiceIndex;
}
void Synth::killFreeRunningVoice(int oscIndex) {
    if (oscIndex < 0 || oscIndex >= OSCILLATORS_PER_VOICE) return;
    if (!freeRunningVoiceActive[oscIndex]) return;
    int voiceIndex = freeRunningVoiceIndex[oscIndex];
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) return;

    freeRunningVoiceActive[oscIndex] = false;
    freeRunningVoiceIndex[oscIndex] = -1;

    Voice& voice = voices[voiceIndex];
    voice.forceSilence();
}

void Synth::spawnFreeRunningSamplerVoice(int samplerIndex) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) return;
    if (freeRunningSamplerActive[samplerIndex]) {
        int existingIndex = freeRunningSamplerVoiceIndex[samplerIndex];
        if (existingIndex >= 0 && existingIndex < MAX_VOICES) {
            Voice& voice = voices[existingIndex];
            voice.ampGateTarget = 1.0f;
            voice.ampGateValue = 1.0f;
        }
        return;
    }

    int voiceIndex = allocateFreeRunningVoiceSlot();
    if (voiceIndex == -1) {
        return;
    }

    Voice& voice = voices[voiceIndex];
    voice.forceSilence();
    voice.active = true;
    voice.note = 60;
    voice.velocity = 100;
    voice.noteSource = static_cast<int>(NoteSource::FREE);
    voice.freeRunningOscIndex = -1;
    voice.freeRunningSamplerIndex = samplerIndex;
    voice.startTime = voiceCounter++;

    for (int i = 0; i < SAMPLERS_PER_VOICE; ++i) {
        bool keyMode = (i == samplerIndex) ? false : samplerKeyModes[i];
        voice.samplers[i].setKeyMode(keyMode);
        if (i == samplerIndex) {
            voice.samplers[i].requestRestart();
        } else {
            voice.samplers[i].stopPlayback();
        }
    }

    voice.resetFMHistory();
    for (int ei = 0; ei < 4; ++ei) {
        voice.envelopes[ei].noteOn();
    }
    voice.ampGateTarget = 1.0f;
    voice.ampGateValue = 1.0f;

    freeRunningSamplerActive[samplerIndex] = true;
    freeRunningSamplerVoiceIndex[samplerIndex] = voiceIndex;
}

void Synth::killFreeRunningSamplerVoice(int samplerIndex) {
    if (samplerIndex < 0 || samplerIndex >= SAMPLERS_PER_VOICE) return;
    if (!freeRunningSamplerActive[samplerIndex]) return;
    int voiceIndex = freeRunningSamplerVoiceIndex[samplerIndex];
    if (voiceIndex < 0 || voiceIndex >= MAX_VOICES) return;

    freeRunningSamplerActive[samplerIndex] = false;
    freeRunningSamplerVoiceIndex[samplerIndex] = -1;

    Voice& voice = voices[voiceIndex];
    voice.forceSilence();
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
