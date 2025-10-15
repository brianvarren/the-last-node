#include "pattern.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

Pattern::Pattern(int len, Subdivision res)
    : length(len)
    , resolution(res)
    , rotation(0)
{
    steps.resize(length);
}

void Pattern::setLength(int len) {
    length = std::clamp(len, 1, 256);
    steps.resize(length);
}

PatternStep& Pattern::getStep(int index) {
    int idx = (index + rotation) % length;
    if (idx < 0) idx += length;
    return steps[idx];
}

const PatternStep& Pattern::getStep(int index) const {
    int idx = (index + rotation) % length;
    if (idx < 0) idx += length;
    return steps[idx];
}

void Pattern::generateStep(int stepIndex, MusicalConstraints& constraints,
                           MarkovChain& markov, bool euclideanTrigger) {
    PatternStep& s = steps[stepIndex];

    // Set active state from Euclidean pattern
    s.active = euclideanTrigger;

    if (s.active) {
        // Get next note from Markov chain
        markov.getNextState();
        s.midiNote = markov.getCurrentNote();

        // Quantize to scale (in case Markov drifted)
        s.midiNote = constraints.quantizeToScale(s.midiNote);

        // Generate velocity with variation (70-100)
        s.velocity = 70 + (rand() % 31);

        // Default gate length (80% of step)
        s.gateLength = 0.7f + (rand() % 30) / 100.0f;  // 0.7-1.0

        // Default probability (usually 100%, but add some variation)
        float densityFactor = constraints.getDensity();
        if (densityFactor < 0.8f) {
            s.probability = 0.8f + (rand() % 21) / 100.0f;  // 0.8-1.0
        } else {
            s.probability = 1.0f;
        }

        // Clear parameter automation (user can set these manually)
        s.filterCutoff = -1.0f;
        s.reverbMix = -1.0f;
        s.brainwaveMorph = -1.0f;
    }
}

void Pattern::generateFromConstraints(
    MusicalConstraints& constraints,
    MarkovChain& markov,
    const EuclideanPattern& rhythm)
{
    // Generate pattern from scratch
    for (int step = 0; step < length; ++step) {
        // Skip locked steps
        if (steps[step].locked) continue;

        bool trigger = rhythm.getTrigger(step);
        generateStep(step, constraints, markov, trigger);
    }
}

void Pattern::regenerateUnlocked(
    MusicalConstraints& constraints,
    MarkovChain& markov,
    const EuclideanPattern& rhythm)
{
    // Only regenerate unlocked steps
    for (int step = 0; step < length; ++step) {
        if (steps[step].locked) continue;

        bool trigger = rhythm.getTrigger(step);
        generateStep(step, constraints, markov, trigger);
    }
}

void Pattern::mutate(float amount) {
    // Mutate unlocked steps slightly
    amount = std::clamp(amount, 0.0f, 1.0f);

    for (int step = 0; step < length; ++step) {
        if (steps[step].locked || !steps[step].active) continue;

        // Randomly mutate based on amount
        if ((rand() % 100) / 100.0f < amount) {
            // Mutate note (move by small interval)
            int shift = (rand() % 5) - 2;  // -2 to +2 semitones
            steps[step].midiNote = std::clamp(steps[step].midiNote + shift, 0, 127);
        }

        if ((rand() % 100) / 100.0f < amount * 0.5f) {
            // Mutate velocity slightly
            int shift = (rand() % 21) - 10;  // -10 to +10
            steps[step].velocity = std::clamp(steps[step].velocity + shift, 1, 127);
        }

        if ((rand() % 100) / 100.0f < amount * 0.3f) {
            // Mutate probability
            float shift = ((rand() % 21) - 10) / 100.0f;  // -0.1 to +0.1
            steps[step].probability = std::clamp(steps[step].probability + shift, 0.0f, 1.0f);
        }
    }
}

void Pattern::rotate(int steps) {
    rotation = (rotation + steps) % length;
    if (rotation < 0) rotation += length;
}

