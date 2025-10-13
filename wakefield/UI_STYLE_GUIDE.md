# Wakefield UI/UX Style Guide

**Version 1.0** | Terminal-based Synthesizer Interface

---

## Core Design Philosophy

Wakefield's interface follows **guitar pedal/hardware synth** interaction patterns adapted for terminal use. The UI prioritizes:

1. **Immediate visual feedback** - All parameter changes reflect instantly
2. **Muscle memory** - Consistent key mappings across all pages
3. **Non-modal editing** - Direct parameter manipulation without popup dialogs
4. **Performance-ready** - Controls designed for live use (MIDI learn, quick navigation)

---

## Color Theme

### Standard Color Palette

**Background:**
- Terminal black (default)

**Text Colors:**
```
COLOR_PAIR(1) = Default (white/gray)    - Normal text, borders
COLOR_PAIR(2) = Green                   - Active/enabled states, success
COLOR_PAIR(3) = Yellow                  - Warning, learning mode
COLOR_PAIR(4) = Red                     - Error, recording state
COLOR_PAIR(5) = Teal/Cyan (bright)      - Selected/highlighted items
COLOR_PAIR(6) = Gray (dim)              - Inactive tabs, disabled items
```

### Color Usage Guidelines

**When to use each color:**

- **White/Gray (COLOR_PAIR 1)**: Default text, labels, parameter names, borders, separators
- **Green (COLOR_PAIR 2)**: 
  - "ON" status indicators
  - Playing state
  - Successfully mapped MIDI CC
  - Positive confirmation
- **Yellow (COLOR_PAIR 3)**:
  - Recording state (looper)
  - "Learning mode active" banners
  - Attention-needed states
- **Red (COLOR_PAIR 4)**:
  - Error messages
  - Overdubbing state (looper)
  - Critical warnings
- **Teal/Cyan (COLOR_PAIR 5)**:
  - Currently selected tab
  - Currently selected row/parameter
  - Active focus indicators
  - Current loop selection
- **Gray/Dim (COLOR_PAIR 6)**:
  - Inactive tabs
  - Disabled parameters
  - "OFF" status indicators

**Attribute Modifiers:**
- Use `A_BOLD` with colors for emphasis (selected items, section headers)
- Avoid `A_REVERSE` except for extreme emphasis
- No `A_UNDERLINE` (poor terminal support)

---

## Navigation System

### Arrow Key Navigation

**Global Navigation (no popup active):**

```
← / → (Left/Right)   - Navigate between tabs/pages
↑ / ↓ (Up/Down)      - Navigate between rows/parameters
Tab                  - Cycle forward through pages
Shift+Tab            - Cycle backward through pages
```

**In-Page Navigation:**
- Arrow keys ALWAYS navigate to selectable parameters
- Selected row/item highlighted with `COLOR_PAIR(5) | A_BOLD` and `>` prefix
- Navigation wraps (top ↑ goes to bottom, bottom ↓ goes to top)

### Edit Mode Pattern

**Standard parameter editing flow:**

1. **Navigate** to parameter using ↑/↓ arrows
2. **Press Enter** to select parameter for editing (visual indicator changes)
3. **Use ← / → arrows** to adjust value in real-time
   - Updates apply immediately (no confirmation needed)
   - Visual feedback shows current value
4. **Press Enter or Esc** to exit edit mode
   - Returns to navigation mode
   - Parameter retains edited value

**Alternative: Direct keyboard shortcuts** (current implementation)
- Letter keys (A/a, D/d, etc.) directly modify parameters
- No explicit edit mode required
- Works when page is active

**Future Implementation Note:**
The Enter-based edit mode should be added alongside existing shortcuts for better discoverability and consistency.

---

## Page Layout Structure

### Standard Page Template

