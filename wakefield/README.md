# Wakefield Synthesizer

**Wakefield** is a real-time polyphonic synthesizer built in C++ with a terminal-based UI (ncurses). It features MIDI input, professional-grade reverb, flexible filtering, and a preset management system.

## Overview

Wakefield is a fully-functional software synthesizer designed for live performance and sound design. It combines classic synthesis techniques with modern DSP algorithms, all controlled through an efficient terminal interface. The project demonstrates advanced audio programming concepts including polyphonic voice management, real-time DSP processing, and low-latency audio/MIDI handling.

## Core Features

### 🎹 Polyphonic Synthesis Engine
- **8-voice polyphony** with automatic voice allocation
- **Multiple waveforms**: Sine, Square, Sawtooth, Triangle
- **ADSR envelope** per voice with independent parameters:
  - Attack (0.01s - 5s)
  - Decay (0.01s - 5s)
  - Sustain (0.0 - 1.0 level)
  - Release (0.01s - 5s)
- Real-time parameter updates without audio glitches
- MIDI note-on/off with velocity sensitivity

### 🎛️ Audio Processing

#### Greyhole Reverb (Primary)
High-quality algorithmic reverb based on Julian Parker's Greyhole algorithm:
- **Nested diffusion network** (4 levels) for natural, dense reverb tails
- **Prime number delays** (1302 primes, 2-10667) to eliminate metallic artifacts
- **Modulated delay lines** with LFO for shimmer and movement
- **7 controllable parameters**:
  - Delay Time (0.001 - 1.45s)
  - Size (0.5 - 3.0, spatial characteristics)
  - Damping (0.0 - 0.99, high-frequency roll-off)
  - Mix (0.0 - 1.0, dry/wet blend)
  - Decay/Feedback (0.0 - 1.0)
  - Diffusion (0.0 - 0.99, density control)
  - Modulation Depth (0.0 - 1.0)
  - Modulation Frequency (0.0 - 10.0 Hz)

#### Filter Section
Four filter types with real-time parameter control:
- **Lowpass**: One-pole TPT (Trapezoidal) design
- **Highpass**: Complementary one-pole filter
- **High Shelf**: One-pole bilinear transform with dB gain control
- **Low Shelf**: One-pole bilinear transform with dB gain control
- **Cutoff frequency**: 20 Hz - 20 kHz (logarithmic mapping)
- **Gain control**: ±12dB for shelf filters
- Independent stereo processing

### 🎚️ MIDI Integration
- **Automatic MIDI device detection** with preference for Arturia keyboards
- **Full MIDI implementation**:
  - Note On/Off messages
  - Control Change (CC) messages
  - Velocity sensitivity
- **MIDI Learn** for CC mapping to parameters
- **Hot-swappable MIDI devices** (change without restart)
- Thread-safe MIDI message processing in audio callback

### 💾 Preset Management
- **Save/Load presets** with user-friendly names
- **INI-style preset files** for easy editing
- Preset directory: `~/.config/wakefield/presets/`
- **Auto-restore state** on device changes
- Presets store all synthesis parameters:
  - Envelope settings (ADSR)
  - Waveform type
  - Master volume
  - All reverb parameters
  - All filter parameters
  - MIDI CC mappings

### 🖥️ Terminal User Interface (ncurses)

#### Multiple Pages:
1. **Main Page** - Envelope, waveform, master volume, voice count
2. **Reverb Page** - All 8 reverb parameters with visual feedback
3. **Filter Page** - Filter type, cutoff, gain controls
4. **Config Page** - Audio/MIDI device selection and system info
5. **Test Page** - Real-time oscilloscope visualization

#### UI Features:
- **Visual parameter bars** with real-time value display
- **Active voice counter** showing current polyphony
- **Console message system** for status updates and errors
- **Popup menus** for selection (waveforms, filters, presets, devices)
- **Color-coded interface** for better readability
- **Keyboard shortcuts** for all operations
- **Text input** for preset naming

### ⚙️ Audio System
- **RtAudio backend** for cross-platform audio
- **48 kHz sample rate** (configurable)
- **256-frame buffer** for low latency (~5.3ms at 48kHz)
- **Stereo output** with independent channel processing
- **Device hot-swapping** with state preservation
- **Graceful degradation** (runs without audio if unavailable)

## Technical Architecture

### Core Components

#### Voice Management (`voice.h`)
- Encapsulates oscillator + envelope for each voice
- Automatic voice deactivation when envelope completes
- Per-sample audio generation

