#include "ui.h"
#include "synth.h"
#include "preset.h"
#include "loop_manager.h"
#include "looper.h"
#include "sequencer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cwchar>

// External references to global objects from main.cpp
extern LoopManager* loopManager;
extern Sequencer* sequencer;

struct SequencerInfoEntryDef {
    UI::SequencerInfoField field;
    const char* label;
    bool editable;
};

static const std::vector<SequencerInfoEntryDef> kSequencerInfoEntries = {
    {UI::SequencerInfoField::TEMPO,       "Tempo",        true},
    {UI::SequencerInfoField::ROOT,        "Root",         true},
    {UI::SequencerInfoField::SCALE,       "Scale",        true},
    {UI::SequencerInfoField::EUCLID_HITS, "Euclid Hits",  true},
    {UI::SequencerInfoField::EUCLID_STEPS,"Euclid Steps", true},
    {UI::SequencerInfoField::SUBDIVISION, "Subdivision",  true},
    {UI::SequencerInfoField::MUTATE_AMOUNT, "Mutate %",   true},
    {UI::SequencerInfoField::MUTED,       "Muted",        true},
    {UI::SequencerInfoField::SOLO,        "Solo",         true},
    {UI::SequencerInfoField::ACTIVE_COUNT,"Active",       false},
    {UI::SequencerInfoField::LOCKED_COUNT,"Locked",       false}
};

static const std::vector<Scale> kScaleOrder = {
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

static const std::vector<Subdivision> kSubdivisionOrder = {
    Subdivision::WHOLE,
    Subdivision::HALF,
    Subdivision::QUARTER,
    Subdivision::EIGHTH,
    Subdivision::SIXTEENTH,
    Subdivision::THIRTYSECOND,
    Subdivision::SIXTYFOURTH
};

static const char* NOTE_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

namespace {

enum class CellAlign {
    Left,
    Center,
    Right
};

inline int SafeWcWidth(wchar_t ch) {
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

const std::vector<int> kSequencerColumnWidths = {3, 1, 6, 4, 4, 4};
const std::vector<int> kSequencerColumnStarts = ComputeColumnStarts(kSequencerColumnWidths);
const std::vector<int> kSequencerColumnBoundaries = ComputeColumnBoundaries(kSequencerColumnWidths);
const int kSequencerGridWidth = ComputeTotalWidth(kSequencerColumnWidths);

} // namespace

static std::string midiNoteToString(int midiNote) {
    if (midiNote < 0 || midiNote > 127) {
        return "--";
    }
    int octave = (midiNote / 12) - 1;
    int noteIndex = midiNote % 12;
    return std::string(NOTE_NAMES[noteIndex]) + std::to_string(octave);
}

static std::string rootNoteToString(int root) {
    root = (root % 12 + 12) % 12;
    return NOTE_NAMES[root];
}

static int noteNameToSemitone(const std::string& name) {
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

static bool parseNoteText(const std::string& text, int& midiOut) {
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

static bool parseRootText(const std::string& text, int& rootOut) {
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

static std::string subdivisionToString(Subdivision subdiv) {
    int denom = static_cast<int>(subdiv);
    if (denom <= 0) denom = 4;
    return "1/" + std::to_string(denom);
}

static bool parseSubdivisionText(const std::string& text, Subdivision& outSubdiv) {
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

static std::string scaleKey(Scale scale) {
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

static const char* scaleDisplayName(Scale scale) {
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

static bool parseScaleText(const std::string& text, Scale& outScale) {
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

UI::UI(Synth* synth, SynthParameters* params)
    : synth(synth)
    , params(params)
    , initialized(false)
    , currentPage(UIPage::MAIN)
    , audioDeviceName("Unknown")
    , audioSampleRate(0)
    , audioBufferSize(0)
    , midiDeviceName("Not connected")
    , midiPortNum(-1)
    , currentAudioDeviceId(-1)
    , currentMidiPortNum(-1)
    , selectedParameterId(0)
    , numericInputActive(false)
    , currentPresetName("None")
    , textInputActive(false)
    , deviceChangeRequested(false)
    , requestedAudioDeviceId(-1)
    , requestedMidiPortNum(-1)
    , helpActive(false)
    , helpScrollOffset(0)
    , waveformBuffer(WAVEFORM_BUFFER_SIZE, 0.0f)
    , waveformBufferWritePos(0)
    , sequencerSelectedRow(0)
    , sequencerSelectedColumn(static_cast<int>(SequencerTrackerColumn::NOTE))
    , sequencerFocusRightPane(false)
    , sequencerRightSelection(0)
    , sequencerMutateAmount(20.0f)
    , sequencerFocusActionsPane(false)
    , sequencerActionsRow(0)
    , sequencerActionsColumn(0)
    , sequencerScaleMenuActive(false)
    , sequencerScaleMenuIndex(0)
    , numericInputIsSequencer(false) {
    
    // Load available presets
    refreshPresetList();
    
    // Initialize parameter definitions
    initializeParameters();
    
    // Set initial selected parameter to first parameter on main page
    std::vector<int> mainParams = getParameterIdsForPage(UIPage::MAIN);
    if (!mainParams.empty()) {
        selectedParameterId = mainParams[0];  // Start with first parameter
    }
    
}

UI::~UI() {
    if (initialized) {
        endwin();
    }
}

bool UI::initialize() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);
        init_pair(4, COLOR_RED, COLOR_BLACK);
        init_pair(5, COLOR_WHITE, COLOR_BLUE);   // Active tab
        init_pair(6, COLOR_BLACK, COLOR_CYAN);   // Inactive tab
        
        // Check if we have 256 colors available
        if (COLORS >= 256) {
            // Use 256-color grayscale palette (colors 232-255 are grayscale)
            // Map to pairs 7-26 for 20 levels of gray
            for (int i = 0; i < 20; ++i) {
                int grayColor = 232 + i * 23 / 19;  // Map 0-19 to 232-255
                init_pair(7 + i, grayColor, COLOR_BLACK);
            }
        } else {
            // Fallback to 8-color mode with attributes
            // Use pairs 7-12 for basic grayscale (6 levels)
            init_pair(7, COLOR_BLACK, COLOR_BLACK);    // Level 0 - darkest
            init_pair(8, COLOR_BLACK, COLOR_BLACK);    // Level 1 - very dim (will use A_BOLD)
            init_pair(9, COLOR_WHITE, COLOR_BLACK);    // Level 2 - dim (will use A_DIM)
            init_pair(10, COLOR_WHITE, COLOR_BLACK);   // Level 3 - medium (will use A_NORMAL)
            init_pair(11, COLOR_WHITE, COLOR_BLACK);   // Level 4 - bright (will use A_BOLD)
            init_pair(12, COLOR_CYAN, COLOR_BLACK);    // Level 5 - brightest (cyan bold for extra pop)
        }
    }
    
    initialized = true;
    return true;
}

bool UI::update() {
    int ch = getch();

    if (ch != ERR) {
        handleInput(ch);

        if (ch == 'q' || ch == 'Q') {
            return false;
        }
    }

    // Check for MIDI learn timeout (10 seconds)
    if (params->midiLearnActive.load()) {
        auto now = std::chrono::steady_clock::now();
        double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();
        double startTime = params->midiLearnStartTime.load();

        if (currentTime - startTime > 10.0) {
            // Timeout - cancel MIDI learn
            finishMidiLearn();
            addConsoleMessage("MIDI Learn timeout - cancelled");
        }
    }

    return true;
}

void UI::setDeviceInfo(const std::string& audioDevice, int sampleRate, int bufferSize,
                       const std::string& midiDevice, int midiPort) {
    audioDeviceName = audioDevice;
    audioSampleRate = sampleRate;
    audioBufferSize = bufferSize;
    midiDeviceName = midiDevice;
    midiPortNum = midiPort;
}

void UI::setAvailableAudioDevices(const std::vector<std::pair<int, std::string>>& devices, int currentDeviceId) {
    availableAudioDevices = devices;
    currentAudioDeviceId = currentDeviceId;
}

void UI::setAvailableMidiDevices(const std::vector<std::pair<int, std::string>>& devices, int currentPort) {
    availableMidiDevices = devices;
    currentMidiPortNum = currentPort;
}

void UI::handleInput(int ch) {
    // Handle help mode
    if (helpActive) {
        if (ch == 'h' || ch == 'H' || ch == 27 || ch == 'q' || ch == 'Q') {  // H, Esc, or Q to close
            hideHelp();
        } else if (ch == KEY_UP || ch == 'k' || ch == 'K') {
            helpScrollOffset = std::max(0, helpScrollOffset - 1);
        } else if (ch == KEY_DOWN || ch == 'j' || ch == 'J') {
            helpScrollOffset++;
        } else if (ch == KEY_PPAGE) {  // Page Up
            helpScrollOffset = std::max(0, helpScrollOffset - 10);
        } else if (ch == KEY_NPAGE) {  // Page Down
            helpScrollOffset += 10;
        }
        return;
    }

    // Handle preset text input mode
    if (textInputActive) {
        handleTextInput(ch);
        return;
    }

    if (sequencerScaleMenuActive) {
        handleSequencerScaleMenuInput(ch);
        return;
    }

    // Handle numeric input mode for parameters
    if (numericInputActive) {
        if (ch == '\n' || ch == KEY_ENTER) {
            finishNumericInput();
        } else if (ch == 27) {  // Escape key
            if (numericInputIsSequencer) {
                cancelSequencerNumericInput();
            }
            numericInputActive = false;
            numericInputBuffer.clear();
            clear();  // Clear screen to refresh properly
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            if (!numericInputBuffer.empty()) {
                numericInputBuffer.pop_back();
            }
        } else if (ch >= 32 && ch < 127) {  // Printable characters
            if (numericInputBuffer.length() < 20) {
                numericInputBuffer += static_cast<char>(ch);
            }
        }
        return;
    }

    // Handle MIDI learn mode - allow Escape to cancel
    if (params->midiLearnActive.load()) {
        if (ch == 27) {  // Escape key
            finishMidiLearn();
            addConsoleMessage("MIDI Learn cancelled");
            clear();  // Clear screen to refresh properly
        }
        return;  // Don't process other keys during MIDI learn
    }
    
    // Sequencer-specific navigation and editing
    if (currentPage == UIPage::SEQUENCER && sequencer) {
        if (handleSequencerInput(ch)) {
            return;
        }
    }

    // Get current page parameter IDs (for non-sequencer pages)
    std::vector<int> pageParams;
    if (currentPage != UIPage::SEQUENCER) {
        pageParams = getParameterIdsForPage(currentPage);
    }
    
    // Tab key cycles forward through pages
    if (ch == '\t') {
        if (currentPage == UIPage::MAIN) currentPage = UIPage::BRAINWAVE;
        else if (currentPage == UIPage::BRAINWAVE) currentPage = UIPage::REVERB;
        else if (currentPage == UIPage::REVERB) currentPage = UIPage::FILTER;
        else if (currentPage == UIPage::FILTER) currentPage = UIPage::LOOPER;
        else if (currentPage == UIPage::LOOPER) currentPage = UIPage::SEQUENCER;
        else if (currentPage == UIPage::SEQUENCER) currentPage = UIPage::CONFIG;
        else currentPage = UIPage::MAIN;

        // Set selected parameter to first parameter on new page
        std::vector<int> newPageParams = getParameterIdsForPage(currentPage);
        if (!newPageParams.empty()) {
            selectedParameterId = newPageParams[0];
        }
        return;
    }

    // Ctrl+Tab (KEY_BTAB or Shift+Tab) cycles backward through pages
    if (ch == KEY_BTAB || ch == 353) {  // KEY_BTAB = Shift+Tab, 353 = some terminals
        if (currentPage == UIPage::MAIN) currentPage = UIPage::CONFIG;
        else if (currentPage == UIPage::BRAINWAVE) currentPage = UIPage::MAIN;
        else if (currentPage == UIPage::REVERB) currentPage = UIPage::BRAINWAVE;
        else if (currentPage == UIPage::FILTER) currentPage = UIPage::REVERB;
        else if (currentPage == UIPage::LOOPER) currentPage = UIPage::FILTER;
        else if (currentPage == UIPage::SEQUENCER) currentPage = UIPage::LOOPER;
        else if (currentPage == UIPage::CONFIG) currentPage = UIPage::SEQUENCER;

        // Set selected parameter to first parameter on new page
        std::vector<int> newPageParams = getParameterIdsForPage(currentPage);
        if (!newPageParams.empty()) {
            selectedParameterId = newPageParams[0];
        }
        return;
    }
    
    // Up/Down arrows navigate between parameters on current page
    if (currentPage != UIPage::SEQUENCER && ch == KEY_UP) {
        if (!pageParams.empty()) {
            auto it = std::find(pageParams.begin(), pageParams.end(), selectedParameterId);
            if (it != pageParams.end() && it != pageParams.begin()) {
                selectedParameterId = *(--it);
            } else {
                selectedParameterId = pageParams.back(); // Wrap to end
            }
        }
        return;
    } else if (currentPage != UIPage::SEQUENCER && ch == KEY_DOWN) {
        if (!pageParams.empty()) {
            auto it = std::find(pageParams.begin(), pageParams.end(), selectedParameterId);
            if (it != pageParams.end() && (it + 1) != pageParams.end()) {
                selectedParameterId = *(++it);
            } else {
                selectedParameterId = pageParams.front(); // Wrap to beginning
            }
        }
        return;
    }

    // Left/Right arrows adjust parameter values
    if (currentPage != UIPage::SEQUENCER && ch == KEY_LEFT) {
        adjustParameter(selectedParameterId, false);
        return;
    } else if (currentPage != UIPage::SEQUENCER && ch == KEY_RIGHT) {
        adjustParameter(selectedParameterId, true);
        return;
    }

    // Enter key starts numeric input for current parameter
    if (currentPage != UIPage::SEQUENCER && (ch == '\n' || ch == KEY_ENTER)) {
        startNumericInput(selectedParameterId);
        return;
    }

    // 'H' key shows help (uppercase only to avoid conflicts)
    if (ch == 'H') {
        showHelp();
        return;
    }

    // 'L' key starts MIDI learn for current parameter
    if (ch == 'L' || ch == 'l') {
        startMidiLearn(selectedParameterId);
        return;
    }
    
    // Transport and looping hotkeys (keep these)
    if (ch == ' ') {
        // Spacebar behavior depends on current page
        if (currentPage == UIPage::LOOPER) {
            if (loopManager) {
                Looper* loop = loopManager->getCurrentLoop();
                if (loop) loop->pressRecPlay();
            }
        } else if (currentPage == UIPage::SEQUENCER) {
            if (sequencer) {
                if (sequencer->isPlaying()) {
                    sequencer->stop();
                } else {
                    sequencer->play();
                }
            }
        }
        return;
    }
    
    // Looper-specific hotkeys (only active on looper page)
    if (currentPage == UIPage::LOOPER) {
        switch (ch) {
            // Loop selection (1-4)
            case '1': params->currentLoop = 0; break;
            case '2': params->currentLoop = 1; break;
            case '3': params->currentLoop = 2; break;
            case '4': params->currentLoop = 3; break;
                
            // Overdub toggle (O/o)
            case 'O':
            case 'o':
                if (loopManager) {
                    Looper* loop = loopManager->getCurrentLoop();
                    if (loop) loop->pressOverdub();
                }
                break;
                
            // Stop (S/s)
            case 'S':
            case 's':
                if (loopManager) {
                    Looper* loop = loopManager->getCurrentLoop();
                    if (loop) loop->pressStop();
                }
                break;
                
            // Clear (C/c)
            case 'C':
            case 'c':
                if (loopManager) {
                    Looper* loop = loopManager->getCurrentLoop();
                    if (loop) loop->pressClear();
                }
                break;
                
            // Overdub mix ([/])
            case '[':
                params->overdubMix = std::max(0.0f, params->overdubMix.load() - 0.05f);
                break;
            case ']':
                params->overdubMix = std::min(1.0f, params->overdubMix.load() + 0.05f);
                break;
        }
    }

    // Sequencer-specific hotkeys (only active on sequencer page)
    if (currentPage == UIPage::SEQUENCER && sequencer) {
        switch (ch) {
            // Generate pattern (G/g)
            case 'G':
            case 'g':
                sequencer->generatePattern();
                addConsoleMessage("Generated new pattern");
                break;

            // Mutate pattern (M/m)
            case 'M':
            case 'm':
                sequencer->mutatePattern(0.2f);  // 20% mutation
                addConsoleMessage("Pattern mutated");
                break;

            // Reset sequencer (R/r)
            case 'R':
            case 'r':
                sequencer->reset();
                addConsoleMessage("Sequencer reset");
                break;

            // Tempo control (T/Y)
            case 'T':
            case 't':
                {
                    double newTempo = std::max(20.0, sequencer->getTempo() - 5.0);
                    sequencer->setTempo(newTempo);
                }
                break;
            case 'Y':
            case 'y':
                {
                    double newTempo = std::min(300.0, sequencer->getTempo() + 5.0);
                    sequencer->setTempo(newTempo);
                }
                break;

            // Euclidean hits control (H/J)
            case 'H':
            case 'h':
                {
                    EuclideanPattern& euc = sequencer->getEuclideanPattern();
                    int newHits = std::max(1, euc.getHits() - 1);
                    sequencer->setEuclideanPattern(newHits, euc.getSteps(), euc.getRotation());
                    sequencer->regenerateUnlocked();
                    addConsoleMessage("Euclidean hits: " + std::to_string(newHits));
                }
                break;
            case 'J':
            case 'j':
                {
                    EuclideanPattern& euc = sequencer->getEuclideanPattern();
                    int newHits = std::min(euc.getSteps(), euc.getHits() + 1);
                    sequencer->setEuclideanPattern(newHits, euc.getSteps(), euc.getRotation());
                    sequencer->regenerateUnlocked();
                    addConsoleMessage("Euclidean hits: " + std::to_string(newHits));
                }
                break;

            // Track selection (1-4)
            case '1':
                sequencer->setCurrentTrack(0);
                addConsoleMessage("Sequencer: Track 1 selected");
                break;
            case '2':
                sequencer->setCurrentTrack(1);
                addConsoleMessage("Sequencer: Track 2 selected");
                break;
            case '3':
                sequencer->setCurrentTrack(2);
                addConsoleMessage("Sequencer: Track 3 selected");
                break;
            case '4':
                sequencer->setCurrentTrack(3);
                addConsoleMessage("Sequencer: Track 4 selected");
                break;

            // Pattern rotation ([/])
            case '[':
                sequencer->rotatePattern(-1);
                addConsoleMessage("Sequencer: Rotated pattern left");
                break;
            case ']':
                sequencer->rotatePattern(1);
                addConsoleMessage("Sequencer: Rotated pattern right");
                break;
        }
    }
}

void UI::drawTabs() {
    int cols = getmaxx(stdscr);
    
    // Main tab
    if (currentPage == UIPage::MAIN) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 0, " MAIN ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 0, " MAIN ");
        attroff(COLOR_PAIR(6));
    }
    
    // Brainwave tab
    if (currentPage == UIPage::BRAINWAVE) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 6, " BRAINWAVE ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 6, " BRAINWAVE ");
        attroff(COLOR_PAIR(6));
    }
    
    // Reverb tab
    if (currentPage == UIPage::REVERB) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 17, " REVERB ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 17, " REVERB ");
        attroff(COLOR_PAIR(6));
    }
    
    // Filter tab
    if (currentPage == UIPage::FILTER) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 25, " FILTER ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 25, " FILTER ");
        attroff(COLOR_PAIR(6));
    }
    
    // Looper tab
    if (currentPage == UIPage::LOOPER) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 33, " LOOPER ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 33, " LOOPER ");
        attroff(COLOR_PAIR(6));
    }

    // Sequencer tab
    if (currentPage == UIPage::SEQUENCER) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 41, " SEQUENCER ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 41, " SEQUENCER ");
        attroff(COLOR_PAIR(6));
    }

    // Config tab
    if (currentPage == UIPage::CONFIG) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(0, 52, " CONFIG ");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        attron(COLOR_PAIR(6));
        mvprintw(0, 52, " CONFIG ");
        attroff(COLOR_PAIR(6));
    }


    // Fill rest of line
    for (int i = 60; i < cols; ++i) {
        mvaddch(0, i, ' ');
    }
    
    // Draw separator line
    attron(COLOR_PAIR(1));
    mvhline(1, 0, '-', cols);
    attroff(COLOR_PAIR(1));
}

