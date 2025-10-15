#ifndef MARKOV_H
#define MARKOV_H

#include <vector>
#include <string>

class MarkovChain {
public:
    MarkovChain();

    // Initialize with a set of states (MIDI notes)
    void initialize(const std::vector<int>& notes);

    // Set transition probability from one state to another
    void setTransition(int fromStateIndex, int toStateIndex, float probability);

    // Get next state using weighted random selection
    int getNextState();

    // Get current MIDI note
    int getCurrentNote() const;

    // Set current state by MIDI note value
    void setCurrentNote(int midiNote);

    // Get number of states
    int getStateCount() const { return states.size(); }

    // Get state by index
    int getStateNote(int index) const;

    // Preset transition patterns
    void setRandomWalk();              // Equal probability to neighbors
    void setOrbitingPattern(int centerNote);  // Attracted to center note
    void setAscending(float bias = 0.7f);     // Bias upward movement
    void setDescending(float bias = 0.7f);    // Bias downward movement
    void setDronePattern(float repeatProb = 0.8f);  // High repetition

    // Evolution (self-modifying Markov chains)
    void reinforceLastTransition(float amount);  // Strengthen recently used path
    void decayUnusedTransitions(float rate);     // Weaken rarely used paths
    void normalizeRow(int stateIndex);           // Ensure row sums to 1.0

    // Serialization (future: save/load patterns)
    std::string serializeToJson() const;
    void deserializeFromJson(const std::string& json);

    // Debug
    void printMatrix() const;

private:
    std::vector<int> states;                           // MIDI note values
    std::vector<std::vector<float>> transitionMatrix;  // [from][to] probabilities
    int currentState;                                   // Current state index
    int lastState;                                      // Previous state (for reinforcement)

    // Helper: find state index for a MIDI note
    int findStateIndex(int midiNote) const;

    // Helper: weighted random selection
    int weightedRandomChoice(const std::vector<float>& probabilities) const;
};

#endif // MARKOV_H
