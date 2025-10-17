#include "ui_utils.h"
#include <algorithm>
#include <cctype>
#include <cwchar>

namespace UIUtils {

// Note names for MIDI note conversion
static const char* NOTE_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// Scale order used for sequencer menu
extern const std::vector<Scale> kScaleOrder = {
    Scale::CHROMATIC,
    Scale::MINOR_NATURAL,
    Scale::HARMONIC_MINOR,
    Scale::PHRYGIAN,
    Scale::LOCRIAN,
    Scale::DORIAN,
    Scale::WHOLE_TONE,
    Scale::DIMINISHED,
    Scale::PENTATONIC_MINOR,
    Scale::CUSTOM
};

// Subdivision order
extern const std::vector<Subdivision> kSubdivisionOrder = {
    Subdivision::WHOLE,
    Subdivision::HALF,
    Subdivision::QUARTER,
    Subdivision::EIGHTH,
    Subdivision::SIXTEENTH,
    Subdivision::THIRTYSECOND,
    Subdivision::SIXTYFOURTH
};

// UTF-8 / Wide string functions

int SafeWcWidth(wchar_t ch) {
    int w = wcwidth(ch);
    return (w < 0) ? 1 : w;
}

int DisplayWidth(const std::wstring& text) {
    int w = wcswidth(text.c_str(), text.size());
    if (w >= 0) {
        return w;
    }

    int total = 0;
    for (wchar_t ch : text) {
        total += SafeWcWidth(ch);
    }
    return total;
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return L"";
    }
    size_t required = std::mbstowcs(nullptr, text.c_str(), 0);
    if (required == static_cast<size_t>(-1)) {
        std::wstring fallback;
        fallback.reserve(text.size());
        for (unsigned char c : text) {
            fallback.push_back(static_cast<wchar_t>(c));
        }
        return fallback;
    }
    std::wstring result(required, L'\0');
    std::mbstowcs(result.data(), text.c_str(), required);
    return result;
}

std::wstring FitCell(const std::wstring& input, int width, CellAlign align) {
    if (width <= 0) {
        return L"";
    }

    std::wstring text = input;
    int w = DisplayWidth(text);

    if (w > width) {
        std::wstring truncated;
        truncated.reserve(text.size());
        int target = (width >= 2) ? (width - 1) : width;
        int acc = 0;
        for (wchar_t ch : text) {
            int cw = SafeWcWidth(ch);
            if (acc + cw > target) break;
            truncated.push_back(ch);
            acc += cw;
        }
        if (width >= 2) {
            truncated.push_back(L'…');
        }
        text.swap(truncated);
        w = DisplayWidth(text);
    }

    if (w < width) {
        int pad = width - w;
        if (align == CellAlign::Right) {
            text = std::wstring(pad, L' ') + text;
        } else if (align == CellAlign::Center) {
            int left = pad / 2;
            int right = pad - left;
            text = std::wstring(left, L' ') + text + std::wstring(right, L' ');
        } else {
            text += std::wstring(pad, L' ');
        }
    }

    return text;
}

// Column layout computation

int ComputeTotalWidth(const std::vector<int>& widths) {
    int total = 1; // left border
    for (int w : widths) {
        total += w + 1; // column width + boundary
    }
    return total;
}

std::vector<int> ComputeColumnStarts(const std::vector<int>& widths) {
    std::vector<int> starts;
    starts.reserve(widths.size());
    int x = 1;
    for (int w : widths) {
        starts.push_back(x);
        x += w + 1;
    }
    return starts;
}

std::vector<int> ComputeColumnBoundaries(const std::vector<int>& widths) {
    std::vector<int> boundaries;
    boundaries.reserve(widths.size() + 1);
    int x = 0;
    boundaries.push_back(x);
    for (int w : widths) {
        x += w;
        boundaries.push_back(x);
        ++x;
    }
    return boundaries;
}

// Grid drawing helpers

void DrawHorizontalLine(WINDOW* win, int y, const std::vector<int>& widths,
                        chtype leftChar, chtype midChar, chtype rightChar, chtype junctionChar) {
    int x = 0;
    mvwaddch(win, y, x, leftChar);
    ++x;
    for (size_t i = 0; i < widths.size(); ++i) {
        mvwhline(win, y, x, midChar, widths[i]);
        x += widths[i];
        chtype junction = (i == widths.size() - 1) ? rightChar : junctionChar;
        mvwaddch(win, y, x, junction);
        ++x;
    }
}

