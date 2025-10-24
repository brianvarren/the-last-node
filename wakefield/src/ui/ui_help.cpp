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
        case UIPage::OSCILLATOR:
            content = R"(
=== OSCILLATOR ===

CONTROLS:
  Tab/Shift+Tab  - Navigate between pages
  Up/Down        - Select parameter
  Left/Right     - Adjust parameter value
  Enter          - Type exact value
  L              - MIDI Learn (assign MIDI CC)

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
  Tab/Shift+Tab  - Navigate between pages
  Up/Down        - Select parameter
  Left/Right     - Adjust parameter value
  Enter          - Type exact value
  L              - MIDI Learn (assign MIDI CC)

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

        case UIPage::ENV:
            content = R"(
=== ENVELOPE ===

CONTROLS:
  Tab/Shift+Tab  - Navigate between pages
  Up/Down        - Select parameter
  Left/Right     - Adjust parameter value
  Enter          - Type exact value
  L              - MIDI Learn (assign MIDI CC)
  1-4        - Select envelope (4 independent envelopes)

PARAMETERS:
  Attack     - Attack time (0.001-30s) with exponential scaling
  Decay      - Decay time (0.001-30s) with exponential scaling
  Sustain    - Sustain level (0-1, linear)
  Release    - Release time (0.001-30s) with exponential scaling
  Atk Bend   - Attack curve shape (0-1, 0.5=linear, <0.5=concave, >0.5=convex)
  Rel Bend   - Release curve shape (0-1, affects both decay and release)

ABOUT:
The ENV page provides 4 independent ADSR envelopes with adjustable bend curves
for precise control over amplitude, filter, or modulation shaping.

CURVE BENDING:
  - Attack Bend controls the curvature during the attack phase
  - Release Bend affects both decay and release phases
  - Values < 0.5 create concave (logarithmic) curves - slow start, fast finish
  - Values > 0.5 create convex (exponential) curves - fast start, slow finish
  - Value = 0.5 creates linear curves (default)

ENVELOPE PREVIEW:
The bottom preview shows the complete ADSR envelope with the current bend
settings applied. Labels indicate Attack (A), Decay (D), Sustain (S), and
Release (R) sections.

FUTURE:
These envelopes will be available as modulation sources in the upcoming
modulation matrix, allowing any envelope to modulate any parameter.
Currently, Envelope 1 drives the primary amplitude envelope for each voice.
)";
            break;

        case UIPage::MOD:
            content = R"(
=== MODULATION MATRIX ===

CONTROLS:
  Tab/Shift+Tab  - Navigate between pages

ABOUT:
The modulation matrix will route envelopes, LFOs, and other control-rate
sources to any destination. The UI scaffolding is in place and routing
controls will arrive in an upcoming update.
)";
            break;

        case UIPage::REVERB:
            content = R"(
=== REVERB ===

CONTROLS:
  Tab/Shift+Tab  - Navigate between pages
  Up/Down        - Select parameter
  Left/Right     - Adjust parameter value
  Enter          - Type exact value
  L              - MIDI Learn (assign MIDI CC)

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
  Tab/Shift+Tab  - Navigate between pages
  Up/Down        - Select parameter
  Left/Right     - Adjust parameter value
  Enter          - Type exact value
  L              - MIDI Learn (assign MIDI CC)

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

        case UIPage::MIXER:
            content = R"(
=== MIXER ===

CONTROLS:
  Tab/Shift+Tab  - Navigate between pages
  Up/Down        - Select mixer channel
  Left/Right     - Adjust channel level
  Enter          - Type exact value
  M              - Toggle mute for selected channel
  S              - Toggle solo for selected channel
  L              - MIDI Learn (assign MIDI CC)

CHANNELS:
  OSC 1-4    - Oscillator mix levels (0.0-1.0)
  SAMP 1-4   - Sampler mix levels (0.0-1.0)

ABOUT:
The Mixer page provides level control and mute/solo functionality for all
8 audio sources (4 oscillators + 4 samplers). Each channel has:

LEVEL CONTROL:
  - Mix level sets the static output volume for each source
  - Values range from 0.0 (silent) to 1.0 (full volume)
  - Levels are applied before the master output

MUTE/SOLO:
  - MUTE: Silence a channel without changing its level setting
  - SOLO: Solo one or more channels (all non-soloed channels are muted)
  - Multiple channels can be soloed simultaneously
  - Visual indicators: M (muted in red), S (solo in green)