#### Oscillator (`oscillator.h/cpp`)
- Phase-accumulator design with anti-aliasing considerations
- Waveform generation:
  - **Sine**: `sin(2πφ)` lookup
  - **Square**: Naive `-1/+1` (band-limited version planned)
  - **Sawtooth**: `2φ/2π - 1`
  - **Triangle**: Folded sawtooth

#### Envelope Generator (`envelope.h/cpp`)
- State-machine based ADSR implementation
- Sample-accurate transitions
- Linear envelope segments
- Rate-based computation for efficiency

#### Filter Bank (`filters.hpp`)
- **OnePoleTPT**: Topology-preserving transform (trapezoidal integration)
- **Shelf filters**: Bilinear transform with frequency prewarping
- State-variable design for modulation stability
- Denormal protection

#### Reverb (`reverb.h/cpp`, `greyhole_dsp.h`)
- Faust-compiled DSP wrapped in C++ class
- Manages stereo interleaving/deinterleaving
- Parameter smoothing to avoid zipper noise
- ~262KB delay buffer allocation

#### Synth Engine (`synth.h/cpp`)
- Central audio processing coordinator
- Voice allocation/deallocation logic
- MIDI-to-frequency conversion (`440 * 2^((note-69)/12)`)
- Parameter thread-safety via atomic operations
- Processes reverb and filters in series

#### MIDI Handler (`midi.h/cpp`)
- RtMidi wrapper for cross-platform MIDI
- Message parsing (Note On/Off, CC)
- Port scanning and auto-detection
- Error callback routing to UI console

#### UI Manager (`ui.h/cpp`)
- Multi-page interface with tab navigation
- Atomic parameter passing to audio thread
- Non-blocking input handling
- Thread-safe console message queue
- Oscilloscope rendering with fade effects

#### Preset Manager (`preset.h/cpp`)
- INI file parsing/writing
- Directory management (`mkdir -p` equivalent)
- Preset discovery and listing
- Type-safe parameter serialization

## Build System

### Requirements
- **CMake** 3.10+
- **C++17** compiler (GCC 7+, Clang 5+, MSVC 2017+)
- **RtAudio** library (`librtaudio-dev`)
- **RtMidi** library (`librtmidi-dev`)
- **ncurses** library (`libncurses-dev`)
- **Faust** compiler (for DSP modifications only)

### Building

```bash
mkdir build
cd build
cmake ..
make
```

Executable: `build/synth` (~157KB)

## Usage

### Launching
```bash
./build/synth
```

### Keyboard Controls

#### Global
- **Tab** / **Shift+Tab**: Navigate between pages
- **↑/↓**: Select parameter row
- **←/→**: Decrease/increase parameter value
- **Enter**: Open popup menu (for selectable parameters)
- **q**: Quit application

#### Preset Management
- **s**: Save preset (opens text input)
- **l**: Load preset (opens preset list)

#### Special Functions
- **t** (on Config page): Toggle test oscillator
- **f** (on Filter page): Toggle filter on/off
- **r** (on Reverb page): Toggle reverb on/off
- **m** (on Filter page): MIDI Learn for cutoff frequency

### MIDI Control
1. Connect MIDI keyboard
2. Navigate to Config page to verify device
3. Play notes for immediate sound output
4. Use MIDI Learn:
   - Press **m** on Filter page
   - Move a CC controller (e.g., mod wheel)
   - Controller is now mapped to filter cutoff

### Device Configuration
- Audio and MIDI devices can be changed from Config page
- Preferences stored in: `~/.config/wakefield/device_config.txt`
- App automatically restarts with new devices, preserving state

## Configuration Files

### Device Config
**Location**: `~/.config/wakefield/device_config.txt`
```
audio_device=2
midi_port=1
```

### Preset Format
**Location**: `~/.config/wakefield/presets/<name>.preset`
```ini
[envelope]
attack=0.010000
decay=0.100000
sustain=0.700000
release=0.200000

[oscillator]
waveform=0

[main]
master_volume=0.500000

[reverb]
enabled=1
delay_time=0.500000
size=0.500000
damping=0.500000
mix=0.300000
decay=0.500000
diffusion=0.500000
mod_depth=0.100000
mod_freq=2.000000

[filter]
enabled=0
type=0
cutoff=1000.000000
gain=0.000000
cutoff_cc=-1
```

## Architecture Highlights

