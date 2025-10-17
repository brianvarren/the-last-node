#ifndef UI_UTILS_H
#define UI_UTILS_H

#include <string>
#include <vector>
#include <ncursesw/curses.h>
#include "constraint.h"  // For Subdivision, Scale enums
#include "pattern.h"     // For Subdivision

namespace UIUtils {

// Scale and subdivision ordering for menus
extern const std::vector<Scale> kScaleOrder;
extern const std::vector<Subdivision> kSubdivisionOrder;

// Cell alignment options
enum class CellAlign {
    Left,
    Center,
    Right
};

// UTF-8 / Wide string conversion
int SafeWcWidth(wchar_t ch);
int DisplayWidth(const std::wstring& text);
std::wstring Utf8ToWide(const std::string& text);

// Cell formatting
std::wstring FitCell(const std::wstring& input, int width, CellAlign align);

// Column layout computation
int ComputeTotalWidth(const std::vector<int>& widths);
std::vector<int> ComputeColumnStarts(const std::vector<int>& widths);
std::vector<int> ComputeColumnBoundaries(const std::vector<int>& widths);

// Grid drawing helpers
void DrawHorizontalLine(WINDOW* win, int y, const std::vector<int>& widths,
                        chtype leftChar, chtype midChar, chtype rightChar, chtype junctionChar);
void DrawSequencerGrid(WINDOW* win, int rows, const std::vector<int>& widths,
                       const std::vector<int>& boundaries);
void RenderSequencerCell(WINDOW* win,
                         int rowIndex,
                         int columnIndex,
                         const std::wstring& text,
                         const std::vector<int>& widths,
                         const std::vector<int>& starts,
                         CellAlign align,
                         bool highlight,
                         bool dim,
                         int dimColorPair);

// Sequencer grid constants
extern const std::vector<int> kSequencerColumnWidths;
extern const std::vector<int> kSequencerColumnStarts;
extern const std::vector<int> kSequencerColumnBoundaries;
extern const int kSequencerGridWidth;

// Note/music conversion helpers
std::string midiNoteToString(int midiNote);
std::string rootNoteToString(int root);
int noteNameToSemitone(const std::string& name);
bool parseNoteText(const std::string& text, int& midiOut);
bool parseRootText(const std::string& text, int& rootOut);

// Subdivision helpers
std::string subdivisionToString(Subdivision subdiv);
bool parseSubdivisionText(const std::string& text, Subdivision& outSubdiv);

// Scale helpers
std::string scaleKey(Scale scale);
const char* scaleDisplayName(Scale scale);
bool parseScaleText(const std::string& text, Scale& outScale);

} // namespace UIUtils

#endif // UI_UTILS_H
