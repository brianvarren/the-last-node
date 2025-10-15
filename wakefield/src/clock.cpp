#include "clock.h"
#include <algorithm>

Clock::Clock(float sampleRate)
    : sampleRate(sampleRate)
    , tempo(120.0)
    , sampleCounter(0)
    , playing(false)
    , loopEnabled(false)
    , externalSync(false)
    , loopStartStep(0)
    , loopEndStep(16)
    , loopSubdivision(Subdivision::SIXTEENTH)
{
    // Initialize samples per beat
    samplesPerBeat = (60.0 / tempo) * sampleRate;

    // Initialize last step samples
    for (int i = 0; i < 7; ++i) {
        lastStepSample[i] = 0;
    }
}

void Clock::reset() {
    sampleCounter = 0;
    for (int i = 0; i < 7; ++i) {
        lastStepSample[i] = 0;
    }
}

void Clock::setTempo(double bpm) {
    tempo = std::clamp(bpm, 20.0, 300.0);
    samplesPerBeat = (60.0 / tempo) * sampleRate;
}

void Clock::advance(unsigned int nFrames) {
    if (!playing || externalSync) {
        return;
    }

    sampleCounter += nFrames;
}

int Clock::getSubdivIndex(Subdivision subdiv) const {
    // Map subdivision to array index (0-6)
    switch (subdiv) {
        case Subdivision::WHOLE: return 0;
        case Subdivision::HALF: return 1;
        case Subdivision::QUARTER: return 2;
        case Subdivision::EIGHTH: return 3;
        case Subdivision::SIXTEENTH: return 4;
        case Subdivision::THIRTYSECOND: return 5;
        case Subdivision::SIXTYFOURTH: return 6;
    }
    return 2; // Default to quarter note
}

double Clock::getSamplesPerStep(Subdivision subdiv) const {
    // Quarter note = 1 beat
    // Whole = 4 beats, Half = 2 beats, Eighth = 0.5 beats, etc.
    double beatsPerStep = 4.0 / subdivisionToInt(subdiv);
    return samplesPerBeat * beatsPerStep;
}

bool Clock::checkStepTrigger(unsigned int nFrames, Subdivision subdiv, int& stepIndex) {
    if (!playing) {
        return false;
    }

    int idx = getSubdivIndex(subdiv);
    double samplesPerStep = getSamplesPerStep(subdiv);

    uint64_t oldSample = sampleCounter - nFrames;
    uint64_t newSample = sampleCounter;

    // Calculate step indices
    int oldStep = static_cast<int>(oldSample / samplesPerStep);
    int newStep = static_cast<int>(newSample / samplesPerStep);

    // Did we cross a step boundary?
    if (newStep > oldStep) {
        stepIndex = newStep;
        lastStepSample[idx] = newStep * samplesPerStep;

        // Handle looping
        if (loopEnabled && subdiv == loopSubdivision) {
            int loopLength = loopEndStep - loopStartStep;
            if (loopLength > 0) {
                int relativeStep = (stepIndex - loopStartStep) % loopLength;
                if (relativeStep < 0) relativeStep += loopLength;
                stepIndex = loopStartStep + relativeStep;
            }
        }

        return true;
    }

    return false;
}

double Clock::getPhase(Subdivision subdiv) const {
    double samplesPerStep = getSamplesPerStep(subdiv);
    double phase = fmod(static_cast<double>(sampleCounter), samplesPerStep) / samplesPerStep;
    return phase;
}

int Clock::getCurrentStep(Subdivision subdiv) const {
    double samplesPerStep = getSamplesPerStep(subdiv);
    return static_cast<int>(sampleCounter / samplesPerStep);
}

void Clock::setLoopPoints(int startStep, int endStep, Subdivision subdiv) {
    loopStartStep = startStep;
    loopEndStep = endStep;
    loopSubdivision = subdiv;
}
