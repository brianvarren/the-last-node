#ifndef UI_UTILS_H
#define UI_UTILS_H

#include <string>
#include <vector>
#include <ncursesw/curses.h>
#include "constraint.h"  // For Subdivision, Scale enums
#include "pattern.h"     // For Subdivision
#include "theme.h"       // For theme system

// Theme color helper macros (for easier migration)
#ifndef COLOR_SELECTION
// Typography
#define COLOR_PAGE_TITLE globalTheme->getColorPair(ThemeColor::PageTitle)
#define COLOR_SECTION_HEADER globalTheme->getColorPair(ThemeColor::SectionHeader)
#define COLOR_LABEL globalTheme->getColorPair(ThemeColor::Label)
#define COLOR_VALUE globalTheme->getColorPair(ThemeColor::Value)
#define COLOR_HINT globalTheme->getColorPair(ThemeColor::HintText)

// Selection & Navigation
#define COLOR_SELECTION globalTheme->getColorPair(ThemeColor::SelectionFg)
#define COLOR_CURSOR globalTheme->getColorPair(ThemeColor::Cursor)
#define COLOR_TAB_ACTIVE globalTheme->getColorPair(ThemeColor::TabActiveFg)
#define COLOR_TAB_INACTIVE globalTheme->getColorPair(ThemeColor::TabInactiveFg)

// Parameters
#define COLOR_PARAM_NORMAL globalTheme->getColorPair(ThemeColor::ParamNormal)
#define COLOR_LOCKED globalTheme->getColorPair(ThemeColor::ParamLocked)
#define COLOR_MODULATED globalTheme->getColorPair(ThemeColor::ParamModulated)
#define COLOR_MIDI_MAPPED globalTheme->getColorPair(ThemeColor::ParamMidiMapped)

// Status
#define COLOR_STATUS_ACTIVE globalTheme->getColorPair(ThemeColor::StatusActive)
#define COLOR_STATUS_INACTIVE globalTheme->getColorPair(ThemeColor::StatusInactive)
#define COLOR_STATUS_MUTED globalTheme->getColorPair(ThemeColor::StatusMuted)
#define COLOR_STATUS_SOLO globalTheme->getColorPair(ThemeColor::StatusSolo)

// UI Chrome
#define COLOR_BORDER globalTheme->getColorPair(ThemeColor::Border)
#define COLOR_GRID_LINE globalTheme->getColorPair(ThemeColor::GridLine)
#define COLOR_POPUP_BORDER globalTheme->getColorPair(ThemeColor::PopupBorder)

// Waveform
#define COLOR_WAVEFORM_ACTIVE globalTheme->getColorPair(ThemeColor::WaveformActive)
#define COLOR_WAVEFORM_INACTIVE globalTheme->getColorPair(ThemeColor::WaveformInactive)

// Legacy aliases
#define COLOR_HEADER COLOR_SECTION_HEADER
#endif

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