void UI::drawBar(int y, int x, const char* label, float value, float min, float max, int width) {
    float normalized = (value - min) / (max - min);
    int fillWidth = static_cast<int>(normalized * width);
    
    mvprintw(y, x, "%s", label);
    
    int labelLen = 0;
    while (label[labelLen] != '\0') labelLen++;
    
    int barStart = x + labelLen + 1;
    
    mvaddch(y, barStart, '[');
    attron(COLOR_PAIR(2));
    for (int i = 0; i < width; ++i) {
        if (i < fillWidth) {
            mvaddch(y, barStart + 1 + i, '=');
        } else {
            mvaddch(y, barStart + 1 + i, ' ');
        }
    }
    attroff(COLOR_PAIR(2));
    mvaddch(y, barStart + width + 1, ']');
    
    mvprintw(y, barStart + width + 3, "%.3f", value);
}

void UI::drawMainPage(int activeVoices) {
    int row = 3;
    std::vector<int> pageParams = getParameterIdsForPage(currentPage);
    
    // Draw parameters inline with left/right control indicators
    for (int paramId : pageParams) {
        InlineParameter* param = getParameter(paramId);
        if (!param) continue;
        
        // Highlight selected parameter
        if (paramId == selectedParameterId) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(row, 2, ">");
        } else {
            mvprintw(row, 2, " ");
        }
        
        // Parameter name and value
        std::string displayValue = getParameterDisplayString(paramId);
        mvprintw(row, 4, "%-15s: %s", param->name.c_str(), displayValue.c_str());
        
        // Show arrows for selected parameter
        if (paramId == selectedParameterId) {
            mvprintw(row, 40, "← →");
            attroff(COLOR_PAIR(5) | A_BOLD);
        }
        
        row++;
    }
    
    row += 2;
    
    // Voice info
    attron(A_BOLD);
    mvprintw(row++, 2, "VOICES");
    attroff(A_BOLD);
    mvprintw(row++, 2, "Active: %d/8", activeVoices);
}

void UI::drawParametersPage() {
    int row = 3;
    std::vector<int> pageParams = getParameterIdsForPage(currentPage);
    
    // Draw parameters inline with left/right control indicators
    for (int paramId : pageParams) {
        InlineParameter* param = getParameter(paramId);
        if (!param) continue;
        
        // Highlight selected parameter
        if (paramId == selectedParameterId) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(row, 2, ">");
        } else {
            mvprintw(row, 2, " ");
        }
        
        // Parameter name and value
        std::string displayValue = getParameterDisplayString(paramId);
        mvprintw(row, 4, "%-18s: %s", param->name.c_str(), displayValue.c_str());
        
        // Show control hints for selected parameter
        if (paramId == selectedParameterId) {
            mvprintw(row, 45, "← →");
            if (param->supports_midi_learn) {
                mvprintw(row, 50, "[L]");
            }
            mvprintw(row, 55, "[Enter]");
            attroff(COLOR_PAIR(5) | A_BOLD);
        }
        
        row++;
    }
}

void UI::drawBrainwavePage() {
    drawParametersPage();
}

void UI::drawReverbPage() {
    drawParametersPage();
}

void UI::drawFilterPage() {
    drawParametersPage();
    
    // Show MIDI Learn status if active
    if (params->midiLearnActive.load() && params->midiLearnParameterId.load() == 32) { // Filter cutoff
        int row = 15;
        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(row++, 2, ">>> MIDI LEARN ACTIVE <<<");
        attroff(COLOR_PAIR(3) | A_BOLD);
        mvprintw(row++, 2, "Move a MIDI controller to assign it to Filter Cutoff");
    }
}