```
┌─────────────────────────────────────────────────────┐
│ MAIN  REVERB  FILTER  LOOPER  CONFIG  TEST         │ ← Tabs (row 0)
├─────────────────────────────────────────────────────┤
│                                                     │ ← Separator (row 1)
│  [Page Content Starts - Row 3]                     │
│                                                     │
│  SECTION HEADER (bold)                             │
│                                                     │
│  > Parameter 1: [value]      (←/→)                 │ ← Selected
│    Parameter 2: [value]      (←/→)                 │
│    Parameter 3: [value]      (←/→)                 │
│                                                     │
│  ANOTHER SECTION (bold)                            │
│                                                     │
│  Parameter 4: [value]                              │
│                                                     │
│  ...                                               │
│                                                     │
├─────────────────────────────────────────────────────┤
│ [Console messages (MAIN page only)]                │
│                                                     │
├─────────────────────────────────────────────────────┤
│ q:Quit  s:Save  l:Load  [page-specific]           │ ← Hotkeys (last row)
└─────────────────────────────────────────────────────┘
```

### Layout Rules

1. **Row 0**: Tabs - always visible
2. **Row 1**: Separator line (`-` characters)
3. **Row 2**: Empty (padding)
4. **Row 3+**: Page content begins
5. **Bottom -2**: Console area (MAIN page only, 5 lines max)
6. **Bottom -1**: Separator
7. **Bottom 0**: Hotkey reference line

**Section Headers:**
- Use `A_BOLD` attribute
- ALL CAPS for major sections
- Empty line before/after for spacing

**Parameter Rows:**
- Indent by 2 spaces from left margin
- Format: `  Label (keys): value  (unit if applicable)`
- Add `>` prefix for selected row (teal/bold)

---

## Parameter Display Patterns

### Slider Bars (drawBar function)

```cpp
void drawBar(int y, int x, const char* label, float value, 
             float min, float max, int width);
```

**Visual format:**
```
  Label (Keys): [====      ] 50.0% 
                ^          ^
                filled     empty
```

- Use `█` or `=` for filled portion
- Use ` ` (space) for empty portion  
- Show numeric value after bar
- Include key hints in label: `"Attack (A/a)"`

**Bar width guidelines:**
- Standard parameter: 20 characters
- Long parameter: 30 characters
- Mini parameter: 10 characters

### Numeric Values

**Format based on range:**
- **Percentages**: `50.0%` (1 decimal)
- **Time (seconds)**: `0.25s` or `250ms`
- **Frequency**: `1000 Hz` or `1.0 kHz`
- **Gain (dB)**: `+6.0 dB` or `-12.0 dB`
- **Generic (0-1)**: `0.75` (2 decimals)

### State Indicators

**Boolean states:**
```
Enabled: ON   (green)
Enabled: OFF  (gray)
```

**Multi-state (use color coding):**
```
State: Empty        (gray)
State: Recording    (yellow) 
State: Playing      (green)
State: Overdubbing  (red)
State: Stopped      (cyan)
```

---

## Interactive Element Patterns

### Selectable Parameters

**Row selection indicator:**
```
> Parameter: value    ← Selected (teal, bold, with >)
  Parameter: value    ← Not selected
```

**Currently editing (future):**
```
> Parameter: [value]  ← In edit mode (brackets around value)
```

### Menu Popups

**When to use:**
- Discrete choices (waveform, filter type)
- Preset selection
- Device selection

**Format:**
```
┌─── Select Waveform ───┐
│                        │
│  > Sine                │  ← Selected (teal, bold)
│    Square              │
│    Sawtooth            │
│    Triangle            │
│                        │
└────────────────────────┘
```

**Popup behavior:**
- Center on screen
- Arrow keys navigate
- Enter selects
- Esc cancels
- Close popup on selection

### MIDI Learn Mode

**Visual pattern:**
```
MIDI CC LEARN (section header)

>>> LEARNING MODE ACTIVE <<<  (yellow, bold, blinking if possible)
Move a MIDI controller to assign it to: [Parameter Name]

Assigned CC: CC#42  (green, bold)
```

