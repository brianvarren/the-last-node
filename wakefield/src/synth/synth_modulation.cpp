// ============================================================================
// SYNTH MODULATION - LFO and Chaos Generator Management
// ============================================================================
//
// RESPONSIBILITIES:
// - LFO parameter updates: updateLFOParameters() for period, shape, sync, etc.
// - LFO processing: processLFOs() generates per-buffer LFO outputs
// - Chaos processing: processChaos() generates chaotic modulation sources
// - Modulation getters: getLFOOutput(), getChaosOutput() for modulation matrix
// - Audio-rate chaos access: getChaosOutputAtFrame() for sample-accurate FM
//
// DOES NOT CONTAIN:
// - LFO DSP implementation (see lfo.cpp)
// - Chaos DSP implementation (see chaos.h)
// - Modulation routing/matrix (see modulation.cpp)
// - Audio rendering (see synth_process.cpp)
//
// ============================================================================

#include "../synth.h"
#include "../ui.h"
#include "../clock.h"
#include <algorithm>

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