void DrawSequencerGrid(WINDOW* win, int rows, const std::vector<int>& widths,
                       const std::vector<int>& boundaries) {
    const int totalWidth = ComputeTotalWidth(widths);

    for (int y = 1; y <= rows; ++y) {
        mvwhline(win, y, 1, ' ', totalWidth - 2);
    }

    DrawHorizontalLine(win, 0, widths, ACS_ULCORNER, ACS_HLINE, ACS_URCORNER, ACS_TTEE);
    DrawHorizontalLine(win, rows + 1, widths, ACS_LLCORNER, ACS_HLINE, ACS_LRCORNER, ACS_BTEE);

    for (int boundaryX : boundaries) {
        for (int y = 1; y <= rows; ++y) {
            mvwaddch(win, y, boundaryX, ACS_VLINE);
        }
    }
}

void RenderSequencerCell(WINDOW* win,
                         int rowIndex,
                         int columnIndex,
                         const std::wstring& text,
                         const std::vector<int>& widths,
                         const std::vector<int>& starts,
                         CellAlign align,
                         bool highlight,
                         bool dim,
                         int dimColorPair) {
    int y = 1 + rowIndex;
    int x = starts[columnIndex];
    int width = widths[columnIndex];

    std::wstring slot = FitCell(text, width, align);

    if (dim && !highlight && dimColorPair > 0) {
        wattron(win, COLOR_PAIR(dimColorPair));
    }
    if (highlight) {
        wattron(win, COLOR_PAIR(1) | A_BOLD);
    }

    mvwaddnwstr(win, y, x, slot.c_str(), slot.size());

    if (highlight) {
        wattroff(win, COLOR_PAIR(1) | A_BOLD);
    }
    if (dim && !highlight && dimColorPair > 0) {
        wattroff(win, COLOR_PAIR(dimColorPair));
    }
}

// Sequencer grid constants
const std::vector<int> kSequencerColumnWidths = {3, 1, 6, 4, 4, 4};
const std::vector<int> kSequencerColumnStarts = ComputeColumnStarts(kSequencerColumnWidths);
const std::vector<int> kSequencerColumnBoundaries = ComputeColumnBoundaries(kSequencerColumnWidths);
const int kSequencerGridWidth = ComputeTotalWidth(kSequencerColumnWidths);

// Note/music conversion helpers

std::string midiNoteToString(int midiNote) {
    if (midiNote < 0 || midiNote > 127) {
        return "--";
    }
    int octave = (midiNote / 12) - 1;
    int noteIndex = midiNote % 12;
    return std::string(NOTE_NAMES[noteIndex]) + std::to_string(octave);
}

std::string rootNoteToString(int root) {
    root = (root % 12 + 12) % 12;
    return NOTE_NAMES[root];
}

int noteNameToSemitone(const std::string& name) {
    std::string upper;
    upper.reserve(name.size());
    for (char c : name) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }
    if (upper.empty()) return -1;

    // Extract base note (A-G)
    char base = upper[0];
    if (base < 'A' || base > 'G') return -1;

    int baseIndex;
    switch (base) {
        case 'C': baseIndex = 0; break;
        case 'D': baseIndex = 2; break;
        case 'E': baseIndex = 4; break;
        case 'F': baseIndex = 5; break;
        case 'G': baseIndex = 7; break;
        case 'A': baseIndex = 9; break;
        case 'B': baseIndex = 11; break;
        default: return -1;
    }

    size_t pos = 1;
    int accidental = 0;
    if (pos < upper.size()) {
        if (upper[pos] == '#') {
            accidental = 1;
            ++pos;
        } else if (upper[pos] == 'B') {
            accidental = -1;
            ++pos;
        }
    }

    int octave = 0;
    if (pos < upper.size()) {
        try {
            octave = std::stoi(upper.substr(pos));
        } catch (...) {
            return -1;
        }
    } else {
        // If no octave specified, assume current octave (C3 equivalent)
        octave = 3;
    }

    int midi = (octave + 1) * 12 + baseIndex + accidental;
    if (midi < 0 || midi > 127) return -1;
    return midi;
}

bool parseNoteText(const std::string& text, int& midiOut) {
    try {
        size_t idx = 0;
        int numeric = std::stoi(text, &idx);
        if (idx == text.size()) {
            if (numeric >= 0 && numeric <= 127) {
                midiOut = numeric;
                return true;
            }
            return false;
        }
    } catch (...) {
        // Fall back to note name parsing
    }

    int midi = noteNameToSemitone(text);
    if (midi >= 0) {
        midiOut = midi;
        return true;
    }
    return false;
}

