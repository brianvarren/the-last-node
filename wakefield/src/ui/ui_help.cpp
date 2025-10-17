#include "../ui.h"
#include <algorithm>
#include <string>
#include <vector>

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

        case UIPage::OSCILLATOR:
            content = R"(
=== OSCILLATOR ===

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
The Oscillator page controls audio-rate oscillators that generate sound.
Each voice has 4 oscillators that can be mixed together. In FREE mode,
you control the frequency directly. In KEY mode, it tracks MIDI notes.

The Morph parameter controls waveform shape using phase distortion and
tanh-shaped synthesis. Duty controls pulse width for PWM-like effects.
)";
            break;

        case UIPage::LFO:
            content = R"(
=== LFO (Low Frequency Oscillator) ===

CONTROLS:
  [Same global controls as MAIN page]

PARAMETERS:
  Period     - LFO cycle time (0.1s - 30min) with semantic input
  Sync       - Tempo sync mode (off/on/trip/dot)
  Morph      - Waveform shape (same as oscillator)
  Duty       - Pulse width (same as oscillator)
  Flip       - Polarity inversion
  Note Reset - Restart phase on note-on

ABOUT:
LFOs are control-rate modulation sources optimized for performance.
Unlike audio oscillators, LFOs run slower and can be tempo-synced.
Use semantic time input like "3m39s" for precise period control.
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