**States:**
- Inactive: Show "Press M to learn" prompt
- Active: Show "LEARNING MODE ACTIVE" banner in yellow
- Assigned: Show CC number in green

---

## Keyboard Shortcut Conventions

### Reserved Global Keys

**Navigation:**
- `←/→` or `Tab/Shift+Tab`: Page navigation
- `↑/↓`: Row/parameter navigation  
- `Enter`: Select/confirm (edit mode entry, menu selection)
- `Esc`: Cancel/exit (edit mode exit, close popup)

**Global Actions:**
- `q`: Quit application
- `s`: Save preset (opens text input or menu)
- `l`: Load preset (opens menu)

### Page-Specific Keys

**Letter keys for direct parameter control:**
- Use **uppercase for increase**, **lowercase for decrease**
- Example: `A/a` for Attack, `D/d` for Decay
- Document in hotkey line at bottom

**Number keys for selection:**
- `1-4`: Select items (loops, banks, etc.)
- `0-9`: Numeric input when in text entry mode

**Special characters:**
- `[/]`: Decrease/increase (generic)
- `+/-`: Volume/level controls
- `Space`: Toggle ON/OFF or Rec/Play

### Hotkey Line Format

```
q:Quit  s:Save  l:Load  A/a:Attack  D/d:Decay  Space:Toggle
```

- Separate with double space
- Format: `key:Action`
- Most important actions first
- Limit to ~10 items (fit in 80 cols)

---

## Text Input Pattern

**For preset names, search, etc.:**

1. Show input prompt: `Enter preset name: _` (cursor)
2. Echo typed characters in real-time
3. Support basic editing:
   - `Backspace`: Delete last character
   - `Enter`: Confirm input
   - `Esc`: Cancel input
4. Max length indicator: `[15/32]` (current/max)

---

## Console Message System

**Location:** Bottom of MAIN page (5 lines max)

**Message format:**
```
[12:34:56] Message text here
[12:34:57] Another message
```

**Message types:**
- **Info** (white): General notifications
- **Success** (green): "Loaded preset X", "MIDI CC learned"
- **Warning** (yellow): "Audio buffer underrun"  
- **Error** (red): "Failed to load file"

**Behavior:**
- FIFO queue (oldest message drops off top)
- Timestamp optional (useful for debugging)
- Auto-clear after time limit (optional)

---

## Visual Feedback Timing

**Immediate (<16ms):**
- Parameter value changes
- Row selection changes
- Key press acknowledgment

**Quick (50-100ms):**
- Page transitions
- Popup open/close

**Noticeable (200-500ms):**
- Console message appearance
- State change animations (if added)

**Never block UI:**
- File I/O operations
- Preset loading
- Device changes (handle async, show loading indicator)

---

## Accessibility Considerations

**High contrast:**
- Maintain 4.5:1 contrast ratio minimum
- Test on multiple terminal color schemes
- Provide monochrome fallback option (future)

**No color-only information:**
- Always pair color with text or symbol
- Example: "ON (green)" not just color green
- State names always visible

**Keyboard-only:**
- Every function accessible via keyboard
- No mouse required
- Document all shortcuts

---

## Grid and Spacing

**Horizontal spacing:**
- Left margin: 2 spaces from edge
- Between columns: 4 spaces minimum
- Parameter label to value: 2 spaces

**Vertical spacing:**
- Between sections: 2 blank lines
- Between parameters: 1 line (consecutive rows)
- After section header: 1 blank line

**Alignment:**
- Left-align labels
- Right-align numeric values (in columns)
- Center popup menus

---

## Multi-Item Display Patterns

### Loop/Voice/Channel Lists

**Format for multiple items:**
```
LOOP STATUS

> Loop 1: Playing      00:05.32 / 00:10.00  ← Selected (teal)
  Loop 2: Overdubbing  00:02.15 / 00:08.50
  Loop 3: Stopped      00:00.00 / 00:12.00
  Loop 4: Empty        --:--:-- / --:--:--
```

