# Wakefield UI Color Taxonomy

## Visual Element Categories (CSS-like semantic classes)

### 1. Typography
- `page_title` - Main page title (e.g., "FM MATRIX", "SAMPLER 1")
- `section_header` - Section headings within a page
- `label` - Parameter labels, field names
- `value` - Parameter values, data display
- `hint_text` - Help text, instructions

### 2. Navigation & Selection
- `selection_fg` - Selected item foreground
- `selection_bg` - Selected item background
- `cursor` - Selection cursor (">") character
- `tab_active_fg` - Active tab text
- `tab_active_bg` - Active tab background
- `tab_inactive_fg` - Inactive tab text
- `tab_inactive_bg` - Inactive tab background

### 3. Parameter States
- `param_normal` - Normal parameter display
- `param_locked` - Locked parameter (immune to randomization)
- `param_modulated` - Parameter with active modulation
- `param_midi_mapped` - Parameter mapped to MIDI CC

### 4. Status Indicators
- `status_active` - Active voice, playing note
- `status_inactive` - Inactive voice, no sound
- `status_muted` - Muted channel
- `status_solo` - Solo channel
- `status_error` - Error state
- `status_warning` - Warning state

### 5. UI Chrome
- `border` - Box borders, separators
- `grid_line` - Grid lines in matrices
- `background` - Default background
- `popup_bg` - Popup dialog background
- `popup_border` - Popup dialog border

### 6. Special Elements
- `waveform_active` - Active waveform region (in loop)
- `waveform_inactive` - Inactive waveform region (outside loop)
- `meter_low` - Meter/bar at low levels
- `meter_mid` - Meter/bar at medium levels
- `meter_high` - Meter/bar at high levels
- `midi_learn_popup` - MIDI learn popup styling

### 7. Console
- `console_normal` - Normal console message
- `console_error` - Error message
- `console_warning` - Warning message
- `console_success` - Success message

## Current Mapping Issues

The blind `COLOR_PAIR(1)` → `COLOR_CURSOR` replacement caused these problems:

1. **Page titles** use `COLOR_CURSOR` but should use `page_title`
2. **Headers** use `COLOR_CURSOR` but should use `section_header`
3. **Borders** use `COLOR_CURSOR` but should use `border`
4. **Grid lines** use `COLOR_CURSOR` but should use `grid_line`

## Proposed INI Structure

```ini
[typography]
page_title = cyan
section_header = cyan
label = white
value = white
hint_text = yellow

[navigation]
selection_fg = white
selection_bg = blue
cursor = cyan
tab_active_fg = white
tab_active_bg = blue
tab_inactive_fg = black
tab_inactive_bg = cyan

[parameters]
normal = white
locked = red
modulated = green
midi_mapped = yellow

[status]
active = green
inactive = white
muted = red
solo = yellow
error = red
warning = yellow

[chrome]
border = cyan
grid_line = cyan
background = black
popup_bg = black
popup_border = yellow

[waveform]
active = green
inactive = 238  # Dim gray (256-color)

[console]
normal = white
error = red
warning = yellow
success = green
```

## Migration Strategy

1. Audit every `COLOR_XXX` usage manually
2. Assign proper semantic class based on context
3. Update theme.h enum to match taxonomy
4. Create migration mapping document
5. Replace colors with correct semantic names
6. Test across all pages
