#include "euclidean.h"
#include <algorithm>
#include <string>

EuclideanPattern::EuclideanPattern()
    : hits(4)
    , steps(16)
    , rotation(0)
{
    generate();
}

EuclideanPattern::EuclideanPattern(int h, int s, int r)
    : hits(h)
    , steps(s)
    , rotation(r)
{
    generate();
}

void EuclideanPattern::setHits(int h) {
    hits = std::clamp(h, 0, 64);
    generate();
}

void EuclideanPattern::setSteps(int s) {
    steps = std::clamp(s, 1, 64);
    hits = std::min(hits, steps);  // Can't have more hits than steps
    generate();
}

void EuclideanPattern::setRotation(int r) {
    rotation = r;
    generate();
}

std::vector<bool> EuclideanPattern::bjorklund(int k, int n) const {
    // Bjorklund's algorithm for Euclidean rhythms
    // Distributes k hits evenly across n steps

    if (k == 0 || n == 0) {
        return std::vector<bool>(n, false);
    }

    if (k >= n) {
        return std::vector<bool>(n, true);
    }

    // Initialize: k groups of [1] and (n-k) groups of [0]
    std::vector<std::vector<bool>> groups;
    for (int i = 0; i < k; ++i) {
        groups.push_back({true});
    }
    for (int i = 0; i < n - k; ++i) {
        groups.push_back({false});
    }

    // Repeatedly pair groups until we can't pair anymore
    while (groups.size() > 1) {
        int numLeft = groups.size() - k;
        if (numLeft <= 0) break;

        int pairCount = std::min(k, numLeft);

        // Pair first pairCount groups with last pairCount groups
        for (int i = 0; i < pairCount; ++i) {
            std::vector<bool>& front = groups[i];
            std::vector<bool>& back = groups[groups.size() - 1];
            front.insert(front.end(), back.begin(), back.end());
            groups.pop_back();
        }

        // Update k for next iteration
        k = pairCount;
    }

    // Flatten groups into single pattern
    std::vector<bool> result;
    for (const auto& group : groups) {
        result.insert(result.end(), group.begin(), group.end());
    }

    return result;
}

void EuclideanPattern::generate() {
    // Generate base pattern
    pattern = bjorklund(hits, steps);

    // Apply rotation
    if (rotation != 0 && !pattern.empty()) {
        int rot = rotation % steps;
        if (rot < 0) rot += steps;

        std::rotate(pattern.begin(), pattern.begin() + rot, pattern.end());
    }
}

bool EuclideanPattern::getTrigger(int step) const {
    if (pattern.empty() || steps == 0) return false;

    int index = step % steps;
    if (index < 0) index += steps;

    return pattern[index];
}

std::string EuclideanPattern::toString() const {
    std::string result;
    for (bool hit : pattern) {
        result += hit ? "X " : "· ";
    }
    return result;
}

// Preset patterns for dark ambient
EuclideanPattern EuclideanPattern::createDrone() {
    return EuclideanPattern(16, 16, 0);  // Every step
}

EuclideanPattern EuclideanPattern::createSparse() {
    return EuclideanPattern(3, 32, 0);  // Very sparse
}

EuclideanPattern EuclideanPattern::createPulse() {
    return EuclideanPattern(8, 16, 0);  // Half steps filled
}

EuclideanPattern EuclideanPattern::createBreathing() {
    return EuclideanPattern(5, 16, 0);  // 5/16 rhythm
}

EuclideanPattern EuclideanPattern::createTriplet() {
    return EuclideanPattern(3, 8, 0);  // Triplet feel
}

EuclideanPattern EuclideanPattern::createQuintuplet() {
    return EuclideanPattern(5, 8, 0);  // Quintuplet
}

EuclideanPattern EuclideanPattern::createAudioRate() {
    return EuclideanPattern(16, 16, 0);  // Every step (use with fast tempo)
}