**Guidelines:**
- Show index/number clearly
- State name (color-coded)
- Relevant metrics (time, level, etc.)
- Current selection indicator (`>`)

---

## Error Handling & User Feedback

### Error Messages

**Format:**
```
ERROR: [Brief description]
[Optional: What to do next]
```

**Display:**
- Show in console (if available)
- Show on current page as temporary overlay (3-5 sec)
- Use red color

**Example:**
```
ERROR: Failed to load preset "mypreset"
File not found in ~/.config/wakefield/presets/
```

### Loading States

**While loading:**
```
Loading preset "mypreset"...
```

**After completion:**
```
✓ Loaded preset "mypreset"  (green)
✗ Failed to load            (red)
```

---

## Implementation Checklist

**For each new UI element:**

- [ ] Follows color theme (teal selection, green=on, red=error, yellow=warn)
- [ ] Navigable with arrow keys
- [ ] Visual feedback for selection (`>` prefix, COLOR_PAIR(5))
- [ ] Hotkey documented in bottom bar
- [ ] Parameter changes apply immediately
- [ ] Includes edit mode (Enter to edit, Esc to exit) - *future*
- [ ] Accessible without mouse
- [ ] Error states handled gracefully
- [ ] Console message on state change (if appropriate)

---

## Code Style for UI

### Parameter Drawing Template

```cpp
void UI::drawParameterRow(int row, int paramIndex, const char* label, 
                          float value, const char* unit) {
    // Show selection indicator
    if (selectedRow == paramIndex) {
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(row, 2, ">");
        attroff(COLOR_PAIR(5) | A_BOLD);
    } else {
        mvprintw(row, 2, " ");
    }
    
    // Draw label and value
    mvprintw(row, 3, "%s: %.2f%s", label, value, unit);
}
```

### Section Header Template

```cpp
void UI::drawSection(int& row, const char* title) {
    attron(A_BOLD);
    mvprintw(row++, 1, "%s", title);
    attroff(A_BOLD);
    row++;  // blank line after header
}
```

### Color-Coded State Template

```cpp
void UI::drawState(int row, int col, const char* stateName, int stateValue) {
    const char* stateText[] = {"Empty", "Recording", "Playing"};
    int colors[] = {6, 3, 2};  // gray, yellow, green
    
    mvprintw(row, col, "State: ");
    attron(COLOR_PAIR(colors[stateValue]) | A_BOLD);
    addstr(stateText[stateValue]);
    attroff(COLOR_PAIR(colors[stateValue]) | A_BOLD);
}
```

---

## Future Enhancements

**To be implemented:**

1. **Edit Mode System**
   - Enter key activates edit mode for selected parameter
   - Visual distinction (brackets, different color)
   - Arrow keys adjust in edit mode
   - Enter/Esc exits edit mode

2. **Visual Animations**
   - Fade-in for messages
   - Pulse effect for learning mode
   - Smooth value transitions

3. **Themes**
   - Configurable color schemes
   - High-contrast mode
   - Monochrome mode

4. **Advanced Navigation**
   - Page history (back button)
   - Jump to page by number
   - Search/filter parameters

5. **Help System**
   - Context-sensitive help (F1 key)
   - Overlay help page
   - Parameter descriptions

---

## Quick Reference

**Color Pairs:**
- 1: Default (white/gray)
- 2: Green (on/success)
- 3: Yellow (warn/record)
- 4: Red (error/overdub)
- 5: Teal (selected)
- 6: Gray (off/inactive)

**Key Bindings:**
- `←/→` or `Tab`: Navigate pages
- `↑/↓`: Navigate rows
- `Enter`: Select/confirm
- `Esc`: Cancel/exit
- `q`: Quit

**Layout:**
- Rows 0-1: Tabs & separator
- Row 3+: Content
- Bottom -2: Console (MAIN only)
- Bottom 0: Hotkeys

---

*This guide is a living document. Update as patterns evolve.*