### Thread Safety
- **Atomic parameters** for lock-free audio thread communication
- **MIDI processing** in audio callback for zero-latency
- **Message queue** for console output from any thread
- **Double-buffering** for UI state updates

### Performance Optimizations
- **Pre-computed coefficients** for filters (update only on change)
- **Voice allocation** uses first-free strategy (O(n) lookup)
- **Rate-based envelopes** (increment, not time calculation)
- **Inline DSP** critical paths
- **Denormal protection** in filters and reverb

### Audio Quality
- **48 kHz native sample rate** for wide frequency response
- **Anti-aliasing considerations** (planned for oscillators)
- **Bilinear transform** with frequency prewarping for accurate filter response
- **Prime-based delays** in reverb eliminate comb filtering artifacts
- **Smooth parameter interpolation** prevents zipper noise

## DSP Pipeline

```
MIDI Input → Voice Allocation
                ↓
   [Voice 1..8: Oscillator → Envelope] → Sum
                ↓
         Master Volume
                ↓
         Filter (optional)
                ↓
         Reverb (optional)
                ↓
          Stereo Output
```

## Reverb Implementation Details

The Greyhole reverb is a state-of-the-art algorithm featuring:

1. **Nested Diffusion Network**:
   - 4 recursive diffusion stages
   - Householder rotations for decorrelation
   - Modulated allpass filters at each stage

2. **Prime Number Taps**:
   - 1302 prime delays prevent periodic resonances
   - Inline waveform table (no external dependencies)
   - Dynamic interpolation for smooth size changes

3. **Feedback Network**:
   - Modulated delay lines (LFO-driven)
   - High-frequency damping filters
   - Configurable decay time

4. **Stereo Processing**:
   - Independent L/R processing paths
   - Cross-coupling for spatial diffusion
   - Width control via rotation angles

## Future Enhancements

### Planned Features
1. **Oscillator Improvements**:
   - Band-limited waveforms (BLEP/PolyBLEP)
   - Wavetable synthesis
   - Multiple oscillators per voice

2. **Filter Enhancements**:
   - Resonant filters (2-pole SVF)
   - Filter envelope modulation
   - Multiple filter types (Moog ladder, etc.)

3. **Modulation**:
   - LFOs with multiple destinations
   - Modulation matrix
   - Envelope followers

4. **Effects**:
   - Delay/echo
   - Chorus/flanger
   - Distortion/saturation

5. **UI Improvements**:
   - Spectral analyzer
   - Envelope visualizer
   - MIDI activity indicators

6. **Performance**:
   - SIMD optimizations
   - Multi-threaded voice rendering
   - Lower latency modes

## Credits

### Greyhole Reverb
- **Algorithm**: Julian Parker (2013)
- **Bug Fixes**: Till Bovermann
- **License**: GPL2+

### Libraries
- **RtAudio**: Gary P. Scavone
- **RtMidi**: Gary P. Scavone
- **Faust**: GRAME (v2.37.3)
- **ncurses**: GNU Project

### Project
- **Author**: Brian Varren (The Last Node)
- **License**: (Add your license)

## License

(Specify your license here - GPL2+ recommended due to Greyhole component)

## Development

### Project Structure
```
wakefield/
├── src/              # Core C++ source files
│   ├── main.cpp      # Entry point, audio/MIDI setup
│   ├── synth.*       # Main synthesis engine
│   ├── voice.h       # Voice structure
│   ├── oscillator.*  # Waveform generation
│   ├── envelope.*    # ADSR envelope
│   ├── filters.hpp   # Filter implementations
│   ├── reverb.*      # Reverb wrapper
│   ├── midi.*        # MIDI handling
│   ├── ui.*          # ncurses interface
│   └── preset.*      # Preset management
├── reverb/           # Faust DSP files
│   ├── GreyholeRaw.dsp       # Faust source
│   ├── greyhole_dsp.h        # Generated C++
│   └── greyhole.cpp          # (deprecated)
├── build/            # Build directory
├── CMakeLists.txt    # Build configuration
└── GREYHOLE_INTEGRATION.md   # Reverb integration notes
```

### Modifying the Reverb

To regenerate the reverb DSP (if modifying `GreyholeRaw.dsp`):

```bash
cd reverb/
faust -i -scn GreyholeReverb GreyholeRaw.dsp -o greyhole_dsp.h
```

Then rebuild the project.

---

**Wakefield** - A professional software synthesizer for the terminal. Play, tweak, and create music without leaving your command line.