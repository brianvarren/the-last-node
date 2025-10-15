#include "markov.h"
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <numeric>

MarkovChain::MarkovChain()
    : currentState(0)
    , lastState(0)
{
}

void MarkovChain::initialize(const std::vector<int>& notes) {
    states = notes;
    int n = states.size();

    // Initialize transition matrix with equal probabilities
    transitionMatrix.clear();
    transitionMatrix.resize(n, std::vector<float>(n, 1.0f / n));

    currentState = 0;
    lastState = 0;
}

void MarkovChain::setTransition(int fromStateIndex, int toStateIndex, float probability) {
    if (fromStateIndex >= 0 && fromStateIndex < static_cast<int>(states.size()) &&
        toStateIndex >= 0 && toStateIndex < static_cast<int>(states.size())) {
        transitionMatrix[fromStateIndex][toStateIndex] = probability;
    }
}

int MarkovChain::findStateIndex(int midiNote) const {
    auto it = std::find(states.begin(), states.end(), midiNote);
    if (it != states.end()) {
        return std::distance(states.begin(), it);
    }
    return 0;  // Default to first state
}

int MarkovChain::getCurrentNote() const {
    if (currentState >= 0 && currentState < static_cast<int>(states.size())) {
        return states[currentState];
    }
    return 60;  // Default middle C
}

void MarkovChain::setCurrentNote(int midiNote) {
    currentState = findStateIndex(midiNote);
}

int MarkovChain::getStateNote(int index) const {
    if (index >= 0 && index < static_cast<int>(states.size())) {
        return states[index];
    }
    return 60;
}

int MarkovChain::weightedRandomChoice(const std::vector<float>& probabilities) const {
    if (probabilities.empty()) return 0;

    // Generate random value [0, 1)
    float r = static_cast<float>(rand()) / RAND_MAX;

    // Accumulate probabilities until we exceed r
    float cumulative = 0.0f;
    for (size_t i = 0; i < probabilities.size(); ++i) {
        cumulative += probabilities[i];
        if (r < cumulative) {
            return i;
        }
    }

    return probabilities.size() - 1;  // Fallback to last state
}

int MarkovChain::getNextState() {
    if (states.empty()) return 0;

    lastState = currentState;

    // Get probabilities for current state
    const std::vector<float>& probs = transitionMatrix[currentState];

    // Choose next state weighted by probabilities
    currentState = weightedRandomChoice(probs);

    return currentState;
}

void MarkovChain::normalizeRow(int stateIndex) {
    if (stateIndex < 0 || stateIndex >= static_cast<int>(states.size())) return;

    float sum = std::accumulate(transitionMatrix[stateIndex].begin(),
                                 transitionMatrix[stateIndex].end(), 0.0f);

    if (sum > 0.0001f) {
        for (float& prob : transitionMatrix[stateIndex]) {
            prob /= sum;
        }
    } else {
        // If sum is zero, reset to uniform distribution
        float uniform = 1.0f / states.size();
        for (float& prob : transitionMatrix[stateIndex]) {
            prob = uniform;
        }
    }
}

void MarkovChain::setRandomWalk() {
    int n = states.size();
    if (n == 0) return;

    // Equal probability to move to any neighbor (within 3 steps)
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            int distance = std::abs(i - j);
            if (distance == 0) {
                transitionMatrix[i][j] = 0.3f;  // 30% stay
            } else if (distance <= 3) {
                transitionMatrix[i][j] = 0.7f / std::min(6, n - 1);  // Distribute to neighbors
            } else {
                transitionMatrix[i][j] = 0.0f;  // Can't jump far
            }
        }
        normalizeRow(i);
    }
}

void MarkovChain::setOrbitingPattern(int centerNote) {
    int n = states.size();
    if (n == 0) return;

    int centerIndex = findStateIndex(centerNote);

    // Probabilities increase as we get closer to center note
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            int distanceFromCenter = std::abs(j - centerIndex);
            int currentDistance = std::abs(i - centerIndex);

            // Prefer moving toward center
            if (distanceFromCenter < currentDistance) {
                transitionMatrix[i][j] = 0.4f;
            } else if (distanceFromCenter == currentDistance) {
                transitionMatrix[i][j] = 0.3f;  // Stay at same distance
            } else {
                transitionMatrix[i][j] = 0.1f;  // Moving away is rare
            }

            // Boost probability for center note itself
            if (j == centerIndex) {
                transitionMatrix[i][j] *= 1.5f;
            }
        }
        normalizeRow(i);
    }
}

void MarkovChain::setAscending(float bias) {
    int n = states.size();
    if (n == 0) return;

    // Bias toward higher notes
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (j > i) {
                // Moving up
                transitionMatrix[i][j] = bias / std::max(1, n - i - 1);
            } else if (j == i) {
                // Staying
                transitionMatrix[i][j] = (1.0f - bias) * 0.5f;
            } else {
                // Moving down
                transitionMatrix[i][j] = (1.0f - bias) * 0.5f / std::max(1, i);
            }
        }
        normalizeRow(i);
    }
}

void MarkovChain::setDescending(float bias) {
    int n = states.size();
    if (n == 0) return;

    // Bias toward lower notes
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (j < i) {
                // Moving down
                transitionMatrix[i][j] = bias / std::max(1, i);
            } else if (j == i) {
                // Staying
                transitionMatrix[i][j] = (1.0f - bias) * 0.5f;
            } else {
                // Moving up
                transitionMatrix[i][j] = (1.0f - bias) * 0.5f / std::max(1, n - i - 1);
            }
        }
        normalizeRow(i);
    }
}

void MarkovChain::setDronePattern(float repeatProb) {
    int n = states.size();
    if (n == 0) return;

    // High probability of repeating, low probability of moving
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i == j) {
                transitionMatrix[i][j] = repeatProb;
            } else {
                // Distribute remaining probability to neighbors
                int distance = std::abs(i - j);
                if (distance <= 2) {
                    transitionMatrix[i][j] = (1.0f - repeatProb) / std::min(4, n - 1);
                } else {
                    transitionMatrix[i][j] = 0.0f;
                }
            }
        }
        normalizeRow(i);
    }
}

void MarkovChain::reinforceLastTransition(float amount) {
    if (lastState >= 0 && lastState < static_cast<int>(states.size()) &&
        currentState >= 0 && currentState < static_cast<int>(states.size())) {

        transitionMatrix[lastState][currentState] += amount;
        normalizeRow(lastState);
    }
}

void MarkovChain::decayUnusedTransitions(float rate) {
    // Multiply all probabilities by (1 - rate), then normalize
    for (int i = 0; i < static_cast<int>(states.size()); ++i) {
        for (int j = 0; j < static_cast<int>(states.size()); ++j) {
            transitionMatrix[i][j] *= (1.0f - rate);
        }
        normalizeRow(i);
    }
}

void MarkovChain::printMatrix() const {
    std::cout << "Markov Chain Transition Matrix:\n";
    std::cout << "States: ";
    for (int note : states) {
        std::cout << note << " ";
    }
    std::cout << "\n";

    for (size_t i = 0; i < transitionMatrix.size(); ++i) {
        std::cout << "From " << states[i] << ": ";
        for (float prob : transitionMatrix[i]) {
            std::cout << prob << " ";
        }
        std::cout << "\n";
    }
}

std::string MarkovChain::serializeToJson() const {
    // TODO: Implement JSON serialization
    return "{}";
}

void MarkovChain::deserializeFromJson(const std::string& json) {
    // TODO: Implement JSON deserialization
    (void)json;
}
