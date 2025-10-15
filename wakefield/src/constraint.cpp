#include "constraint.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

MusicalConstraints::MusicalConstraints()
    : currentScale(Scale::PHRYGIAN)
    , rootNote(0)  // C
    , octaveMin(2)
    , octaveMax(5)
    , density(0.6f)
    , maxInterval(7)
    , currentContour(Contour::RANDOM_WALK)
    , gravityNote(60)  // Middle C
    , customScale(12, false)
{
}

void MusicalConstraints::setOctaveRange(int minOct, int maxOct) {
    octaveMin = std::clamp(minOct, 0, 9);
    octaveMax = std::clamp(maxOct, octaveMin, 9);
}

void MusicalConstraints::setDensity(float d) {
    density = std::clamp(d, 0.0f, 1.0f);
}

void MusicalConstraints::setCustomScale(const std::vector<bool>& scale) {
    if (scale.size() == 12) {
        customScale = scale;
    }
}

std::vector<int> MusicalConstraints::getIntervalsForScale(Scale scale) const {
    // Return semitone intervals from root (0-11)
    switch (scale) {
        case Scale::CHROMATIC:
            return {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

        case Scale::MINOR_NATURAL:
            // W-H-W-W-H-W-W (2-1-2-2-1-2-2)
            return {0, 2, 3, 5, 7, 8, 10};

        case Scale::HARMONIC_MINOR:
            // W-H-W-W-H-WH-H (2-1-2-2-1-3-1)
            return {0, 2, 3, 5, 7, 8, 11};

        case Scale::PHRYGIAN:
            // H-W-W-W-H-W-W (1-2-2-2-1-2-2)
            return {0, 1, 3, 5, 7, 8, 10};

        case Scale::LOCRIAN:
            // H-W-W-H-W-W-W (1-2-2-1-2-2-2)
            return {0, 1, 3, 5, 6, 8, 10};

        case Scale::DORIAN:
            // W-H-W-W-W-H-W (2-1-2-2-2-1-2)
            return {0, 2, 3, 5, 7, 9, 10};

        case Scale::WHOLE_TONE:
            // W-W-W-W-W-W (2-2-2-2-2-2)
            return {0, 2, 4, 6, 8, 10};

        case Scale::DIMINISHED:
            // W-H-W-H-W-H-W-H (2-1-2-1-2-1-2-1)
            return {0, 2, 3, 5, 6, 8, 9, 11};

        case Scale::PENTATONIC_MINOR:
            // Minor pentatonic (root, m3, P4, P5, m7)
            return {0, 3, 5, 7, 10};

        case Scale::CUSTOM:
            {
                std::vector<int> intervals;
                for (int i = 0; i < 12; ++i) {
                    if (customScale[i]) {
                        intervals.push_back(i);
                    }
                }
                return intervals;
            }

        default:
            return {0, 2, 3, 5, 7, 8, 10};  // Default to natural minor
    }
}

std::vector<int> MusicalConstraints::getScaleIntervals() const {
    return getIntervalsForScale(currentScale);
}

std::vector<int> MusicalConstraints::getLegalNotes() const {
    std::vector<int> legalNotes;
    std::vector<int> intervals = getScaleIntervals();

    // Generate all legal MIDI notes within octave range
    for (int octave = octaveMin; octave <= octaveMax; ++octave) {
        int baseNote = (octave + 1) * 12;  // C of this octave (MIDI: C-1=0, C0=12, C1=24, etc.)

        for (int interval : intervals) {
            int midiNote = baseNote + rootNote + interval;
            if (midiNote >= 0 && midiNote <= 127) {
                legalNotes.push_back(midiNote);
            }
        }
    }

    return legalNotes;
}

bool MusicalConstraints::isNoteInScale(int midiNote) const {
    if (midiNote < 0 || midiNote > 127) return false;

    // Check if note is within octave range
    int octave = (midiNote / 12) - 1;
    if (octave < octaveMin || octave > octaveMax) return false;

    // Check if note's pitch class is in scale
    int pitchClass = (midiNote - rootNote) % 12;
    if (pitchClass < 0) pitchClass += 12;

    std::vector<int> intervals = getScaleIntervals();
    return std::find(intervals.begin(), intervals.end(), pitchClass) != intervals.end();
}

int MusicalConstraints::quantizeToScale(int midiNote) const {
    std::vector<int> legalNotes = getLegalNotes();

    if (legalNotes.empty()) return 60;  // Default to middle C

    // Find closest legal note
    int closest = legalNotes[0];
    int minDistance = std::abs(midiNote - closest);

    for (int note : legalNotes) {
        int distance = std::abs(midiNote - note);
        if (distance < minDistance) {
            minDistance = distance;
            closest = note;
        }
    }

    return closest;
}

int MusicalConstraints::getConstrainedNextNote(int currentNote) const {
    std::vector<int> legalNotes = getLegalNotes();

    if (legalNotes.empty()) return 60;

    // Find current note in legal notes
    auto it = std::find(legalNotes.begin(), legalNotes.end(), currentNote);
    int currentIndex = (it != legalNotes.end()) ? std::distance(legalNotes.begin(), it) : 0;

    switch (currentContour) {
        case Contour::DRONE:
            // 80% chance stay on same note, 20% move to neighbor
            if (rand() % 100 < 80) {
                return currentNote;
            }
            // Fall through to random walk
            [[fallthrough]];

        case Contour::RANDOM_WALK:
            {
                // Move up or down by small intervals
                int maxSteps = std::min(maxInterval, 3);
                int step = (rand() % (maxSteps * 2 + 1)) - maxSteps;
                int newIndex = currentIndex + step;
                newIndex = std::clamp(newIndex, 0, static_cast<int>(legalNotes.size()) - 1);
                return legalNotes[newIndex];
            }

        case Contour::ASCENDING:
            {
                // 70% chance move up, 30% stay or move down
                if (rand() % 100 < 70) {
                    int step = 1 + (rand() % std::min(maxInterval, 3));
                    int newIndex = std::min(currentIndex + step, static_cast<int>(legalNotes.size()) - 1);
                    return legalNotes[newIndex];
                }
                return currentNote;
            }

        case Contour::DESCENDING:
            {
                // 70% chance move down, 30% stay or move up
                if (rand() % 100 < 70) {
                    int step = 1 + (rand() % std::min(maxInterval, 3));
                    int newIndex = std::max(currentIndex - step, 0);
                    return legalNotes[newIndex];
                }
                return currentNote;
            }

        case Contour::ORBITING:
            {
                // Tendency to move toward gravity note
                int distance = gravityNote - currentNote;

                if (std::abs(distance) < 2) {
                    // Very close to gravity, stay nearby
                    int step = (rand() % 3) - 1;  // -1, 0, or 1
                    int newIndex = currentIndex + step;
                    newIndex = std::clamp(newIndex, 0, static_cast<int>(legalNotes.size()) - 1);
                    return legalNotes[newIndex];
                } else {
                    // Move toward gravity (60% of the time)
                    if (rand() % 100 < 60) {
                        int step = (distance > 0) ? 1 : -1;
                        int newIndex = currentIndex + step;
                        newIndex = std::clamp(newIndex, 0, static_cast<int>(legalNotes.size()) - 1);
                        return legalNotes[newIndex];
                    } else {
                        return currentNote;
                    }
                }
            }

        default:
            return currentNote;
    }
}

std::string MusicalConstraints::getScaleName() const {
    // Note names
    const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    std::string root = noteNames[rootNote];

    // Scale names
    switch (currentScale) {
        case Scale::CHROMATIC: return root + " Chromatic";
        case Scale::MINOR_NATURAL: return root + " Natural Minor";
        case Scale::HARMONIC_MINOR: return root + " Harmonic Minor";
        case Scale::PHRYGIAN: return root + " Phrygian";
        case Scale::LOCRIAN: return root + " Locrian";
        case Scale::DORIAN: return root + " Dorian";
        case Scale::WHOLE_TONE: return root + " Whole Tone";
        case Scale::DIMINISHED: return root + " Diminished";
        case Scale::PENTATONIC_MINOR: return root + " Pentatonic Minor";
        case Scale::CUSTOM: return root + " Custom";
        default: return root + " Unknown";
    }
}
