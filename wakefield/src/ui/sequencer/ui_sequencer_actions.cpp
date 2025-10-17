#include "../../ui.h"
#include "../../sequencer.h"
#include "../../loop_manager.h"
#include "pattern.h"
#include <algorithm>
#include <vector>
#include <cstdlib>

// External references to global objects from main.cpp
extern LoopManager* loopManager;
extern Sequencer* sequencer;

void UI::executeSequencerAction(int actionRow, int actionColumn) {
    if (!sequencer) return;

    Pattern& pattern = sequencer->getPattern();
    float mutateAmount = sequencerMutateAmount / 100.0f;  // Convert percentage to 0-1

    // actionRow: 0=Generate, 1=Clear, 2=Randomize, 3=Mutate, 4=Rotate
    // actionColumn: 0=All, 1=Note, 2=Vel, 3=Gate, 4=Prob

    if (actionRow == 0) {
        // Generate - global regeneration
        sequencer->generatePattern();
        return;
    }

    if (actionRow == 1) {
        // Clear - deactivate all steps
        for (int i = 0; i < pattern.getLength(); ++i) {
            if (!pattern.getStep(i).locked) {
                pattern.getStep(i).active = false;
            }
        }
        return;
    }

    // For other actions, determine target fields
    bool affectNote = (actionColumn == 0 || actionColumn == 1);
    bool affectVel = (actionColumn == 0 || actionColumn == 2);
    bool affectGate = (actionColumn == 0 || actionColumn == 3);
    bool affectProb = (actionColumn == 0 || actionColumn == 4);

    for (int i = 0; i < pattern.getLength(); ++i) {
        PatternStep& step = pattern.getStep(i);
        if (step.locked) continue;  // Skip locked steps

        switch (actionRow) {
            case 2: {  // Randomize
                if (affectNote && step.active) {
                    step.midiNote = rand() % 128;
                }
                if (affectVel && step.active) {
                    step.velocity = 1 + (rand() % 127);
                }
                if (affectGate && step.active) {
                    step.gateLength = 0.05f + (static_cast<float>(rand()) / RAND_MAX) * 1.95f;
                }
                if (affectProb && step.active) {
                    step.probability = static_cast<float>(rand()) / RAND_MAX;
                }
                break;
            }
            case 3: {  // Mutate
                if (affectNote && step.active) {
                    int delta = static_cast<int>((rand() % 25) - 12) * mutateAmount;
                    step.midiNote = std::clamp(step.midiNote + delta, 0, 127);
                }
                if (affectVel && step.active) {
                    int delta = static_cast<int>((rand() % 41) - 20) * mutateAmount;
                    step.velocity = std::clamp(step.velocity + delta, 1, 127);
                }
                if (affectGate && step.active) {
                    float delta = ((static_cast<float>(rand()) / RAND_MAX) * 0.4f - 0.2f) * mutateAmount;
                    step.gateLength = std::max(0.05f, std::min(2.0f, step.gateLength + delta));
                }
                if (affectProb && step.active) {
                    float delta = ((static_cast<float>(rand()) / RAND_MAX) * 0.4f - 0.2f) * mutateAmount;
                    step.probability = std::max(0.0f, std::min(1.0f, step.probability + delta));
                }
                break;
            }
            case 4: {  // Rotate
                // Handled after loop to rotate specific fields
                break;
            }
        }
    }

    // Handle rotation separately - only rotate among active steps
    if (actionRow == 4) {
        if (actionColumn == 0) {
            // Rotate all - use built-in pattern rotation
            sequencer->rotatePattern(1);
        } else {
            // Rotate specific field(s) - only among active, unlocked steps
            int len = pattern.getLength();
            if (len < 2) return;

            // Collect indices of active, unlocked steps
            std::vector<int> activeIndices;
            for (int i = 0; i < len; ++i) {
                if (pattern.getStep(i).active && !pattern.getStep(i).locked) {
                    activeIndices.push_back(i);
                }
            }

            if (activeIndices.size() < 2) return;  // Need at least 2 active steps to rotate

            // Rotate values among active steps only
            if (affectNote) {
                int lastNote = pattern.getStep(activeIndices.back()).midiNote;
                for (int i = static_cast<int>(activeIndices.size()) - 1; i > 0; --i) {
                    pattern.getStep(activeIndices[i]).midiNote = pattern.getStep(activeIndices[i - 1]).midiNote;
                }
                pattern.getStep(activeIndices[0]).midiNote = lastNote;
            }
            if (affectVel) {
                int lastVel = pattern.getStep(activeIndices.back()).velocity;
                for (int i = static_cast<int>(activeIndices.size()) - 1; i > 0; --i) {
                    pattern.getStep(activeIndices[i]).velocity = pattern.getStep(activeIndices[i - 1]).velocity;
                }
                pattern.getStep(activeIndices[0]).velocity = lastVel;
            }
            if (affectGate) {
                float lastGate = pattern.getStep(activeIndices.back()).gateLength;
                for (int i = static_cast<int>(activeIndices.size()) - 1; i > 0; --i) {
                    pattern.getStep(activeIndices[i]).gateLength = pattern.getStep(activeIndices[i - 1]).gateLength;
                }
                pattern.getStep(activeIndices[0]).gateLength = lastGate;
            }
            if (affectProb) {
                float lastProb = pattern.getStep(activeIndices.back()).probability;
                for (int i = static_cast<int>(activeIndices.size()) - 1; i > 0; --i) {
                    pattern.getStep(activeIndices[i]).probability = pattern.getStep(activeIndices[i - 1]).probability;
                }
                pattern.getStep(activeIndices[0]).probability = lastProb;
            }
        }
    }
}
