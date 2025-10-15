#ifndef EUCLIDEAN_H
#define EUCLIDEAN_H

#include <vector>
#include <string>

// Euclidean rhythm generator using Bjorklund's algorithm
// Distributes N hits evenly across M steps
class EuclideanPattern {
public:
    EuclideanPattern();
    EuclideanPattern(int hits, int steps, int rotation = 0);

    // Configuration
    void setHits(int h);
    void setSteps(int s);
    void setRotation(int r);

    int getHits() const { return hits; }
    int getSteps() const { return steps; }
    int getRotation() const { return rotation; }

    // Generate pattern using Bjorklund's algorithm
    void generate();

    // Query pattern
    bool getTrigger(int step) const;
    const std::vector<bool>& getPattern() const { return pattern; }

    // Get pattern as string for display (X = hit, . = rest)
    std::string toString() const;

    // Preset patterns for dark ambient
    static EuclideanPattern createDrone();         // Every step (16/16)
    static EuclideanPattern createSparse();        // Very sparse (3/32)
    static EuclideanPattern createPulse();         // Throbbing (8/16)
    static EuclideanPattern createBreathing();     // Slow pulse (5/16)
    static EuclideanPattern createTriplet();       // 3/8
    static EuclideanPattern createQuintuplet();    // 5/8
    static EuclideanPattern createAudioRate();     // Near audio rate (16/16 but fast tempo)

private:
    int hits;        // Number of triggers
    int steps;       // Total pattern length
    int rotation;    // Rotate pattern by N steps
    std::vector<bool> pattern;  // Generated trigger mask

    // Bjorklund's algorithm implementation
    std::vector<bool> bjorklund(int hits, int steps) const;
};

#endif // EUCLIDEAN_H