bool parseRootText(const std::string& text, int& rootOut) {
    int midi;
    if (parseNoteText(text, midi)) {
        rootOut = ((midi % 12) + 12) % 12;
        return true;
    }

    std::string upper;
    upper.reserve(text.size());
    for (char c : text) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }
    if (upper.empty()) return false;

    // Handle simple root names like "C", "C#", "Db"
    if (upper == "C") { rootOut = 0; return true; }
    if (upper == "C#" || upper == "DB") { rootOut = 1; return true; }
    if (upper == "D") { rootOut = 2; return true; }
    if (upper == "D#" || upper == "EB") { rootOut = 3; return true; }
    if (upper == "E") { rootOut = 4; return true; }
    if (upper == "F") { rootOut = 5; return true; }
    if (upper == "F#" || upper == "GB") { rootOut = 6; return true; }
    if (upper == "G") { rootOut = 7; return true; }
    if (upper == "G#" || upper == "AB") { rootOut = 8; return true; }
    if (upper == "A") { rootOut = 9; return true; }
    if (upper == "A#" || upper == "BB") { rootOut = 10; return true; }
    if (upper == "B") { rootOut = 11; return true; }

    return false;
}

// Subdivision helpers

std::string subdivisionToString(Subdivision subdiv) {
    int denom = static_cast<int>(subdiv);
    if (denom <= 0) denom = 4;
    return "1/" + std::to_string(denom);
}

bool parseSubdivisionText(const std::string& text, Subdivision& outSubdiv) {
    std::string cleaned;
    cleaned.reserve(text.size());
    for (char c : text) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            cleaned.push_back(c);
        }
    }

    size_t slashPos = cleaned.find('/');
    if (slashPos != std::string::npos && slashPos + 1 < cleaned.size()) {
        cleaned = cleaned.substr(slashPos + 1);
    }

    if (cleaned.empty()) {
        return false;
    }

    int denom = 0;
    try {
        denom = std::stoi(cleaned);
    } catch (...) {
        return false;
    }

    switch (denom) {
        case 1: outSubdiv = Subdivision::WHOLE; return true;
        case 2: outSubdiv = Subdivision::HALF; return true;
        case 4: outSubdiv = Subdivision::QUARTER; return true;
        case 8: outSubdiv = Subdivision::EIGHTH; return true;
        case 16: outSubdiv = Subdivision::SIXTEENTH; return true;
        case 32: outSubdiv = Subdivision::THIRTYSECOND; return true;
        case 64: outSubdiv = Subdivision::SIXTYFOURTH; return true;
        default: break;
    }
    return false;
}

// Scale helpers

std::string scaleKey(Scale scale) {
    switch (scale) {
        case Scale::CHROMATIC: return "chromatic";
        case Scale::MINOR_NATURAL: return "minornatural";
        case Scale::HARMONIC_MINOR: return "harmonicminor";
        case Scale::PHRYGIAN: return "phrygian";
        case Scale::LOCRIAN: return "locrian";
        case Scale::DORIAN: return "dorian";
        case Scale::WHOLE_TONE: return "wholetone";
        case Scale::DIMINISHED: return "diminished";
        case Scale::PENTATONIC_MINOR: return "pentatonicminor";
        case Scale::CUSTOM: return "custom";
        default: return "chromatic";
    }
}

const char* scaleDisplayName(Scale scale) {
    switch (scale) {
        case Scale::CHROMATIC: return "Chromatic";
        case Scale::MINOR_NATURAL: return "Natural Minor";
        case Scale::HARMONIC_MINOR: return "Harmonic Minor";
        case Scale::PHRYGIAN: return "Phrygian";
        case Scale::LOCRIAN: return "Locrian";
        case Scale::DORIAN: return "Dorian";
        case Scale::WHOLE_TONE: return "Whole Tone";
        case Scale::DIMINISHED: return "Diminished";
        case Scale::PENTATONIC_MINOR: return "Pentatonic Minor";
        case Scale::CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

bool parseScaleText(const std::string& text, Scale& outScale) {
    std::string lower;
    lower.reserve(text.size());
    for (char c : text) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    if (lower.empty()) return false;

    try {
        int idx = std::stoi(lower);
        if (idx >= 0 && idx < static_cast<int>(kScaleOrder.size())) {
            outScale = kScaleOrder[idx];
            return true;
        }
    } catch (...) {
        // Not numeric, fall through
    }

    for (Scale sc : kScaleOrder) {
        std::string key = scaleKey(sc);
        if (lower == key) {
            outScale = sc;
            return true;
        }
        // Accept shortened aliases
        if (sc == Scale::MINOR_NATURAL && (lower == "minor" || lower == "naturalminor")) {
            outScale = sc; return true;
        }
        if (sc == Scale::HARMONIC_MINOR && (lower == "harmminor" || lower == "harmonic")) {
            outScale = sc; return true;
        }
        if (sc == Scale::WHOLE_TONE && lower == "whole") {
            outScale = sc; return true;
        }
        if (sc == Scale::DIMINISHED && (lower == "dim" || lower == "diminished")) {
            outScale = sc; return true;
        }
        if (sc == Scale::PENTATONIC_MINOR && (lower == "pentatonic" || lower == "penta")) {
            outScale = sc; return true;
        }
    }

    return false;
}

} // namespace UIUtils
