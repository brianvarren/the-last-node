#include "synth.h"
#include <iostream>

Synth::Synth(float sampleRate) 
    : sampleRate(sampleRate)
    , masterVolume(0.5f)
    , reverbEnabled(false)
    , filterEnabled(false)
    , currentFilterType(0)
    , reverb(sampleRate)
    , filterL(sampleRate)
    , filterR(sampleRate) {
    
    // Initialize shelf filters
    highShelfL.setSampleRate(sampleRate);
    highShelfR.setSampleRate(sampleRate);
    lowShelfL.setSampleRate(sampleRate);
    lowShelfR.setSampleRate(sampleRate);
    
    // Initialize voices with sample rate
    for (int i = 0; i < MAX_VOICES; ++i) {
        voices.emplace_back(sampleRate);
    }
}

float Synth::midiNoteToFrequency(int midiNote) {
    // MIDI note 69 = A4 = 440 Hz
    // Formula: f = 440 * 2^((n-69)/12)
    return 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
}

int Synth::findFreeVoice() {
    // Find first inactive voice
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!voices[i].active) {
            return i;
        }
    }
    
    // No free voice found
    return -1;
}

void Synth::updateEnvelopeParameters(float attack, float decay, float sustain, float release) {
    // Update all voice envelopes with new parameters
    for (auto& voice : voices) {
        voice.envelope.setAttack(attack);
        voice.envelope.setDecay(decay);
        voice.envelope.setSustain(sustain);
        voice.envelope.setRelease(release);
    }
}

void Synth::setWaveform(Waveform waveform) {
    // Update all voice oscillators with new waveform
    for (auto& voice : voices) {
        voice.oscillator.setWaveform(waveform);
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

void Synth::updateFilterParameters(int type, float cutoff, float gain) {
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
}

void Synth::noteOn(int midiNote, int velocity) {
    // Find a free voice
    int voiceIndex = findFreeVoice();
    
    if (voiceIndex == -1) {
        // No free voices - drop the note for now
        return;
    }
    
    // Activate the voice
    Voice& voice = voices[voiceIndex];
    voice.active = true;
    voice.note = midiNote;
    
    float frequency = midiNoteToFrequency(midiNote);
    voice.oscillator.setFrequency(frequency);
    voice.oscillator.setPhase(0.0f);  // Reset phase for new note
    
    voice.envelope.noteOn();  // Trigger envelope attack
}

void Synth::noteOff(int midiNote) {
    // Find all voices playing this note and trigger their release
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].active && voices[i].note == midiNote) {
            voices[i].envelope.noteOff();  // Trigger release
        }
    }
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

void Synth::process(float* output, unsigned int nFrames, unsigned int nChannels) {
    // Clear the output buffer first
    for (unsigned int i = 0; i < nFrames * nChannels; ++i) {
        output[i] = 0.0f;
    }
    
    // Process each active voice and mix into output
    for (int v = 0; v < MAX_VOICES; ++v) {
        if (!voices[v].active) {
            continue;
        }
        
        // Generate samples for this voice
        for (unsigned int i = 0; i < nFrames; ++i) {
            float sample = voices[v].generateSample();
            
            // Mix into all channels with master volume
            // Scale by 0.5 to prevent clipping when multiple voices play
            for (unsigned int ch = 0; ch < nChannels; ++ch) {
                output[i * nChannels + ch] += sample * 0.5f * masterVolume;
            }
        }
    }
    
    // Apply filter if enabled (stereo processing)
    if (filterEnabled && nChannels == 2) {
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
}