void UI::drawLooperPage() {
    if (!loopManager) {
        mvprintw(3, 2, "Looper not initialized");
        return;
    }
    
    int row = 3;
    int currentLoop = params->currentLoop.load();
    
    // Title
    attron(A_BOLD);
    mvprintw(row++, 1, "LOOPER - Guitar Pedal Style");
    attroff(A_BOLD);
    row++;
    
    // Current loop indicator
    mvprintw(row++, 2, "Current Loop: ");
    attron(COLOR_PAIR(5) | A_BOLD);
    printw("%d", currentLoop + 1);
    attroff(COLOR_PAIR(5) | A_BOLD);
    printw("  (Press 1-4 to select)");
    row++;
    
    // Loop state grid
    attron(A_BOLD);
    mvprintw(row++, 1, "LOOP STATUS");
    attroff(A_BOLD);
    row++;
    
    const char* stateNames[] = {"Empty", "Recording", "Playing", "Overdubbing", "Stopped"};
    int stateColors[] = {1, 3, 2, 4, 6};  // gray, yellow, green, red, cyan
    
    for (int i = 0; i < 4; ++i) {
        Looper::State state = loopManager->getLoopState(i);
        float loopLen = loopManager->getLoopLength(i);
        float loopTime = loopManager->getLoopTime(i);
        
        // Loop number
        if (i == currentLoop) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(row, 2, ">");
            attroff(COLOR_PAIR(5) | A_BOLD);
        } else {
            mvprintw(row, 2, " ");
        }
        
        mvprintw(row, 3, "Loop %d: ", i + 1);
        
        // State with color
        attron(COLOR_PAIR(stateColors[static_cast<int>(state)]) | A_BOLD);
        printw("%-12s", stateNames[static_cast<int>(state)]);
        attroff(COLOR_PAIR(stateColors[static_cast<int>(state)]) | A_BOLD);
        
        // Time display
        if (loopLen > 0.0f) {
            int lenMin = static_cast<int>(loopLen) / 60;
            int lenSec = static_cast<int>(loopLen) % 60;
            int lenMs = static_cast<int>((loopLen - static_cast<int>(loopLen)) * 100);
            
            int timeMin = static_cast<int>(loopTime) / 60;
            int timeSec = static_cast<int>(loopTime) % 60;
            int timeMs = static_cast<int>((loopTime - static_cast<int>(loopTime)) * 100);
            
            printw("  %02d:%02d.%02d / %02d:%02d.%02d", 
                   timeMin, timeSec, timeMs, 
                   lenMin, lenSec, lenMs);
        } else {
            printw("  --:--:-- / --:--:--");
        }
        
        row++;
    }
    
    row += 2;
    
    // Global parameters
    attron(A_BOLD);
    mvprintw(row++, 1, "PARAMETERS");
    attroff(A_BOLD);
    row++;
    
    drawBar(row++, 2, "Overdub Mix ([/])", params->overdubMix.load(), 0.0f, 1.0f, 20);
    
    row += 2;
    
    // Controls section
    attron(A_BOLD);
    mvprintw(row++, 1, "CONTROLS");
    attroff(A_BOLD);
    row++;
    
    mvprintw(row++, 2, "Space - Rec/Play toggle");
    mvprintw(row++, 2, "O     - Overdub toggle");
    mvprintw(row++, 2, "S     - Stop current loop");
    mvprintw(row++, 2, "C     - Clear current loop");
    mvprintw(row++, 2, "1-4   - Select loop");
    mvprintw(row++, 2, "[/]   - Adjust overdub mix");
    
    row += 2;
    
    // MIDI Learn section
    attron(A_BOLD);
    mvprintw(row++, 1, "MIDI CC LEARN");
    attroff(A_BOLD);
    row++;
    
    if (params->loopMidiLearnMode.load()) {
        const char* targetNames[] = {"Rec/Play", "Overdub", "Stop", "Clear"};
        int target = params->loopMidiLearnTarget.load();
        
        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(row++, 2, ">>> LEARNING MODE ACTIVE <<<");
        attroff(COLOR_PAIR(3) | A_BOLD);
        
        if (target >= 0 && target < 4) {
            mvprintw(row++, 2, "Move a MIDI controller to assign it to: %s", targetNames[target]);
        }
    } else {
        mvprintw(row++, 2, "Press M to cycle through MIDI learn targets");
    }
    
    // Show current CC mappings
    int recPlayCC = params->loopRecPlayCC.load();
    int overdubCC = params->loopOverdubCC.load();
    int stopCC = params->loopStopCC.load();
    int clearCC = params->loopClearCC.load();
    
    mvprintw(row++, 2, "Rec/Play CC: %s", recPlayCC >= 0 ? (std::string("CC#") + std::to_string(recPlayCC)).c_str() : "Not assigned");
    mvprintw(row++, 2, "Overdub CC:  %s", overdubCC >= 0 ? (std::string("CC#") + std::to_string(overdubCC)).c_str() : "Not assigned");
    mvprintw(row++, 2, "Stop CC:     %s", stopCC >= 0 ? (std::string("CC#") + std::to_string(stopCC)).c_str() : "Not assigned");
    mvprintw(row++, 2, "Clear CC:    %s", clearCC >= 0 ? (std::string("CC#") + std::to_string(clearCC)).c_str() : "Not assigned");
    
    row += 2;
    
    // Usage guide
    attron(A_BOLD);
    mvprintw(row++, 1, "USAGE GUIDE");
    attroff(A_BOLD);
    row++;
    
    mvprintw(row++, 2, "1. Press Space to start recording on selected loop");
    mvprintw(row++, 2, "2. Press Space again to start playback");
    mvprintw(row++, 2, "3. Press O to overdub additional layers");
    mvprintw(row++, 2, "4. Use 1-4 to switch between loops and layer sounds");
    mvprintw(row++, 2, "5. Adjust [/] to control overdub mix (prevents clipping)");
}

void UI::drawSequencerPage() {
    if (!sequencer) {
        mvprintw(3, 2, "Sequencer not initialized");
        return;
    }

    ensureSequencerSelectionInRange();

    int leftCol = 2;
    int row = 3;

    Track& track = sequencer->getCurrentTrack();
    Pattern& pattern = track.getPattern();
    MusicalConstraints& constraints = track.getConstraints();
    EuclideanPattern& euclidean = track.getEuclideanPattern();

    int trackIdx = sequencer->getCurrentTrackIndex();
    double tempo = sequencer->getTempo();
    bool playing = sequencer->isPlaying();
    int patternLength = std::max(1, pattern.getLength());
    int currentStep = sequencer->getCurrentStep(trackIdx);
    currentStep = (currentStep % patternLength + patternLength) % patternLength;

    int rightCol = leftCol + 35;  // Place right pane closer to tracker

    // Status indicators at top
    int statusAttr = playing ? (COLOR_PAIR(2) | A_BOLD) : (COLOR_PAIR(4) | A_BOLD);
    attron(statusAttr);
    mvprintw(row, leftCol, "[%s]", playing ? "PLAYING" : "STOPPED");
    attroff(statusAttr);
    mvprintw(row, leftCol + 12, "Step: %d/%d", currentStep + 1, patternLength);
    row++;

    attron(A_BOLD);
    mvprintw(row, leftCol, "SEQUENCER - Track %d", trackIdx + 1);
    attroff(A_BOLD);
    row += 2;

    // Draw actions section FIRST at top of right pane
    int actionsStartRow = 3;
    attron(A_BOLD);
    mvprintw(actionsStartRow, rightCol, "ACTIONS");
    attroff(A_BOLD);
    actionsStartRow++;

    // Generate and Clear buttons
    bool generateSelected = sequencerFocusActionsPane && sequencerActionsRow == 0;
    if (generateSelected) {
        attron(COLOR_PAIR(5) | A_BOLD);
    }
    mvprintw(actionsStartRow, rightCol, "[Generate]");
    if (generateSelected) {
        attroff(COLOR_PAIR(5) | A_BOLD);
    }

    bool clearSelected = sequencerFocusActionsPane && sequencerActionsRow == 1;
    if (clearSelected) {
        attron(COLOR_PAIR(5) | A_BOLD);
    }
    mvprintw(actionsStartRow, rightCol + 12, "[Clear]");
    if (clearSelected) {
        attroff(COLOR_PAIR(5) | A_BOLD);
    }
    actionsStartRow += 2;

    // Actions grid header
    mvprintw(actionsStartRow, rightCol, "            All  Note Vel  Gate Prob");
    actionsStartRow++;

    // Actions grid rows (Randomize, Mutate, Rotate only)
    const char* actionLabels[] = {"Randomize", "Mutate   ", "Rotate   "};
    for (int aRow = 0; aRow < 3; ++aRow) {
        mvprintw(actionsStartRow, rightCol, "%-10s", actionLabels[aRow]);
        for (int aCol = 0; aCol < 5; ++aCol) {
            bool cellSelected = sequencerFocusActionsPane &&
                                sequencerActionsRow == (aRow + 2) &&
                                sequencerActionsColumn == aCol;
            int xPos = rightCol + 12 + aCol * 5;
            if (cellSelected) {
                attron(COLOR_PAIR(5) | A_BOLD);
            }
            mvprintw(actionsStartRow, xPos, "[ ]");
            if (cellSelected) {
                attroff(COLOR_PAIR(5) | A_BOLD);
            }
        }
        actionsStartRow++;
    }

    int infoRow = actionsStartRow + 1;
    attron(A_BOLD);
    mvprintw(infoRow, rightCol, "PARAMETERS");
    attroff(A_BOLD);
    infoRow += 2;

    // Draw right-side info panel
    struct InfoValue {
        const SequencerInfoEntryDef* def;
        std::string text;
        int attr;
        bool highlight;
    };

    std::vector<InfoValue> infoValues;
    infoValues.reserve(kSequencerInfoEntries.size());

    auto pushInfo = [&](const SequencerInfoEntryDef& def, const std::string& value, int attr, bool highlight) {
        infoValues.push_back({&def, value, attr, highlight});
    };

    for (size_t i = 0; i < kSequencerInfoEntries.size(); ++i) {
        const auto& def = kSequencerInfoEntries[i];
        bool highlight = sequencerFocusRightPane && sequencerRightSelection == static_cast<int>(i);
        int attr = 0;
        std::string text;

        switch (def.field) {
            case SequencerInfoField::TEMPO: {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.1f BPM", tempo);
                text = buf;
                break;
            }
            case SequencerInfoField::ROOT: {
                text = rootNoteToString(constraints.getRootNote());
                break;
            }
            case SequencerInfoField::SCALE: {
                // Extract scale name without root
                std::string fullName = constraints.getScaleName();
                size_t spacePos = fullName.find(' ');
                if (spacePos != std::string::npos) {
                    text = fullName.substr(spacePos + 1);  // Skip "Root " part
                } else {
                    text = fullName;
                }
                break;
            }
            case SequencerInfoField::EUCLID_HITS: {
                text = std::to_string(euclidean.getHits());
                break;
            }
            case SequencerInfoField::EUCLID_STEPS: {
                text = std::to_string(euclidean.getSteps());
                break;
            }
            case SequencerInfoField::SUBDIVISION: {
                text = subdivisionToString(pattern.getResolution());
                break;
            }
            case SequencerInfoField::MUTATE_AMOUNT: {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.0f%%", sequencerMutateAmount);
                text = buf;
                break;
            }
            case SequencerInfoField::MUTED: {
                bool muted = track.isMuted();
                text = muted ? "YES" : "NO";
                attr = muted ? (COLOR_PAIR(4) | A_BOLD) : 0;
                break;
            }
            case SequencerInfoField::SOLO: {
                bool solo = track.isSolo();
                text = solo ? "YES" : "NO";
                attr = solo ? (COLOR_PAIR(3) | A_BOLD) : 0;
                break;
            }
            case SequencerInfoField::ACTIVE_COUNT: {
                text = std::to_string(pattern.getActiveStepCount());
                break;
            }
            case SequencerInfoField::LOCKED_COUNT: {
                text = std::to_string(pattern.getLockedStepCount());
                break;
            }
            default:
                text.clear();
                break;
        }

        pushInfo(def, text, attr, highlight);
    }

    for (const auto& info : infoValues) {
        int attr = info.highlight ? (COLOR_PAIR(5) | A_BOLD) : info.attr;
        if (attr != 0) {
            attron(attr);
        }
        mvprintw(infoRow, rightCol, "%-12s: %s", info.def->label, info.text.c_str());
        if (attr != 0) {
            attroff(attr);
        }
        ++infoRow;
    }

    // Simple tracker display - cap at 16 steps
    int displayRows = std::min(16, patternLength);

    // Draw header
    attron(A_BOLD);
    mvprintw(row, leftCol, " # L Note   Vel Gate Prob");
    attroff(A_BOLD);
    row++;

    // Draw separator
    mvprintw(row, leftCol, "----------------------------");
    row++;

    // Draw each step
    for (int i = 0; i < displayRows; ++i) {
        const PatternStep& step = pattern.getStep(i);
        bool rowSelected = (!sequencerFocusRightPane && !sequencerFocusActionsPane && sequencerSelectedRow == i);
        bool isCurrentStep = (currentStep == i);

        // Reset attributes and clear the line
        attrset(A_NORMAL);
        mvhline(row, leftCol, ' ', 30);  // Clear 30 chars for the row
        move(row, leftCol);

        // Apply cyan color to entire row if it's the current step
        if (isCurrentStep) {
            attron(COLOR_PAIR(1) | A_BOLD);
        }

        // Step number (1-based)
        mvprintw(row, leftCol, "%2d", i + 1);

        // Lock indicator - with selection highlight
        bool lockSelected = rowSelected && sequencerSelectedColumn == static_cast<int>(SequencerTrackerColumn::LOCK);
        if (!isCurrentStep && lockSelected) {
            attroff(COLOR_PAIR(1) | A_BOLD);  // Turn off cyan if needed
            attron(COLOR_PAIR(5) | A_BOLD);
        }
        if (step.locked) {
            mvprintw(row, leftCol + 3, "L");
        } else {
            mvprintw(row, leftCol + 3, " ");
        }
        if (!isCurrentStep && lockSelected) {
            attroff(COLOR_PAIR(5) | A_BOLD);
            attron(COLOR_PAIR(1) | A_BOLD);  // Restore cyan if needed
        }

        // Note field
        bool noteSelected = rowSelected && sequencerSelectedColumn == static_cast<int>(SequencerTrackerColumn::NOTE);
        if (!isCurrentStep && noteSelected) {
            attroff(COLOR_PAIR(1) | A_BOLD);
            attron(COLOR_PAIR(5) | A_BOLD);
        }
        if (step.active) {
            mvprintw(row, leftCol + 5, "%-6s", midiNoteToString(step.midiNote).c_str());
        } else {
            mvprintw(row, leftCol + 5, "---   ");
        }
        if (!isCurrentStep && noteSelected) {
            attroff(COLOR_PAIR(5) | A_BOLD);
            attron(COLOR_PAIR(1) | A_BOLD);
        }

        // Velocity field
        bool velSelected = rowSelected && sequencerSelectedColumn == static_cast<int>(SequencerTrackerColumn::VELOCITY);
        if (!isCurrentStep && velSelected) {
            attroff(COLOR_PAIR(1) | A_BOLD);
            attron(COLOR_PAIR(5) | A_BOLD);
        }
        if (step.active) {
            mvprintw(row, leftCol + 12, "%3d", step.velocity);
        } else {
            mvprintw(row, leftCol + 12, "---");
        }
        if (!isCurrentStep && velSelected) {
            attroff(COLOR_PAIR(5) | A_BOLD);
            attron(COLOR_PAIR(1) | A_BOLD);
        }

        // Gate field
        bool gateSelected = rowSelected && sequencerSelectedColumn == static_cast<int>(SequencerTrackerColumn::GATE);
        if (!isCurrentStep && gateSelected) {
            attroff(COLOR_PAIR(1) | A_BOLD);
            attron(COLOR_PAIR(5) | A_BOLD);
        }
        if (step.active) {
            mvprintw(row, leftCol + 16, "%3d%%", static_cast<int>(step.gateLength * 100.0f));
        } else {
            mvprintw(row, leftCol + 16, "--- ");
        }
        if (!isCurrentStep && gateSelected) {
            attroff(COLOR_PAIR(5) | A_BOLD);
            attron(COLOR_PAIR(1) | A_BOLD);
        }

        // Probability field
        bool probSelected = rowSelected && sequencerSelectedColumn == static_cast<int>(SequencerTrackerColumn::PROBABILITY);
        if (!isCurrentStep && probSelected) {
            attroff(COLOR_PAIR(1) | A_BOLD);
            attron(COLOR_PAIR(5) | A_BOLD);
        }
        if (step.active) {
            mvprintw(row, leftCol + 21, "%3d%%", static_cast<int>(step.probability * 100.0f));
        } else {
            mvprintw(row, leftCol + 21, "--- ");
        }
        if (!isCurrentStep && probSelected) {
            attroff(COLOR_PAIR(5) | A_BOLD);
            attron(COLOR_PAIR(1) | A_BOLD);
        }

        // Reset attributes at end of row
        attrset(A_NORMAL);

        row++;
    }
}