WORKFLOW:
1. Use Up/Down to select a channel
2. Adjust level with Left/Right (coarse) or Shift+Left/Right (fine)
3. Press M to mute/unmute or S to solo/unsolo
4. Press L to assign MIDI CC control for real-time mixing
)";
            break;

        case UIPage::SAMPLER:
            content = R"(
=== SAMPLER ===

CONTROLS:
  Tab/Shift+Tab  - Navigate between pages
  Up/Down        - Select parameter
  Left/Right     - Adjust parameter value
  Enter          - Type exact value or load sample
  L              - MIDI Learn (assign MIDI CC)
  1-4            - Select sampler (4 independent samplers)

PARAMETERS:
  Key Mode   - FREE (manual speed) or KEY (MIDI tracking)
  Direction  - Forward, Reverse, or Ping-Pong playback
  Loop Start - Start position of loop region (0-100%)
  Loop Length- Length of loop region (0-100%)
  Xfade      - Crossfade length at loop boundaries (0-100%)
  Ratio      - Playback speed multiplier (0.125-16.0)
  Offset     - Frequency offset in Hz (-1000-1000)
  Sync       - Tempo sync mode (off/on/trip/dot)
  Note Reset - Restart playback on note-on

ABOUT:
Wakefield features 4 independent samplers with advanced loop control.
Each sampler can load 16-bit WAV files and play them back with:

PLAYBACK MODES:
  - Forward: Normal playback direction
  - Reverse: Plays sample backwards
  - Ping-Pong: Alternates between forward and reverse at loop boundaries

KEY TRACKING:
  - FREE mode: Manual speed control via Ratio parameter
  - KEY mode: Sample pitch tracks MIDI notes (like a keyboard sampler)
  - Offset adds/subtracts frequency for detuning effects

LOOP CONTROL:
  - Loop Start/Length define a region within the sample
  - Crossfade smoothly blends loop boundaries to prevent clicks
  - Visual loop indicator (green bar) shows active loop region
  - Useful for creating sustained textures from one-shot samples

WAVEFORM DISPLAY:
The waveform preview shows the loaded sample's amplitude envelope.
The green bar at the bottom indicates the active loop region.
)";
            break;

        case UIPage::FM:
            content = R"(
=== FM MATRIX ===

CONTROLS:
  Arrow Keys  - Navigate FM matrix cells (source/target pairs)
  Left/Right  - Adjust FM depth (-99% to +99%)
  Enter       - Type exact value
  Shift+Arrow - Jump by 4 cells
  Tab         - Navigate between pages

MATRIX LAYOUT:
  ROWS (Sources):    OSC1, OSC2, OSC3, OSC4, SAMP1, SAMP2, SAMP3, SAMP4
  COLUMNS (Targets): OSC1, OSC2, OSC3, OSC4, SAMP1, SAMP2, SAMP3, SAMP4

ABOUT:
The FM Matrix provides audio-rate frequency modulation routing between
all 8 audio sources (4 oscillators + 4 samplers). This creates complex,
harmonically rich timbres through phase modulation synthesis.

HOW FM WORKS:
  - Source oscillators/samplers modulate the frequency of target oscillators
  - Each cell shows FM depth as percentage (-99% to +99%)
  - Positive values: Normal FM (adds harmonics)
  - Negative values: Inverted FM (different harmonic content)
  - Zero depth: No modulation (default)

CREATING FM PATCHES:
1. Navigate to a cell where row is MODULATOR and column is CARRIER
   Example: Row=OSC1, Col=OSC2 means OSC1 modulates OSC2
2. Increase depth to hear frequency modulation effects
3. Try multiple modulators on one carrier for complex timbres
4. Experiment with feedback by routing an oscillator to itself
   Example: Row=OSC1, Col=OSC1 creates self-modulation

TIPS:
  - Start with 10-30% depth and adjust to taste
  - Use samplers as modulators for unique textures
  - Combine FM with the morph parameter for evolving sounds
  - Higher depth values create more aggressive, harmonically dense sounds
  - Self-modulation (diagonal cells) creates feedback FM

