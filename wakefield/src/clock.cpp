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
    , loopSubdivision(Subdivisions::SIXTEENTH)
{
    // Initialize samples per beat (quarter note baseline)
    samplesPerBeat = (60.0 / tempo) * sampleRate;
}

void Clock::reset() {
    sampleCounter = 0;
}

void Clock::setTempo(double bpm) {
    tempo = std::clamp(bpm, 0.1, 999.0);
    samplesPerBeat = (60.0 / tempo) * sampleRate;
}

void Clock::advance(unsigned int nFrames) {
    if (!playing || externalSync) {
        return;
    }

    sampleCounter += nFrames;
}

double Clock::getSamplesPerStep(Subdivision subdiv) const {
    // Subdivision is a tempo multiplier relative to quarter note:
    //   4.0 = 1/16 note (4x faster) = 0.25 beats per step
    //   1.0 = 1/4 note (baseline) = 1.0 beats per step
    //   0.25 = whole note (0.25x) = 4.0 beats per step
    // beatsPerStep = 1.0 / subdivisionMultiplier
    double beatsPerStep = 1.0 / subdiv;
    return samplesPerBeat * beatsPerStep;
}

bool Clock::checkStepTrigger(unsigned int nFrames, Subdivision subdiv, int& stepIndex) {
    if (!playing) {
        return false;
    }

    double samplesPerStep = getSamplesPerStep(subdiv);

    uint64_t oldSample = sampleCounter - nFrames;
    uint64_t newSample = sampleCounter;

    // Calculate step indices
    int oldStep = static_cast<int>(oldSample / samplesPerStep);
    int newStep = static_cast<int>(newSample / samplesPerStep);

    // Did we cross a step boundary?
    if (newStep > oldStep) {
        stepIndex = newStep;

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
