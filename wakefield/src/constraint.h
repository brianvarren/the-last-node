#ifndef CONSTRAINT_H
#define CONSTRAINT_H

#include <vector>
#include <string>

// Dark ambient scales perfect for drone/atmospheric music
enum class Scale {
    CHROMATIC = 0,      // All 12 notes
    MINOR_NATURAL = 1,  // W-H-W-W-H-W-W (natural minor)
    HARMONIC_MINOR = 2, // W-H-W-W-H-WH-H (raised 7th)
    PHRYGIAN = 3,       // H-W-W-W-H-W-W (dark Spanish flavor)
    LOCRIAN = 4,        // H-W-W-H-W-W-W (most unstable/dark)
    DORIAN = 5,         // W-H-W-W-W-H-W (mysterious, jazzy)
    WHOLE_TONE = 6,     // W-W-W-W-W-W (dreamy, dissonant)
    DIMINISHED = 7,     // W-H-W-H-W-H-W-H (symmetrical, tense)
    PENTATONIC_MINOR = 8, // Minor pentatonic (safe, droney)
    CUSTOM = 9          // User-defined scale
};

// Melodic contour modes
enum class Contour {
    RANDOM_WALK = 0,    // Up/down by small intervals
    ASCENDING = 1,      // Tendency upward
    DESCENDING = 2,     // Tendency downward
    ORBITING = 3,       // Stay near a center pitch
    DRONE = 4           // Repeat same note heavily
};

class MusicalConstraints {
public:
    MusicalConstraints();

    // Scale configuration
    void setScale(Scale scale) { currentScale = scale; }
    Scale getScale() const { return currentScale; }

    void setRootNote(int root) { rootNote = root % 12; }  // 0=C, 1=C#, 2=D, etc.
    int getRootNote() const { return rootNote; }

    void setOctaveRange(int minOct, int maxOct);
    int getOctaveMin() const { return octaveMin; }
    int getOctaveMax() const { return octaveMax; }

    // Custom scale (12 bools for each semitone)
    void setCustomScale(const std::vector<bool>& scale);

    // Melodic contour
    void setContour(Contour contour) { currentContour = contour; }
    Contour getContour() const { return currentContour; }

    void setGravityNote(int midiNote) { gravityNote = midiNote; }
    int getGravityNote() const { return gravityNote; }

    // Density (how many steps filled)
    void setDensity(float density);  // 0.0-1.0
    float getDensity() const { return density; }

    // Max interval jump (semitones, for smooth melodies)
    void setMaxInterval(int semitones) { maxInterval = semitones; }
    int getMaxInterval() const { return maxInterval; }

    // Query functions
    std::vector<int> getLegalNotes() const;
    bool isNoteInScale(int midiNote) const;
    int quantizeToScale(int midiNote) const;  // Snap to nearest scale note

    // Get scale interval pattern (semitones from root)
    std::vector<int> getScaleIntervals() const;

    // Get constrained next note given current note and contour
    int getConstrainedNextNote(int currentNote) const;

    // Scale name for UI display
    std::string getScaleName() const;

private:
    Scale currentScale;
    int rootNote;           // 0-11 (C-B)
    int octaveMin;          // e.g., 2 (C2 = MIDI 36)
    int octaveMax;          // e.g., 5 (C5 = MIDI 72)
    float density;          // 0.0-1.0
    int maxInterval;        // Max jump in semitones

    Contour currentContour;
    int gravityNote;        // Center pitch for ORBITING mode

    std::vector<bool> customScale;  // 12 bools for custom scale

    // Helper to get scale intervals from root
    std::vector<int> getIntervalsForScale(Scale scale) const;
};

#endif // CONSTRAINT_H