void Pattern::lockStep(int step) {
    if (step >= 0 && step < length) {
        steps[step].locked = true;
    }
}

void Pattern::unlockStep(int step) {
    if (step >= 0 && step < length) {
        steps[step].locked = false;
    }
}

void Pattern::toggleLock(int step) {
    if (step >= 0 && step < length) {
        steps[step].locked = !steps[step].locked;
    }
}

bool Pattern::isStepLocked(int step) const {
    if (step >= 0 && step < length) {
        return steps[step].locked;
    }
    return false;
}

void Pattern::setNote(int step, int midiNote) {
    if (step >= 0 && step < length) {
        steps[step].midiNote = std::clamp(midiNote, 0, 127);
    }
}

void Pattern::setVelocity(int step, int velocity) {
    if (step >= 0 && step < length) {
        steps[step].velocity = std::clamp(velocity, 1, 127);
    }
}

void Pattern::setGateLength(int step, float gate) {
    if (step >= 0 && step < length) {
        steps[step].gateLength = std::clamp(gate, 0.0f, 2.0f);
    }
}

void Pattern::setProbability(int step, float prob) {
    if (step >= 0 && step < length) {
        steps[step].probability = std::clamp(prob, 0.0f, 1.0f);
    }
}

void Pattern::clear() {
    for (auto& step : steps) {
        step.active = false;
        step.locked = false;
        step.midiNote = 60;
        step.velocity = 80;
        step.gateLength = 0.8f;
        step.probability = 1.0f;
        step.filterCutoff = -1.0f;
        step.reverbMix = -1.0f;
        step.brainwaveMorph = -1.0f;
    }
}

void Pattern::randomizeVelocities(int minVel, int maxVel) {
    for (auto& step : steps) {
        if (step.active && !step.locked) {
            step.velocity = minVel + (rand() % (maxVel - minVel + 1));
        }
    }
}

void Pattern::randomizeProbabilities(float minProb, float maxProb) {
    for (auto& step : steps) {
        if (step.active && !step.locked) {
            float range = maxProb - minProb;
            step.probability = minProb + (rand() % 101) / 100.0f * range;
        }
    }
}

int Pattern::getActiveStepCount() const {
    int count = 0;
    for (const auto& step : steps) {
        if (step.active) count++;
    }
    return count;
}

int Pattern::getLockedStepCount() const {
    int count = 0;
    for (const auto& step : steps) {
        if (step.locked) count++;
    }
    return count;
}

std::string Pattern::getPatternString() const {
    std::string result;
    for (const auto& step : steps) {
        if (step.locked) {
            result += "🔒";
        } else if (step.active) {
            result += "X ";
        } else {
            result += "· ";
        }
    }
    return result;
}

void Pattern::saveToFile(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return;

    // Simple CSV format
    file << "step,active,locked,note,velocity,gate,probability\n";
    for (int i = 0; i < length; ++i) {
        const auto& s = steps[i];
        file << i << ","
             << s.active << ","
             << s.locked << ","
             << s.midiNote << ","
             << s.velocity << ","
             << s.gateLength << ","
             << s.probability << "\n";
    }

    file.close();
}

void Pattern::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    std::getline(file, line);  // Skip header

    int stepIndex = 0;
    while (std::getline(file, line) && stepIndex < length) {
        std::stringstream ss(line);
        std::string token;

        std::getline(ss, token, ',');  // step index (ignore)

        std::getline(ss, token, ',');
        steps[stepIndex].active = (token == "1");

        std::getline(ss, token, ',');
        steps[stepIndex].locked = (token == "1");

        std::getline(ss, token, ',');
        steps[stepIndex].midiNote = std::stoi(token);

        std::getline(ss, token, ',');
        steps[stepIndex].velocity = std::stoi(token);

        std::getline(ss, token, ',');
        steps[stepIndex].gateLength = std::stof(token);

        std::getline(ss, token, ',');
        steps[stepIndex].probability = std::stof(token);

        stepIndex++;
    }

    file.close();
}