void UI::drawConfigPage() {
    int row = 3;
    
    // System information
    attron(A_BOLD);
    mvprintw(row++, 1, "DEVICE CONFIGURATION");
    attroff(A_BOLD);
    row++;
    
    attron(COLOR_PAIR(3));
    mvprintw(row++, 2, "Audio and MIDI device configuration");
    attroff(COLOR_PAIR(3));
    row++;
    
    // Audio device info
    attron(A_BOLD);
    mvprintw(row++, 1, "AUDIO DEVICE");
    attroff(A_BOLD);
    mvprintw(row++, 2, "Device: %s", audioDeviceName.c_str());
    mvprintw(row++, 2, "Sample Rate: %d Hz", audioSampleRate);
    mvprintw(row++, 2, "Buffer Size: %d samples", audioBufferSize);
    
    if (audioSampleRate > 0 && audioBufferSize > 0) {
        float latency = (audioBufferSize * 1000.0f) / audioSampleRate;
        mvprintw(row++, 2, "Latency: %.2f ms", latency);
    }
    
    row += 2;
    
    // MIDI device info
    attron(A_BOLD);
    mvprintw(row++, 1, "MIDI DEVICE");
    attroff(A_BOLD);
    mvprintw(row++, 2, "Device: %s", midiDeviceName.c_str());
    
    if (midiPortNum >= 0) {
        mvprintw(row++, 2, "Port: %d", midiPortNum);
        attron(COLOR_PAIR(2));
        mvprintw(row++, 2, "Status: Connected");
        attroff(COLOR_PAIR(2));
    } else {
        attron(COLOR_PAIR(4));
        mvprintw(row++, 2, "Status: Not connected");
        attroff(COLOR_PAIR(4));
    }
    
    row += 2;
    
    // Build info
    attron(A_BOLD);
    mvprintw(row++, 1, "BUILD INFO");
    attroff(A_BOLD);
    mvprintw(row++, 2, "Max Voices: 8 (polyphonic)");
    mvprintw(row++, 2, "Waveforms: Sine, Square, Sawtooth, Triangle");
    mvprintw(row++, 2, "Filters: Lowpass, Highpass, Shelving");
    mvprintw(row++, 2, "Reverb: Greyhole (Faust 2.37.3)");
}

void UI::addConsoleMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(consoleMutex);
    consoleMessages.push_back(message);
    if (consoleMessages.size() > MAX_CONSOLE_MESSAGES) {
        consoleMessages.pop_front();
    }
}

void UI::drawConsole() {
    int maxY = getmaxy(stdscr);
    int maxX = getmaxx(stdscr);
    
    // Console starts 7 lines from bottom
    int consoleStart = maxY - 7;
    
    // Draw separator
    attron(COLOR_PAIR(1));
    mvhline(consoleStart, 0, '-', maxX);
    attroff(COLOR_PAIR(1));
    
    // Draw console title
    attron(A_BOLD);
    mvprintw(consoleStart + 1, 1, "CONSOLE");
    attroff(A_BOLD);
    
    // Draw messages
    std::lock_guard<std::mutex> lock(consoleMutex);
    int row = consoleStart + 2;
    for (const auto& msg : consoleMessages) {
        if (row >= maxY - 2) break;  // Don't overwrite hotkey line
        attron(COLOR_PAIR(2));
        mvprintw(row++, 2, "%s", msg.c_str());
        attroff(COLOR_PAIR(2));
    }
}

void UI::drawHotkeyLine() {
    int maxY = getmaxy(stdscr);
    int maxX = getmaxx(stdscr);
    int row = maxY - 1;
    
    attron(COLOR_PAIR(1));
    mvhline(row - 1, 0, '-', maxX);
    attroff(COLOR_PAIR(1));
    
    if (numericInputActive) {
        mvprintw(row, 1, "Type value  |  Enter Confirm  |  Esc Cancel  |  Q Quit");
    } else if (sequencerScaleMenuActive) {
        mvprintw(row, 1, "Up/Down Choose Scale  |  Enter Confirm  |  Esc Cancel  |  Q Quit");
    } else if (textInputActive) {
        mvprintw(row, 1, "Type preset name  |  Enter Save  |  Esc Cancel  |  Q Quit");
    } else if (params->midiLearnActive.load()) {
        mvprintw(row, 1, "Move MIDI controller to assign  |  Esc Cancel  |  Q Quit");
    } else {
        mvprintw(row, 1, "Tab/Shift+Tab Page  |  Up/Dn Param  |  Left/Right Adjust  |  Enter Type  |  L Learn  |  Q Quit");
    }
}

void UI::draw(int activeVoices) {
    erase();  // Use erase() instead of clear() - doesn't cause flicker

    // If help is active, show help instead of normal UI
    if (helpActive) {
        drawHelpPage();
        refresh();
        return;
    }

    drawTabs();
    
    switch (currentPage) {
        case UIPage::MAIN:
            drawMainPage(activeVoices);
            drawConsole();  // Console only on main page
            break;
        case UIPage::BRAINWAVE:
            drawBrainwavePage();
            break;
        case UIPage::REVERB:
            drawReverbPage();
            break;
        case UIPage::FILTER:
            drawFilterPage();
            break;
        case UIPage::LOOPER:
            drawLooperPage();
            break;
        case UIPage::SEQUENCER:
            drawSequencerPage();
            break;
        case UIPage::CONFIG:
            drawConfigPage();
            break;
    }
    
    drawHotkeyLine();  // Always draw hotkey line at bottom
    
    // Draw input overlays if active
    if (textInputActive) {
        int maxY = getmaxy(stdscr);
        int maxX = getmaxx(stdscr);
        int height = 7;
        int width = 50;
        int startY = (maxY - height) / 2;
        int startX = (maxX - width) / 2;
        
        // Draw box
        attron(COLOR_PAIR(5));
        for (int y = startY; y < startY + height; ++y) {
            for (int x = startX; x < startX + width; ++x) {
                mvaddch(y, x, ' ');
            }
        }
        attroff(COLOR_PAIR(5));
        
        // Draw border
        attron(A_BOLD);
        mvhline(startY, startX, '-', width);
        mvhline(startY + height - 1, startX, '-', width);
        mvvline(startY, startX, '|', height);
        mvvline(startY, startX + width - 1, '|', height);
        attroff(A_BOLD);
        
        // Draw title
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(startY + 1, startX + 2, "Save Preset As:");
        attroff(COLOR_PAIR(5) | A_BOLD);
        
        // Draw input field
        mvprintw(startY + 3, startX + 2, "> %s_", textInputBuffer.c_str());
        
        // Draw hint
        attron(COLOR_PAIR(3));
        mvprintw(startY + 5, startX + 2, "Press Enter to save, Esc to cancel");
        attroff(COLOR_PAIR(3));
    } else if (numericInputActive) {
        int maxY = getmaxy(stdscr);
        int maxX = getmaxx(stdscr);
        int height = 7;
        int width = 40;
        int startY = (maxY - height) / 2;
        int startX = (maxX - width) / 2;
        
        // Draw box
        attron(COLOR_PAIR(5));
        for (int y = startY; y < startY + height; ++y) {
            for (int x = startX; x < startX + width; ++x) {
                mvaddch(y, x, ' ');
            }
        }
        attroff(COLOR_PAIR(5));
        
        // Draw border
        attron(A_BOLD);
        mvhline(startY, startX, '-', width);
        mvhline(startY + height - 1, startX, '-', width);
        mvvline(startY, startX, '|', height);
        mvvline(startY, startX + width - 1, '|', height);
        attroff(A_BOLD);
        
        const char* label = nullptr;
        if (numericInputIsSequencer) {
            switch (sequencerNumericContext.field) {
                case SequencerNumericField::NOTE: label = "MIDI Note / Name"; break;
                case SequencerNumericField::VELOCITY: label = "Velocity (0-127)"; break;
                case SequencerNumericField::GATE: label = "Gate % (0-200)"; break;
                case SequencerNumericField::PROBABILITY: label = "Probability % (0-100)"; break;
                case SequencerNumericField::TEMPO: label = "Tempo BPM (0.1-999)"; break;
                case SequencerNumericField::SCALE: label = "Scale"; break;
                case SequencerNumericField::ROOT: label = "Root Note"; break;
                case SequencerNumericField::EUCLID_HITS: label = "Euclid Hits"; break;
                case SequencerNumericField::EUCLID_STEPS: label = "Euclid Steps"; break;
                case SequencerNumericField::EUCLID_ROTATION: label = "Euclid Rotation"; break;
                case SequencerNumericField::SUBDIVISION: label = "Subdivision (1/..)"; break;
                case SequencerNumericField::MUTATE_AMOUNT: label = "Mutate % (0-100)"; break;
                case SequencerNumericField::MUTED: label = "Muted (on/off)"; break;
                case SequencerNumericField::SOLO: label = "Solo (on/off)"; break;
                default: label = "Value"; break;
            }
        } else {
            InlineParameter* param = getParameter(selectedParameterId);
            if (param) {
                label = param->name.c_str();
            }
        }

        if (label) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvprintw(startY + 1, startX + 2, "Enter %s:", label);
            attroff(COLOR_PAIR(5) | A_BOLD);
        }

        // Draw input field
        mvprintw(startY + 3, startX + 2, "> %s_", numericInputBuffer.c_str());

        // Draw hint
        attron(COLOR_PAIR(3));
        if (numericInputIsSequencer) {
            mvprintw(startY + 5, startX + 2, "Enter value, Esc cancel");
        } else {
            mvprintw(startY + 5, startX + 2, "Press Enter to confirm, Esc to cancel");
        }
        attroff(COLOR_PAIR(3));
    } else if (sequencerScaleMenuActive) {
        int maxY = getmaxy(stdscr);
        int maxX = getmaxx(stdscr);
        int optionCount = static_cast<int>(kScaleOrder.size());
        int height = optionCount + 4;
        if (height < 8) {
            height = 8;
        }
        int width = 36;
        int startY = std::max(1, (maxY - height) / 2);
        int startX = std::max(1, (maxX - width) / 2);

        for (int y = startY; y < startY + height; ++y) {
            for (int x = startX; x < startX + width; ++x) {
                mvaddch(y, x, ' ');
            }
        }

        attron(A_BOLD);
        mvhline(startY, startX, '-', width);
        mvhline(startY + height - 1, startX, '-', width);
        mvvline(startY, startX, '|', height);
        mvvline(startY, startX + width - 1, '|', height);
        mvprintw(startY + 1, startX + 2, "Select Scale");
        attroff(A_BOLD);

        for (int i = 0; i < optionCount; ++i) {
            int y = startY + 3 + i;
            const char* name = scaleDisplayName(kScaleOrder[i]);
            bool selected = (i == sequencerScaleMenuIndex);
            if (selected) {
                attron(COLOR_PAIR(5) | A_BOLD);
                mvprintw(y, startX + 2, "> %-20s", name);
                attroff(COLOR_PAIR(5) | A_BOLD);
            } else {
                mvprintw(y, startX + 2, "  %-20s", name);
            }
        }

        attron(COLOR_PAIR(3));
        mvprintw(startY + height - 2, startX + 2, "Enter confirm, Esc cancel");
        attroff(COLOR_PAIR(3));
    }
    
    refresh();
}

