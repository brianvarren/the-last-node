# Color Taxonomy Audit - Work in Progress

## Status
The theme system infrastructure is complete with proper semantic color enums, but the actual color assignments in the UI code need manual review and correction.

## Known Issues with Current Assignments

### `COLOR_CURSOR` is Overloaded
Currently used for multiple semantic purposes:
- Page titles (should be `COLOR_PAGE_TITLE`)
- Section headers (should be `COLOR_SECTION_HEADER`)
- Borders and separators (should be `COLOR_BORDER`)
- Grid lines (should be `COLOR_GRID_LINE`)
- Labels (should be `COLOR_LABEL`)

### Manual Audit Required For Each File:

#### ui_drawing.cpp
- Line 59: Separator line → `COLOR_BORDER`
- Line 106, 130: CPU/Hotkey labels → `COLOR_LABEL`
- Line 566: "MIXER" title → `COLOR_PAGE_TITLE`
- Line 572: Column headers → `COLOR_SECTION_HEADER`

#### ui_page_fm.cpp
- Page title "FM MATRIX" → `COLOR_PAGE_TITLE`
- Column labels "TARGETS:" → `COLOR_SECTION_HEADER`
- Grid separators → `COLOR_GRID_LINE`

#### ui_page_envelope.cpp
- "Select Envelope" → `COLOR_SECTION_HEADER`
- "Envelope X Parameters:" → `COLOR_SECTION_HEADER`
- "VOICE ENVELOPES" → `COLOR_SECTION_HEADER`

#### ui_page_sampler.cpp
- "SAMPLER X" title → `COLOR_PAGE_TITLE`
- "PARAMETERS" header → `COLOR_SECTION_HEADER`
- Waveform active region → `COLOR_WAVEFORM_ACTIVE` (with A_BOLD)
- Waveform inactive → `COLOR_WAVEFORM_INACTIVE` (with A_DIM)

#### ui_page_mod.cpp
- "MODULATION MATRIX" → `COLOR_PAGE_TITLE`
- "Select Source/Destination" → `COLOR_SECTION_HEADER`
- Column headers → `COLOR_SECTION_HEADER`

#### ui_drawing.cpp (Main Page)
- Mute indicators → `COLOR_STATUS_MUTED`
- Solo indicators → `COLOR_STATUS_SOLO`

## Correct Approach

For each `attron(COLOR_XXX)` call:
1. Look at what visual element it's styling
2. Determine semantic purpose from context
3. Choose appropriate ThemeColor enum value
4. Replace macro with correct semantic name

## Example Corrections

### Before (Incorrect):
```cpp
attron(COLOR_CURSOR | A_BOLD);
mvprintw(row, 2, "FM MATRIX");
attroff(COLOR_CURSOR | A_BOLD);
```

### After (Correct):
```cpp
attron(COLOR_PAGE_TITLE | A_BOLD);
mvprintw(row, 2, "FM MATRIX");
attroff(COLOR_PAGE_TITLE | A_BOLD);
```

## Priority Order

1. **High Priority** - Page titles and major headers (most visible)
2. **Medium Priority** - Status indicators (mute/solo/active)
3. **Low Priority** - Borders and grid lines (less critical for functionality)

## Testing Strategy

After each file is corrected:
1. Build and run synth
2. Navigate to affected page
3. Verify colors match intended semantic meaning
4. Check that theme changes work correctly
