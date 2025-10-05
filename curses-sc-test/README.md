# Curses Loop Sampler Prototype

Phase 2 introduces a polyphonic SuperCollider sampler with ADSR envelopes and
voice management. The Python/curses client now reacts to MIDI note events,
controls envelope parameters, and displays the current voice load.

## Requirements

- Python 3.9+
- SuperCollider (tested with 3.12+)
- `pip install -r requirements.txt`
  - For hardware MIDI devices you also need a backend such as
    `pip install python-rtmidi`.
- Ensure the sample exists at `/srv/storage/Dropbox/_reaktor/Instruments/Brutalisk/default samples/bass ebow lo.wav`

## SuperCollider setup

1. Open SuperCollider.
2. Load `supercollider/loop_sampler_setup.scd` and evaluate it (Cmd/Ctrl+Enter).
   - Boots the audio server, loads `bass ebow lo.wav`, defines a
     `\loopSamplerPlayer` SynthDef, and installs OSC handlers for `/note_on`,
     `/note_off`, `/param`, and `/loopSampler/loop`.
   - Voices are stored in `~loopSamplerVoices`, capped at eight with FIFO voice
     stealing. ADSR defaults: attack 10 ms, decay 100 ms, sustain 0.7, release
     500 ms. Loop state lives in `~loopSamplerLoop` and is pushed to all active
     voices whenever it changes.
   - The post window logs note events, loop updates, steals, and parameter
     changes.

Run `Cmd-.` (or `Ctrl-.` on Linux) to clean up everything; the script also
installs a `CmdPeriod.doOnce` hook.

## Python TUI

1. (Optional) create and activate a virtualenv.
2. Install dependencies: `pip install -r requirements.txt`
   - `mido` is mandatory; install a backend (e.g. `python-rtmidi`) if you want
     to read from physical MIDI devices.
3. List MIDI inputs if needed:

   ```bash
   python loop_sampler.py --list-midi
   ```

4. Run the curses client (MIDI auto-detects the Arturia MiniLab mkII; use
   `--midi-port none` to fall back to the computer keyboard only):

   ```bash
   python loop_sampler.py --host 127.0.0.1 --port 57120 --sample-key plasma --voice-limit 8
   ```

5. Controls inside the TUI:
   - MIDI keyboard: normal note on/off behaviour, velocity sensitive.
   - Computer keyboard fallback: `Z S X D C V G B H N J M ,` trigger notes
     60–72. Press the same key again to send the note-off event. Velocity uses
     `--default-velocity` (default 0.6).
   - Loop window:
     - `a`/`d` (`←`/`→`) nudge loop start (Shift = coarse)
     - `j`/`l` (`↓`/`↑`) nudge loop end (Shift = coarse)
   - ADSR tweaks:
     - `t`/`T`: attack −/+ (shift = coarse step)
     - `y`/`Y`: decay −/+ (shift = coarse)
     - `u`/`U`: sustain −/+ (shift = coarse)
     - `i`/`I`: release −/+ (shift = coarse)
   - Panic: `;` issues note-off to every active voice (helpful if you forget to
     toggle a fallback key off).
   - `Q`: quit.

   The HUD shows the shared loop window (numeric + bar), ADSR values (with
   ranges), and live voice usage. The counter turns yellow at ≥7/8 voices, red
   at 8/8.

### Troubleshooting

- `python-osc is required`: reinstall dependencies.
- `mido is required` or no MIDI ports: install `mido` + a backend, and verify the
  device is not captured by another app.
- If notes hang, press the same fallback key again (or tap `;` for panic) or use
  `Cmd-.` in SuperCollider to kill all voices.
- Voice stealing is FIFO; when hitting the eight-voice ceiling the oldest voice
  is released so the latest note plays.

## Next steps

- Display recent OSC activity or meters returned from SuperCollider.
- Persist ADSR/loop presets and expose them via hotkeys.