// Helper functions

void UI::refreshPresetList() {
    availablePresets = PresetManager::listPresets();
}

void UI::loadPreset(const std::string& filename) {
    if (PresetManager::loadPreset(filename, params)) {
        currentPresetName = filename;
    }
}

void UI::savePreset(const std::string& filename) {
    if (PresetManager::savePreset(filename, params)) {
        currentPresetName = filename;
        refreshPresetList();
    }
}

void UI::startTextInput() {
    textInputActive = true;
    textInputBuffer.clear();
}

void UI::handleTextInput(int ch) {
    if (ch == '\n' || ch == KEY_ENTER) {
        // Save preset with entered name
        if (!textInputBuffer.empty()) {
            savePreset(textInputBuffer);
        }
        finishTextInput();
    } else if (ch == 27) {  // Escape
        finishTextInput();
        clear();  // Clear screen to refresh properly
    } else if (ch == KEY_BACKSPACE || ch == 127) {
        if (!textInputBuffer.empty()) {
            textInputBuffer.pop_back();
        }
    } else if (ch >= 32 && ch < 127) {  // Printable characters
        if (textInputBuffer.length() < 30) {
            textInputBuffer += static_cast<char>(ch);
        }
    }
}

void UI::finishTextInput() {
    textInputActive = false;
    textInputBuffer.clear();
}