CLASSIC FM ALGORITHMS:
  - 2-Operator: OSC1→OSC2 (simple, bell-like tones)
  - 3-Operator Stack: OSC1→OSC2→OSC3 (complex harmonics)
  - Parallel: OSC1→OSC3, OSC2→OSC3 (dual modulators)
)";
            break;

        case UIPage::CHAOS:
            content = R"(
=== CHAOS GENERATORS (Ikeda Map) ===

CONTROLS:
  Tab/Shift+Tab  - Navigate between pages
  Up/Down        - Select parameter
  Left/Right     - Adjust parameter value
  Enter          - Type exact value
  1-4            - Select chaos generator (4 independent generators)

PARAMETERS:
  Chaos      - Chaos amount (0.0-2.0, ~0.6 is chaotic)
  Clock      - Update rate in Hz (0.01-1000)
  Mode       - FAST (audio-rate) or CLOCKED (low-rate)
  Interp     - Interpolation: CUBIC or LINEAR

ABOUT:
Chaos generators produce complex, non-repeating modulation signals using
the Ikeda map algorithm. Unlike LFOs which repeat in cycles, chaos
generators create evolving, organic modulation that never exactly repeats.

THE IKEDA MAP:
The Ikeda map is a 2D chaotic attractor originally used to model laser
behavior. It produces smooth, complex trajectories in phase space that
can be used as modulation sources for synthesis parameters.

CHAOS PARAMETER:
  - 0.0-0.3:   Periodic (repeating patterns)
  - 0.4-0.7:   Chaotic (sweet spot, complex but controlled)
  - 0.8-2.0:   Very chaotic (wild, unpredictable)
  - ~0.6:      Ideal for musical applications

OPERATING MODES:
  FAST Mode:   Updates every audio sample (48kHz rate)
               - More CPU intensive
               - Richer, more complex output
               - Best for audio-rate modulation

  CLOCKED Mode: Updates at clock frequency (0.01-1000 Hz)
                - Less CPU intensive
                - Interpolates between iterations
                - Best for slow, evolving modulation

INTERPOLATION:
  LINEAR: Straight lines between chaos iterations (faster)
  CUBIC:  Smooth curves between iterations (more organic)

MUSICAL APPLICATIONS:
  - Evolving filter sweeps (modulate filter cutoff)
  - Non-repeating LFO-like modulation
  - Generative pitch variations
  - Organic amplitude modulation
  - Slow-moving timbral evolution (low clock rate)
  - Aggressive modulation effects (high clock rate)

WORKFLOW:
1. Start with Chaos=0.6, Clock=1Hz, CLOCKED mode, LINEAR interp
2. Route chaos output to a parameter via the MOD matrix
3. Adjust Chaos parameter to find interesting behavior
4. Increase Clock frequency for faster modulation
5. Switch to FAST mode for audio-rate effects
6. Try CUBIC interpolation for smoother movement

TIPS:
  - Lower clock rates (0.1-5 Hz) create slow, ambient evolution
  - Higher clock rates (10-100 Hz) create tremolo/vibrato effects
  - FAST mode can create audio-rate distortion and noise textures
  - Chaos generators never repeat, making them ideal for generative music
  - Use multiple chaos generators with different parameters for layered complexity
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
  Ctrl+K     - Toggle MIDI Keyboard Mode (global)

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

MIDI KEYBOARD MODE:
Press Ctrl+K from any page to toggle MIDI Keyboard Mode. When active, your
typing keyboard becomes a musical keyboard that triggers MIDI notes:

KEYBOARD LAYOUT:
  Lower Row (white keys):  Z X C V B N M , (C D E F G A B C)
                           S D   G H J   (C# D# F# G# A#)

  Upper Row (second octave): Q W E R T Y U I O P (C through E, +12 semitones)
                             2 3   5 6 7   9 0   (sharps/flats)

OCTAVE CONTROLS:
  A        - Lower octave (0-10)
  ' (apos) - Raise octave (0-10)
  ESC      - Exit MIDI Keyboard Mode

When active, a highlighted "[MIDI KB: OCTX]" indicator appears in the top bar
showing the current octave. Default octave is 4 (middle C = C4 = MIDI note 60).

Notes:
- Fixed velocity of 100 for all notes
- Notes sustain until you release the mode (no key-release detection)
- All normal UI navigation is disabled while in MIDI keyboard mode
- Use ESC or Ctrl+K to exit the mode and return to normal operation
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