// Parameter management functions
void UI::initializeParameters() {
    parameters.clear();
    
    // MAIN page parameters - ALL support MIDI learn
    parameters.push_back({1, ParamType::ENUM, "Waveform", "", 0, 3, {"Sine", "Square", "Sawtooth", "Triangle"}, true, static_cast<int>(UIPage::MAIN)});
    parameters.push_back({2, ParamType::FLOAT, "Attack", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::MAIN)});
    parameters.push_back({3, ParamType::FLOAT, "Decay", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::MAIN)});
    parameters.push_back({4, ParamType::FLOAT, "Sustain", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MAIN)});
    parameters.push_back({5, ParamType::FLOAT, "Release", "s", 0.001f, 30.0f, {}, true, static_cast<int>(UIPage::MAIN)});
    parameters.push_back({6, ParamType::FLOAT, "Master Volume", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::MAIN)});

    // BRAINWAVE page parameters - ALL support MIDI learn
    parameters.push_back({10, ParamType::ENUM, "Mode", "", 0, 1, {"FREE", "KEY"}, true, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({11, ParamType::FLOAT, "Frequency", "Hz", 20.0f, 2000.0f, {}, true, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({12, ParamType::FLOAT, "Morph", "", 0.0001f, 0.9999f, {}, true, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({13, ParamType::FLOAT, "Duty", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({14, ParamType::INT, "Octave", "", -3, 3, {}, true, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({15, ParamType::BOOL, "LFO Enabled", "", 0, 1, {}, true, static_cast<int>(UIPage::BRAINWAVE)});
    parameters.push_back({16, ParamType::INT, "LFO Speed", "", 0, 9, {}, true, static_cast<int>(UIPage::BRAINWAVE)});

    // REVERB page parameters - ALL support MIDI learn
    parameters.push_back({20, ParamType::ENUM, "Reverb Type", "", 0, 4, {"Greyhole", "Plate", "Room", "Hall", "Spring"}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({21, ParamType::BOOL, "Reverb Enabled", "", 0, 1, {}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({22, ParamType::FLOAT, "Delay Time", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({23, ParamType::FLOAT, "Size", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({24, ParamType::FLOAT, "Damping", "", 0.0f, 0.99f, {}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({25, ParamType::FLOAT, "Mix", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({26, ParamType::FLOAT, "Decay", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({27, ParamType::FLOAT, "Diffusion", "", 0.0f, 0.99f, {}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({28, ParamType::FLOAT, "Mod Depth", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::REVERB)});
    parameters.push_back({29, ParamType::FLOAT, "Mod Freq", "", 0.0f, 10.0f, {}, true, static_cast<int>(UIPage::REVERB)});

    // FILTER page parameters - ALL support MIDI learn
    parameters.push_back({30, ParamType::ENUM, "Filter Type", "", 0, 3, {"Lowpass", "Highpass", "High Shelf", "Low Shelf"}, true, static_cast<int>(UIPage::FILTER)});
    parameters.push_back({31, ParamType::BOOL, "Filter Enabled", "", 0, 1, {}, true, static_cast<int>(UIPage::FILTER)});
    parameters.push_back({32, ParamType::FLOAT, "Cutoff", "Hz", 20.0f, 20000.0f, {}, true, static_cast<int>(UIPage::FILTER)});
    parameters.push_back({33, ParamType::FLOAT, "Gain", "dB", -24.0f, 24.0f, {}, true, static_cast<int>(UIPage::FILTER)});

    // LOOPER page parameters - ALL support MIDI learn
    parameters.push_back({40, ParamType::INT, "Current Loop", "", 0, 3, {}, true, static_cast<int>(UIPage::LOOPER)});
    parameters.push_back({41, ParamType::FLOAT, "Overdub Mix", "", 0.0f, 1.0f, {}, true, static_cast<int>(UIPage::LOOPER)});
}

InlineParameter* UI::getParameter(int id) {
    for (auto& param : parameters) {
        if (param.id == id) {
            return &param;
        }
    }
    return nullptr;
}

std::vector<int> UI::getParameterIdsForPage(UIPage page) {
    std::vector<int> ids;
    int pageInt = static_cast<int>(page);
    for (const auto& param : parameters) {
        if (param.page == pageInt) {
            ids.push_back(param.id);
        }
    }
    return ids;
}

float UI::getParameterValue(int id) {
    switch (id) {
        case 1: return static_cast<float>(params->waveform.load());
        case 2: return params->attack.load();
        case 3: return params->decay.load();
        case 4: return params->sustain.load();
        case 5: return params->release.load();
        case 6: return params->masterVolume.load();
        case 10: return static_cast<float>(params->brainwaveMode.load());
        case 11: return params->brainwaveFreq.load();
        case 12: return params->brainwaveMorph.load();
        case 13: return params->brainwaveDuty.load();
        case 14: return static_cast<float>(params->brainwaveOctave.load());
        case 15: return params->brainwaveLFOEnabled.load() ? 1.0f : 0.0f;
        case 16: return static_cast<float>(params->brainwaveLFOSpeed.load());
        case 20: return static_cast<float>(params->reverbType.load());
        case 21: return params->reverbEnabled.load() ? 1.0f : 0.0f;
        case 22: return params->reverbDelayTime.load();
        case 23: return params->reverbSize.load();
        case 24: return params->reverbDamping.load();
        case 25: return params->reverbMix.load();
        case 26: return params->reverbDecay.load();
        case 27: return params->reverbDiffusion.load();
        case 28: return params->reverbModDepth.load();
        case 29: return params->reverbModFreq.load();
        case 30: return static_cast<float>(params->filterType.load());
        case 31: return params->filterEnabled.load() ? 1.0f : 0.0f;
        case 32: return params->filterCutoff.load();
        case 33: return params->filterGain.load();
        case 40: return static_cast<float>(params->currentLoop.load());
        case 41: return params->overdubMix.load();
        default: return 0.0f;
    }
}

void UI::setParameterValue(int id, float value) {
    switch (id) {
        case 1: params->waveform = static_cast<int>(value); break;
        case 2: params->attack = value; break;
        case 3: params->decay = value; break;
        case 4: params->sustain = value; break;
        case 5: params->release = value; break;
        case 6: params->masterVolume = value; break;
        case 10: params->brainwaveMode = static_cast<int>(value); break;
        case 11: params->brainwaveFreq = value; break;
        case 12: params->brainwaveMorph = value; break;
        case 13: params->brainwaveDuty = value; break;
        case 14: params->brainwaveOctave = static_cast<int>(value); break;
        case 15: params->brainwaveLFOEnabled = (value > 0.5f); break;
        case 16: params->brainwaveLFOSpeed = static_cast<int>(value); break;
        case 20: params->reverbType = static_cast<int>(value); break;
        case 21: params->reverbEnabled = (value > 0.5f); break;
        case 22: params->reverbDelayTime = value; break;
        case 23: params->reverbSize = value; break;
        case 24: params->reverbDamping = value; break;
        case 25: params->reverbMix = value; break;
        case 26: params->reverbDecay = value; break;
        case 27: params->reverbDiffusion = value; break;
        case 28: params->reverbModDepth = value; break;
        case 29: params->reverbModFreq = value; break;
        case 30: params->filterType = static_cast<int>(value); break;
        case 31: params->filterEnabled = (value > 0.5f); break;
        case 32: params->filterCutoff = value; break;
        case 33: params->filterGain = value; break;
        case 40: params->currentLoop = static_cast<int>(value); break;
        case 41: params->overdubMix = value; break;
    }
}

void UI::adjustParameter(int id, bool increase) {
    InlineParameter* param = getParameter(id);
    if (!param) return;
    
    float currentValue = getParameterValue(id);
    float newValue = currentValue;
    
    switch (param->type) {
        case ParamType::FLOAT: {
            float step = (param->max_val - param->min_val) * 0.01f; // 1% steps
            
            // Special exponential handling for certain parameters
            if (id == 2 || id == 3 || id == 5) { // Attack, Decay, Release - exponential scaling
                if (increase) {
                    newValue = std::min(param->max_val, currentValue * 1.1f);
                } else {
                    newValue = std::max(param->min_val, currentValue / 1.1f);
                }
            } else if (id == 11) { // Brainwave frequency - musical scaling
                if (increase) {
                    newValue = std::min(param->max_val, currentValue * 1.059463f); // semitone ratio
                } else {
                    newValue = std::max(param->min_val, currentValue / 1.059463f);
                }
            } else if (id == 32) { // Filter cutoff - logarithmic scaling
                if (increase) {
                    newValue = std::min(param->max_val, currentValue * 1.1f);
                } else {
                    newValue = std::max(param->min_val, currentValue / 1.1f);
                }
            } else {
                // Linear scaling for other parameters
                if (increase) {
                    newValue = std::min(param->max_val, currentValue + step);
                } else {
                    newValue = std::max(param->min_val, currentValue - step);
                }
            }
            break;
        }
        case ParamType::INT: {
            int intValue = static_cast<int>(currentValue);
            if (increase) {
                intValue = std::min(static_cast<int>(param->max_val), intValue + 1);
            } else {
                intValue = std::max(static_cast<int>(param->min_val), intValue - 1);
            }
            newValue = static_cast<float>(intValue);
            break;
        }
        case ParamType::ENUM: {
            int enumValue = static_cast<int>(currentValue);
            if (increase) {
                enumValue = std::min(static_cast<int>(param->max_val), enumValue + 1);
            } else {
                enumValue = std::max(static_cast<int>(param->min_val), enumValue - 1);
            }
            newValue = static_cast<float>(enumValue);
            break;
        }
        case ParamType::BOOL: {
            newValue = (currentValue > 0.5f) ? 0.0f : 1.0f;
            break;
        }
    }
    
    setParameterValue(id, newValue);
}

std::string UI::getParameterDisplayString(int id) {
    InlineParameter* param = getParameter(id);
    if (!param) return "";
    
    float value = getParameterValue(id);
    
    switch (param->type) {
        case ParamType::FLOAT: {
            if (param->unit.empty()) {
                return std::to_string(value);
            } else {
                return std::to_string(value) + " " + param->unit;
            }
        }
        case ParamType::INT: {
            return std::to_string(static_cast<int>(value));
        }
        case ParamType::ENUM: {
            int enumIndex = static_cast<int>(value);
            if (enumIndex >= 0 && enumIndex < static_cast<int>(param->enum_values.size())) {
                return param->enum_values[enumIndex];
            }
            return "Unknown";
        }
        case ParamType::BOOL: {
            return (value > 0.5f) ? "ON" : "OFF";
        }
    }
    return "";
}

void UI::startNumericInput(int id) {
    numericInputActive = true;
    selectedParameterId = id;
    numericInputBuffer.clear();
    numericInputIsSequencer = false;
    sequencerNumericContext = SequencerNumericContext();
}

void UI::finishNumericInput() {
    if (!numericInputActive) return;
    if (numericInputIsSequencer) {
        if (!numericInputBuffer.empty()) {
            applySequencerNumericInput(numericInputBuffer);
        }
        numericInputIsSequencer = false;
        sequencerNumericContext = SequencerNumericContext();
        numericInputActive = false;
        numericInputBuffer.clear();
        return;
    }

    InlineParameter* param = getParameter(selectedParameterId);
    if (param && !numericInputBuffer.empty()) {
        try {
            float value = std::stof(numericInputBuffer);
            value = std::max(param->min_val, std::min(param->max_val, value));
            setParameterValue(selectedParameterId, value);
        } catch (...) {
            // Invalid input, ignore
        }
    }

    numericInputActive = false;
    numericInputBuffer.clear();
}

void UI::startMidiLearn(int id) {
    InlineParameter* param = getParameter(id);
    if (param && param->supports_midi_learn) {
        params->midiLearnActive = true;
        params->midiLearnParameterId = id;

        // Record start time for timeout
        auto now = std::chrono::steady_clock::now();
        double currentTime = std::chrono::duration<double>(now.time_since_epoch()).count();
        params->midiLearnStartTime = currentTime;

        addConsoleMessage("MIDI Learn: " + param->name + " (10s timeout) - move a controller");
    } else if (param && !param->supports_midi_learn) {
        addConsoleMessage("MIDI Learn not available for " + param->name);
    }
}

void UI::finishMidiLearn() {
    params->midiLearnActive = false;
    params->midiLearnParameterId = -1;
}

std::string UI::getParameterName(int id) {
    InlineParameter* param = getParameter(id);
    return param ? param->name : "Unknown";
}

void UI::writeToWaveformBuffer(float sample) {
    int pos = waveformBufferWritePos.load(std::memory_order_relaxed);
    waveformBuffer[pos] = sample;
    waveformBufferWritePos.store((pos + 1) % WAVEFORM_BUFFER_SIZE, std::memory_order_relaxed);
}

void UI::ensureSequencerSelectionInRange() {
    if (!sequencer) return;

    Pattern& pattern = sequencer->getPattern();
    int maxRows = std::min(16, pattern.getLength());
    if (maxRows <= 0) {
        maxRows = 1;
    }

    if (sequencerSelectedRow < 0) {
        sequencerSelectedRow = maxRows - 1;
    } else if (sequencerSelectedRow >= maxRows) {
        sequencerSelectedRow = 0;
    }

    int maxColumn = static_cast<int>(SequencerTrackerColumn::PROBABILITY);
    if (sequencerSelectedColumn < static_cast<int>(SequencerTrackerColumn::LOCK)) {
        sequencerSelectedColumn = static_cast<int>(SequencerTrackerColumn::LOCK);
    } else if (sequencerSelectedColumn > maxColumn) {
        sequencerSelectedColumn = maxColumn;
    }

    if (sequencerRightSelection < 0) {
        sequencerRightSelection = 0;
    }
    if (!kSequencerInfoEntries.empty() && sequencerRightSelection >= static_cast<int>(kSequencerInfoEntries.size())) {
        sequencerRightSelection = static_cast<int>(kSequencerInfoEntries.size()) - 1;
    }
}

void UI::cancelSequencerNumericInput() {
    numericInputIsSequencer = false;
    sequencerNumericContext = SequencerNumericContext();
}

void UI::startSequencerScaleMenu() {
    if (!sequencer) return;

    numericInputActive = false;
    numericInputBuffer.clear();

    Track& track = sequencer->getCurrentTrack();
    MusicalConstraints& constraints = track.getConstraints();
    Scale current = constraints.getScale();
    auto it = std::find(kScaleOrder.begin(), kScaleOrder.end(), current);
    if (it == kScaleOrder.end()) {
        sequencerScaleMenuIndex = 0;
    } else {
        sequencerScaleMenuIndex = static_cast<int>(std::distance(kScaleOrder.begin(), it));
    }
    sequencerScaleMenuActive = true;
}

void UI::finishSequencerScaleMenu(bool applySelection) {
    if (!sequencerScaleMenuActive) return;

    if (applySelection && sequencer && !kScaleOrder.empty()) {
        int clampedIndex = std::clamp(sequencerScaleMenuIndex, 0,
                                      static_cast<int>(kScaleOrder.size()) - 1);
        Track& track = sequencer->getCurrentTrack();
        MusicalConstraints& constraints = track.getConstraints();
        Scale chosen = kScaleOrder[clampedIndex];
        if (constraints.getScale() != chosen) {
            constraints.setScale(chosen);
            sequencer->regenerateUnlocked();
        }
    }

    sequencerScaleMenuActive = false;
}

void UI::handleSequencerScaleMenuInput(int ch) {
    if (!sequencerScaleMenuActive) return;

    int optionCount = static_cast<int>(kScaleOrder.size());
    if (optionCount <= 0) {
        finishSequencerScaleMenu(false);
        return;
    }

    switch (ch) {
        case KEY_UP:
        case 'k':
        case 'K':
            sequencerScaleMenuIndex =
                (sequencerScaleMenuIndex - 1 + optionCount) % optionCount;
            break;
        case KEY_DOWN:
        case 'j':
        case 'J':
            sequencerScaleMenuIndex =
                (sequencerScaleMenuIndex + 1) % optionCount;
            break;
        case '\n':
        case KEY_ENTER:
            finishSequencerScaleMenu(true);
            break;
        case 27:  // Escape
            finishSequencerScaleMenu(false);
            break;
        default:
            break;
    }
}

bool UI::handleSequencerInput(int ch) {
    if (!sequencer) {
        return false;
    }

    Pattern& pattern = sequencer->getPattern();
    int patternLength = pattern.getLength();
    if (patternLength <= 0) {
        return false;
    }

    ensureSequencerSelectionInRange();

    int maxRows = std::min(16, patternLength);
    if (maxRows <= 0) {
        maxRows = 1;
    }

    auto wrapIndex = [](int value, int max) {
        if (max <= 0) return 0;
        if (value < 0) {
            value = (value % max + max) % max;
        } else if (value >= max) {
            value %= max;
        }
        return value;
    };

    switch (ch) {
        case KEY_UP:
            if (sequencerFocusActionsPane) {
                if (sequencerActionsRow > 0) {
                    --sequencerActionsRow;
                } else {
                    // Move from actions to parameters
                    sequencerFocusActionsPane = false;
                    sequencerFocusRightPane = true;
                    sequencerRightSelection = static_cast<int>(kSequencerInfoEntries.size()) - 1;
                }
            } else if (sequencerFocusRightPane) {
                if (sequencerRightSelection > 0) {
                    --sequencerRightSelection;
                }
            } else {
                sequencerSelectedRow = wrapIndex(sequencerSelectedRow - 1, maxRows);
            }
            return true;

        case KEY_DOWN:
            if (sequencerFocusActionsPane) {
                if (sequencerActionsRow < 4) {
                    ++sequencerActionsRow;
                }
            } else if (sequencerFocusRightPane) {
                if (sequencerRightSelection < static_cast<int>(kSequencerInfoEntries.size()) - 1) {
                    ++sequencerRightSelection;
                } else {
                    // Move from parameters to actions
                    sequencerFocusRightPane = false;
                    sequencerFocusActionsPane = true;
                    sequencerActionsRow = 0;
                    sequencerActionsColumn = 0;
                }
            } else {
                sequencerSelectedRow = wrapIndex(sequencerSelectedRow + 1, maxRows);
            }
            return true;

        case KEY_LEFT:
            if (sequencerFocusActionsPane) {
                if (sequencerActionsRow >= 2 && sequencerActionsColumn > 0) {
                    --sequencerActionsColumn;
                } else {
                    // Move from actions to tracker
                    sequencerFocusActionsPane = false;
                }
            } else if (sequencerFocusRightPane) {
                sequencerFocusRightPane = false;
            } else if (sequencerSelectedColumn > static_cast<int>(SequencerTrackerColumn::LOCK)) {
                --sequencerSelectedColumn;
            }
            return true;

        case KEY_RIGHT:
            if (sequencerFocusActionsPane) {
                if (sequencerActionsRow >= 2 && sequencerActionsColumn < 4) {
                    ++sequencerActionsColumn;
                } else if (sequencerActionsRow < 2) {
                    // On Generate/Clear, move right to next button
                    if (sequencerActionsRow == 0) sequencerActionsRow = 1;
                }
            } else if (!sequencerFocusRightPane) {
                if (sequencerSelectedColumn < static_cast<int>(SequencerTrackerColumn::PROBABILITY)) {
                    ++sequencerSelectedColumn;
                } else {
                    sequencerFocusRightPane = true;
                    sequencerRightSelection = 0;
                }
            }
            return true;

        case KEY_SR:    // Shift + Up (increase)
        case '+':
        case '=': {
            int direction = 1;
            if (sequencerFocusActionsPane) {
                // No adjustment in actions pane
            } else if (sequencerFocusRightPane) {
                adjustSequencerInfoField(sequencerRightSelection, direction);
            } else {
                adjustSequencerTrackerField(sequencerSelectedRow, sequencerSelectedColumn, direction);
            }
            return true;
        }

        case KEY_SF:    // Shift + Down (decrease)
        case '-':
        case '_': {
            int direction = -1;
            if (sequencerFocusActionsPane) {
                // No adjustment in actions pane
            } else if (sequencerFocusRightPane) {
                adjustSequencerInfoField(sequencerRightSelection, direction);
            } else {
                adjustSequencerTrackerField(sequencerSelectedRow, sequencerSelectedColumn, direction);
            }
            return true;
        }

        case '\n':
        case KEY_ENTER:
            if (sequencerFocusActionsPane) {
                executeSequencerAction(sequencerActionsRow, sequencerActionsColumn);
            } else if (sequencerFocusRightPane) {
                if (sequencerRightSelection >= 0 && sequencerRightSelection < static_cast<int>(kSequencerInfoEntries.size())) {
                    const auto& entry = kSequencerInfoEntries[sequencerRightSelection];
                    if (entry.editable) {
                        // Boolean fields toggle directly without input
                        if (entry.field == SequencerInfoField::MUTED) {
                            Track& track = sequencer->getCurrentTrack();
                            track.setMuted(!track.isMuted());
                        } else if (entry.field == SequencerInfoField::SOLO) {
                            Track& track = sequencer->getCurrentTrack();
                            track.setSolo(!track.isSolo());
                        } else if (entry.field == SequencerInfoField::SCALE) {
                            startSequencerScaleMenu();
                        } else {
                            startSequencerInfoNumericInput(sequencerRightSelection);
                        }
                    }
                }
            } else {
                if (sequencerSelectedColumn == static_cast<int>(SequencerTrackerColumn::LOCK)) {
                    PatternStep& step = pattern.getStep(sequencerSelectedRow);
                    step.locked = !step.locked;
                } else {
                    startSequencerNumericInput(sequencerSelectedRow, sequencerSelectedColumn);
                }
            }
            return true;

        default:
            break;
    }

    return false;
}

void UI::adjustSequencerTrackerField(int row, int column, int direction) {
    if (!sequencer) return;
    Pattern& pattern = sequencer->getPattern();
    if (row < 0 || row >= pattern.getLength()) return;

    PatternStep& step = pattern.getStep(row);
    SequencerTrackerColumn col = static_cast<SequencerTrackerColumn>(column);

    direction = (direction >= 0) ? 1 : -1;

    switch (col) {
        case SequencerTrackerColumn::LOCK: {
            step.locked = (direction > 0);
            break;
        }
        case SequencerTrackerColumn::NOTE: {
            step.active = true;
            int midi = std::clamp(step.midiNote + direction, 0, 127);
            step.midiNote = midi;
            break;
        }
        case SequencerTrackerColumn::VELOCITY: {
            step.active = true;
            int velocity = std::clamp(step.velocity + direction * 5, 1, 127);
            step.velocity = velocity;
            break;
        }
        case SequencerTrackerColumn::GATE: {
            step.active = true;
            float gate = step.gateLength + static_cast<float>(direction) * 0.05f;
            gate = std::max(0.05f, std::min(2.0f, gate));
            step.gateLength = gate;
            break;
        }
        case SequencerTrackerColumn::PROBABILITY: {
            step.active = true;
            float prob = step.probability + static_cast<float>(direction) * 0.05f;
            prob = std::max(0.0f, std::min(1.0f, prob));
            step.probability = prob;
            break;
        }
    }

    ensureSequencerSelectionInRange();
}

void UI::adjustSequencerInfoField(int infoIndex, int direction) {
    if (!sequencer) return;
    if (infoIndex < 0 || infoIndex >= static_cast<int>(kSequencerInfoEntries.size())) return;
    const auto& entry = kSequencerInfoEntries[infoIndex];
    if (!entry.editable) return;

    direction = (direction >= 0) ? 1 : -1;

    Track& track = sequencer->getCurrentTrack();
    Pattern& pattern = track.getPattern();
    MusicalConstraints& constraints = track.getConstraints();
    EuclideanPattern& euclidean = track.getEuclideanPattern();

    switch (entry.field) {
        case SequencerInfoField::TEMPO: {
            double tempo = sequencer->getTempo();
            tempo = std::clamp(tempo + direction * 1.0, 0.1, 999.0);
            sequencer->setTempo(tempo);
            break;
        }
        case SequencerInfoField::ROOT: {
            int root = constraints.getRootNote();
            root = (root + direction + 12) % 12;
            constraints.setRootNote(root);
            int gravityOctave = constraints.getOctaveMin();
            constraints.setGravityNote(gravityOctave * 12 + root);
            sequencer->regenerateUnlocked();
            break;
        }
        case SequencerInfoField::SCALE: {
            Scale current = constraints.getScale();
            auto it = std::find(kScaleOrder.begin(), kScaleOrder.end(), current);
            if (it == kScaleOrder.end()) it = kScaleOrder.begin();
            int index = static_cast<int>(std::distance(kScaleOrder.begin(), it));
            index = (index + direction + static_cast<int>(kScaleOrder.size())) % static_cast<int>(kScaleOrder.size());
            constraints.setScale(kScaleOrder[index]);
            sequencer->regenerateUnlocked();
            break;
        }
        case SequencerInfoField::EUCLID_HITS: {
            int hits = std::clamp(euclidean.getHits() + direction, 0, euclidean.getSteps());
            euclidean.setHits(hits);
            sequencer->regenerateUnlocked();
            break;
        }
        case SequencerInfoField::EUCLID_STEPS: {
            int steps = std::clamp(euclidean.getSteps() + direction, 1, 64);
            euclidean.setSteps(steps);
            pattern.setLength(steps);
            sequencerSelectedRow = std::min(sequencerSelectedRow, steps - 1);
            sequencer->regenerateUnlocked();
            break;
        }
        case SequencerInfoField::SUBDIVISION: {
            Subdivision current = pattern.getResolution();
            auto it = std::find(kSubdivisionOrder.begin(), kSubdivisionOrder.end(), current);
            if (it == kSubdivisionOrder.end()) it = kSubdivisionOrder.begin();
            int index = static_cast<int>(std::distance(kSubdivisionOrder.begin(), it));
            index = (index + direction + static_cast<int>(kSubdivisionOrder.size())) % static_cast<int>(kSubdivisionOrder.size());
            Subdivision newSubdiv = kSubdivisionOrder[index];
            track.setSubdivision(newSubdiv);
            break;
        }
        case SequencerInfoField::MUTATE_AMOUNT: {
            sequencerMutateAmount = std::clamp(sequencerMutateAmount + direction * 5.0f, 0.0f, 100.0f);
            break;
        }
        case SequencerInfoField::MUTED: {
            track.setMuted(!track.isMuted());
            break;
        }
        case SequencerInfoField::SOLO: {
            track.setSolo(!track.isSolo());
            break;
        }
        default:
            break;
    }

    ensureSequencerSelectionInRange();
}

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

void UI::startSequencerNumericInput(int row, int column) {
    if (!sequencer) return;
    sequencerNumericContext = SequencerNumericContext();
    sequencerNumericContext.row = row;
    sequencerNumericContext.column = column;

    SequencerTrackerColumn col = static_cast<SequencerTrackerColumn>(column);
    switch (col) {
        case SequencerTrackerColumn::NOTE:
            sequencerNumericContext.field = SequencerNumericField::NOTE;
            break;
        case SequencerTrackerColumn::VELOCITY:
            sequencerNumericContext.field = SequencerNumericField::VELOCITY;
            break;
        case SequencerTrackerColumn::GATE:
            sequencerNumericContext.field = SequencerNumericField::GATE;
            break;
        case SequencerTrackerColumn::PROBABILITY:
            sequencerNumericContext.field = SequencerNumericField::PROBABILITY;
            break;
        case SequencerTrackerColumn::LOCK:
        default:
            sequencerNumericContext.field = SequencerNumericField::NONE;
            break;
    }

    if (sequencerNumericContext.field == SequencerNumericField::NONE) {
        return;
    }

    numericInputActive = true;
    numericInputIsSequencer = true;
    numericInputBuffer.clear();
    selectedParameterId = -1;
}

void UI::startSequencerInfoNumericInput(int infoIndex) {
    if (!sequencer) return;
    if (infoIndex < 0 || infoIndex >= static_cast<int>(kSequencerInfoEntries.size())) return;
    const auto& entry = kSequencerInfoEntries[infoIndex];
    if (!entry.editable) return;

    sequencerNumericContext = SequencerNumericContext();
    sequencerNumericContext.infoIndex = infoIndex;

    switch (entry.field) {
        case SequencerInfoField::TEMPO:
            sequencerNumericContext.field = SequencerNumericField::TEMPO;
            break;
        case SequencerInfoField::SCALE:
            sequencerNumericContext.field = SequencerNumericField::SCALE;
            break;
        case SequencerInfoField::ROOT:
            sequencerNumericContext.field = SequencerNumericField::ROOT;
            break;
        case SequencerInfoField::EUCLID_HITS:
            sequencerNumericContext.field = SequencerNumericField::EUCLID_HITS;
            break;
        case SequencerInfoField::EUCLID_STEPS:
            sequencerNumericContext.field = SequencerNumericField::EUCLID_STEPS;
            break;
        case SequencerInfoField::SUBDIVISION:
            sequencerNumericContext.field = SequencerNumericField::SUBDIVISION;
            break;
        case SequencerInfoField::MUTATE_AMOUNT:
            sequencerNumericContext.field = SequencerNumericField::MUTATE_AMOUNT;
            break;
        case SequencerInfoField::MUTED:
            sequencerNumericContext.field = SequencerNumericField::MUTED;
            break;
        case SequencerInfoField::SOLO:
            sequencerNumericContext.field = SequencerNumericField::SOLO;
            break;
        default:
            sequencerNumericContext.field = SequencerNumericField::NONE;
            break;
    }

    if (sequencerNumericContext.field == SequencerNumericField::NONE) {
        return;
    }

    numericInputActive = true;
    numericInputIsSequencer = true;
    numericInputBuffer.clear();
    selectedParameterId = -1;
}

void UI::applySequencerNumericInput(const std::string& text) {
    if (!sequencer) return;

    auto trim = [](const std::string& in) {
        size_t start = 0;
        while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start]))) ++start;
        size_t end = in.size();
        while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1]))) --end;
        return in.substr(start, end - start);
    };

    std::string trimmed = trim(text);
    if (trimmed.empty()) return;

    Track& track = sequencer->getCurrentTrack();
    Pattern& pattern = track.getPattern();
    MusicalConstraints& constraints = track.getConstraints();
    EuclideanPattern& euclidean = track.getEuclideanPattern();

    switch (sequencerNumericContext.field) {
        case SequencerNumericField::NOTE: {
            int row = sequencerNumericContext.row;
            if (row < 0 || row >= pattern.getLength()) break;
            PatternStep& step = pattern.getStep(row);
            int midi = -1;
            if (parseNoteText(trimmed, midi)) {
                step.active = true;
                step.midiNote = std::clamp(midi, 0, 127);
            }
            break;
        }
        case SequencerNumericField::VELOCITY: {
            int row = sequencerNumericContext.row;
            if (row < 0 || row >= pattern.getLength()) break;
            PatternStep& step = pattern.getStep(row);
            try {
                int value = std::stoi(trimmed);
                value = std::clamp(value, 1, 127);
                step.active = true;
                step.velocity = value;
            } catch (...) {}
            break;
        }
        case SequencerNumericField::GATE: {
            int row = sequencerNumericContext.row;
            if (row < 0 || row >= pattern.getLength()) break;
            PatternStep& step = pattern.getStep(row);
            try {
                float value = std::stof(trimmed);
                if (value > 2.0f) {
                    value /= 100.0f;
                }
                value = std::max(0.05f, std::min(2.0f, value));
                step.active = true;
                step.gateLength = value;
            } catch (...) {}
            break;
        }
        case SequencerNumericField::PROBABILITY: {
            int row = sequencerNumericContext.row;
            if (row < 0 || row >= pattern.getLength()) break;
            PatternStep& step = pattern.getStep(row);
            try {
                float value = std::stof(trimmed);
                if (value > 1.0f) {
                    value /= 100.0f;
                }
                value = std::max(0.0f, std::min(1.0f, value));
                step.active = true;
                step.probability = value;
            } catch (...) {}
            break;
        }
        case SequencerNumericField::TEMPO: {
            try {
                double tempo = std::stod(trimmed);
                tempo = std::clamp(tempo, 0.1, 999.0);
                sequencer->setTempo(tempo);
            } catch (...) {}
            break;
        }
        case SequencerNumericField::SCALE: {
            Scale newScale;
            if (parseScaleText(trimmed, newScale)) {
                constraints.setScale(newScale);
                sequencer->regenerateUnlocked();
            }
            break;
        }
        case SequencerNumericField::ROOT: {
            int root;
            if (parseRootText(trimmed, root)) {
                constraints.setRootNote(root);
                int gravityOctave = constraints.getOctaveMin();
                constraints.setGravityNote(gravityOctave * 12 + root);
                sequencer->regenerateUnlocked();
            }
            break;
        }
        case SequencerNumericField::EUCLID_HITS: {
            try {
                int hits = std::stoi(trimmed);
                hits = std::clamp(hits, 0, euclidean.getSteps());
                euclidean.setHits(hits);
                sequencer->regenerateUnlocked();
            } catch (...) {}
            break;
        }
        case SequencerNumericField::EUCLID_STEPS: {
            try {
                int steps = std::stoi(trimmed);
                steps = std::clamp(steps, 1, 64);
                euclidean.setSteps(steps);
                pattern.setLength(steps);
                sequencerSelectedRow = std::min(sequencerSelectedRow, steps - 1);
                sequencer->regenerateUnlocked();
            } catch (...) {}
            break;
        }
        case SequencerNumericField::EUCLID_ROTATION: {
            try {
                int rot = std::stoi(trimmed);
                euclidean.setRotation(rot);
            } catch (...) {}
            break;
        }
        case SequencerNumericField::SUBDIVISION: {
            Subdivision subdiv;
            if (parseSubdivisionText(trimmed, subdiv)) {
                track.setSubdivision(subdiv);
            }
            break;
        }
        case SequencerNumericField::MUTATE_AMOUNT: {
            try {
                float value = std::stof(trimmed);
                sequencerMutateAmount = std::clamp(value, 0.0f, 100.0f);
            } catch (...) {}
            break;
        }
        case SequencerNumericField::MUTED: {
            std::string lower;
            lower.reserve(trimmed.size());
            for (char c : trimmed) {
                if (!std::isspace(static_cast<unsigned char>(c))) {
                    lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                }
            }
            if (lower == "1" || lower == "on" || lower == "yes" || lower == "true") {
                track.setMuted(true);
            } else if (lower == "0" || lower == "off" || lower == "no" || lower == "false") {
                track.setMuted(false);
            }
            break;
        }
        case SequencerNumericField::SOLO: {
            std::string lower;
            lower.reserve(trimmed.size());
            for (char c : trimmed) {
                if (!std::isspace(static_cast<unsigned char>(c))) {
                    lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                }
            }
            if (lower == "1" || lower == "on" || lower == "yes" || lower == "true") {
                track.setSolo(true);
            } else if (lower == "0" || lower == "off" || lower == "no" || lower == "false") {
                track.setSolo(false);
            }
            break;
        }
        default:
            break;
    }

    ensureSequencerSelectionInRange();
}

// Help system implementation
void UI::showHelp() {
    helpActive = true;
    helpScrollOffset = 0;
    clear();
}

void UI::hideHelp() {
    helpActive = false;
    helpScrollOffset = 0;
    clear();
}

std::string UI::getHelpContent(UIPage page) {
    std::string content;

    switch (page) {
        case UIPage::MAIN:
            content = R"(
=== MAIN PAGE ===

CONTROLS:
  Tab/Shift+Tab  - Navigate between pages
  Up/Down        - Select parameter
  Left/Right     - Adjust parameter value
  Enter          - Type exact value
  L              - MIDI Learn (assign MIDI CC to parameter)
  H              - Show this help
  Q              - Quit application

PARAMETERS:
  Waveform   - Oscillator waveform type
  Attack     - Envelope attack time (0.001-30s)
  Decay      - Envelope decay time (0.001-30s)
  Sustain    - Envelope sustain level (0-100%)
  Release    - Envelope release time (0.001-30s)
  Volume     - Master output volume

ABOUT:
Wakefield is a polyphonic wavetable synthesizer with brainwave oscillators,
built-in reverb, filters, loopers, and a generative MIDI sequencer for
creating dark ambient soundscapes.

The MAIN page controls the basic synthesis parameters including envelope
(ADSR) and master volume. All parameters support MIDI learn for external
control via MIDI controllers.
)";
            break;

        case UIPage::BRAINWAVE:
            content = R"(
=== BRAINWAVE OSCILLATOR ===

CONTROLS:
  [Same global controls as MAIN page]

PARAMETERS:
  Mode       - FREE (manual freq) or KEY (MIDI tracking)
  Frequency  - Base frequency or offset (20-2000 Hz)
  Morph      - Wavetable position (0-255 frames)
  Duty       - Pulse width for PWM synthesis
  Octave     - Octave shift (-3 to +3)
  LFO Enable - Enable modulation LFO
  LFO Speed  - LFO rate (0-9, slow to fast)

ABOUT:
The Brainwave oscillator is a wavetable synth engine that morphs through
256 frames of complex waveforms. In FREE mode, you control the frequency
directly. In KEY mode, it tracks MIDI notes while the Frequency parameter
acts as an offset or detune amount.

The Morph parameter sweeps through the wavetable frames, allowing smooth
transitions from simple to complex timbres. The LFO can modulate the morph
parameter for evolving textures perfect for dark ambient pads and drones.
)";
            break;

        case UIPage::REVERB:
            content = R"(
=== REVERB ===

CONTROLS:
  [Same global controls as MAIN page]

PARAMETERS:
  Type       - Reverb algorithm (Greyhole, Plate, Room, Hall, Spring)
  Enabled    - Bypass reverb processing
  Delay Time - Pre-delay before reverb (0-1)
  Size       - Reverb room size (0.5-3.0)
  Damping    - High frequency damping
  Mix        - Dry/wet balance
  Decay      - Reverb decay time
  Diffusion  - Density of reflections
  Mod Depth  - Modulation depth
  Mod Freq   - Modulation frequency

ABOUT:
Wakefield features the Greyhole reverb algorithm, a complex diffused delay
network designed specifically for lush, expansive ambient spaces. The reverb
can create everything from subtle room ambience to massive cathedral-like
spaces with infinitely long decay times.

The Modulation parameters add subtle pitch shifting and movement to the
reverb tail, creating shimmering, ethereal textures that never sound static.
)";
            break;

        case UIPage::FILTER:
            content = R"(
=== FILTER ===

CONTROLS:
  [Same global controls as MAIN page]

PARAMETERS:
  Type    - Lowpass, Highpass, High Shelf, Low Shelf
  Enabled - Bypass filter processing
  Cutoff  - Filter cutoff frequency (20-20000 Hz)
  Gain    - Shelf gain in dB (for shelf filters only)

ABOUT:
The filter section provides tone shaping capabilities. Lowpass and highpass
filters are based on the Topology-Preserving Transform (TPT) design for
smooth, artifact-free cutoff modulation.

The shelf filters allow you to boost or cut high or low frequencies,
useful for brightening dull sounds or removing muddiness. All filter types
support MIDI learn for real-time modulation.
)";
            break;

        case UIPage::LOOPER:
            content = R"(
=== LOOPER ===

CONTROLS:
  Space      - Record / Play / Stop
  O          - Toggle Overdub mode
  S          - Stop loop
  C          - Clear loop
  1-4        - Select loop (4 independent loops)
  [/]        - Adjust overdub mix
  H          - Show this help

PARAMETERS:
  Current Loop - Active loop (1-4)
  Overdub Mix  - Wet amount for overdubbing (0-100%)

ABOUT:
Wakefield includes 4 independent guitar-pedal-style loopers. Each loop can
record, playback, overdub, stop, and clear independently.

WORKFLOW:
1. Press Space to start recording
2. Press Space again to start playback
3. Press O to enter overdub mode (layers on top)
4. Use [/] to control how much new audio mixes with existing loop
5. Switch between loops with 1-4 keys to layer different parts

The overdub mix control is crucial - keeping it around 60% prevents the loop
from getting too loud when layering multiple passes.
)";
            break;

        case UIPage::SEQUENCER:
            content = R"(
=== GENERATIVE SEQUENCER ===

CONTROLS:
  Space      - Play/Stop sequencer
  G          - Generate new pattern
  M          - Mutate pattern (20% variation)
  R          - Reset playback position
  T/Y        - Tempo -/+ 5 BPM
  h/j        - Euclidean hits -/+ 1
  1-4        - Switch track (4 tracks)
  [/]        - Rotate pattern left/right
  H          - Show this help
  Arrow keys - Navigate tracker rows/columns (Left pane) and info entries (Right pane)
  Shift+Up/Down - Fine adjust selected tracker cell or info field while playing
  Enter      - Type an exact value for the selected tracker cell or info field

LAYOUT:
  • Left pane shows the vertical tracker (Idx, Lock, Note, Vel, Gate, Prob).
    The highlighted row is the current step; 'L' in the Lock column means a
    step is locked from regeneration; '·' marks inactive steps. Selected cells
    glow teal; inactive cells render as gray dots until activated.
  • Right pane shows track status, tempo, step counter, scale/root note,
    Euclidean data, subdivision, mute/solo state, and quick statistics for the
    active track. Use Arrow Right from the tracker to focus the info pane.

ABOUT:
The sequencer is a generative MIDI pattern generator that combines three
powerful algorithmic composition techniques:

1. CONSTRAINT-BASED PITCH GENERATION
   Generates notes within musical scales (Phrygian, Locrian, Dorian, etc.)
   ensuring all output is musically coherent. Supports melodic contours like
   "Orbiting" (gravitational pull to center note) and "Random Walk".

2. MARKOV CHAIN MELODY
   Uses probability-weighted transitions to generate evolving melodies.
   Notes don't repeat randomly - they follow musical logic with smooth
   voice leading and tendency tones.

3. EUCLIDEAN RHYTHM GENERATION
   Distributes N hits evenly across M steps using Bjorklund's algorithm.
   Example: 7 hits in 16 steps creates [X · · X · · X · · X · · X · ·]
   This creates mathematically perfect, organic-sounding rhythms.

WORKFLOW:
- Press G to generate a new pattern
- Press Space to hear it
- Press M to mutate slightly (keeps good parts, varies others)
- Adjust Euclidean hits (H/J) to change rhythm density
- Switch tracks (1-4) to layer multiple patterns
- Each track can have different scales, tempos, subdivisions

DEFAULT SETTINGS (optimized for dark ambient):
- Scale: D Phrygian (dark, mysterious)
- Octave Range: C2-C4 (low, droney)
- Contour: Orbiting D3 (hypnotic repetition)
- Euclidean: 7/16 (sparse, interesting)
- Tempo: 90 BPM (slow, meditative)

MULTI-TRACK SYSTEM:
Wakefield supports 4 independent tracks playing simultaneously. Each track
has its own:
- Pattern (16-256 steps)
- Scale and constraints
- Euclidean rhythm settings
- Markov chain transition matrix
- Subdivision (1/16th, 1/8th, 1/4th, etc.)

This allows complex polyrhythmic textures:
- Track 1: Slow drone (whole notes, 1/4 subdivision)
- Track 2: Mid-tempo pad (1/8th subdivision)
- Track 3: Fast arpeggio (1/32nd subdivision, near audio-rate)
- Track 4: Sparse percussion (1/16th, very few hits)
)";
            break;

        case UIPage::CONFIG:
            content = R"(
=== CONFIGURATION ===

CONTROLS:
  Up/Down    - Navigate options
  Enter      - Select audio/MIDI device
  H          - Show this help
  Q          - Quit

ABOUT:
The CONFIG page shows system information and allows selection of audio
and MIDI devices. Device changes require restarting the application to
apply the new settings.

SYSTEM INFO:
- Audio device and sample rate
- MIDI input device
- Buffer size (affects latency)

To change devices, select them from the list and press Enter. The application
will restart with the new configuration.
)";
            break;

        default:
            content = "No help available for this page.";
            break;
    }

    return content;
}

void UI::drawHelpPage() {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    // Draw title bar
    attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(0, 0, " HELP ");
    attroff(COLOR_PAIR(5) | A_BOLD);

    attron(COLOR_PAIR(6));
    for (int i = 6; i < cols; ++i) {
        mvaddch(0, i, ' ');
    }
    attroff(COLOR_PAIR(6));

    // Draw separator
    attron(COLOR_PAIR(1));
    mvhline(1, 0, '-', cols);
    attroff(COLOR_PAIR(1));

    // Get help content for current page
    std::string content = getHelpContent(currentPage);

    // Split content into lines
    std::vector<std::string> lines;
    std::string line;
    for (char c : content) {
        if (c == '\n') {
            lines.push_back(line);
            line.clear();
        } else {
            line += c;
        }
    }
    if (!line.empty()) {
        lines.push_back(line);
    }

    // Draw content with scrolling
    int contentHeight = rows - 4;  // Leave space for header and footer
    int startLine = helpScrollOffset;
    int endLine = std::min(static_cast<int>(lines.size()), startLine + contentHeight);

    for (int i = startLine; i < endLine; ++i) {
        int displayRow = 2 + (i - startLine);
        mvprintw(displayRow, 1, "%s", lines[i].c_str());
    }

    // Draw footer with scroll indicator
    attron(COLOR_PAIR(1));
    mvhline(rows - 2, 0, '-', cols);
    attroff(COLOR_PAIR(1));

    attron(COLOR_PAIR(5) | A_BOLD);
    mvprintw(rows - 1, 1, "Up/Down/PgUp/PgDn: Scroll  |  H/Esc/Q: Close");
    attroff(COLOR_PAIR(5) | A_BOLD);

    // Scroll indicator
    if (lines.size() > contentHeight) {
        int percent = (helpScrollOffset * 100) / std::max(1, static_cast<int>(lines.size()) - contentHeight);
        mvprintw(rows - 1, cols - 15, "[%3d%% of %3d]", percent, static_cast<int>(lines.size()));
    }
}
