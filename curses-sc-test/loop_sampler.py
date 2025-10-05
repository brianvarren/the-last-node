"""Curses-based OSC controller for a polyphonic SuperCollider sampler."""
from __future__ import annotations

import argparse
import curses
import socket
import time
from collections import OrderedDict
from dataclasses import dataclass, field
from typing import Dict, Iterable, List, Optional, Tuple

try:
    from pythonosc.udp_client import SimpleUDPClient
except ModuleNotFoundError as exc:  # pragma: no cover
    raise SystemExit(
        "python-osc is required. Install it with `pip install python-osc`."
    ) from exc


OSC_NOTE_ON = "/note_on"
OSC_NOTE_OFF = "/note_off"
OSC_PARAM = "/param"
OSC_LOOP = "/loopSampler/loop"

MIDI_LIB_ERROR = (
    "mido is required for MIDI support. Install it with `pip install mido "
    "python-rtmidi` (or another backend)."
)

DEFAULT_MIDI_PATTERNS = [
    "arturia minilab mkii:arturia minilab mkii midi 1",
    "arturia minilab mkii midi 1",
    "arturia minilab"
]


@dataclass
class MessageLog:
    max_entries: int = 10
    entries: List[str] = field(default_factory=list)

    def push(self, text: str) -> None:
        timestamp = time.strftime("%H:%M:%S")
        entry = f"[{timestamp}] {text}"
        self.entries.append(entry)
        if len(self.entries) > self.max_entries:
            self.entries.pop(0)

    def rendered(self) -> List[str]:
        if not self.entries:
            return ["(no events yet)"]
        return self.entries[::-1]


@dataclass
class ADSRRange:
    minimum: float
    maximum: float
    step: float


@dataclass
class ADSR:
    attack: float = 0.01
    decay: float = 0.1
    sustain: float = 0.7
    release: float = 0.5
    ranges: Dict[str, ADSRRange] = field(default_factory=dict)

    def __post_init__(self) -> None:
        if not self.ranges:
            self.ranges = {
                "attack": ADSRRange(0.001, 2.0, 0.01),
                "decay": ADSRRange(0.001, 4.0, 0.02),
                "sustain": ADSRRange(0.0, 1.0, 0.02),
                "release": ADSRRange(0.005, 8.0, 0.05),
            }

    def get(self, name: str) -> float:
        return getattr(self, name)

    def set(self, name: str, value: float) -> None:
        rng = self.ranges[name]
        clamped = max(rng.minimum, min(rng.maximum, value))
        setattr(self, name, clamped)

    def adjust(self, name: str, direction: int, *, coarse: bool = False) -> bool:
        rng = self.ranges[name]
        step = rng.step * (5 if coarse else 1)
        current = getattr(self, name)
        new_value = current + direction * step
        new_value = max(rng.minimum, min(rng.maximum, new_value))
        if abs(new_value - current) < 1e-6:
            return False
        setattr(self, name, new_value)
        return True

    def items(self) -> Iterable[Tuple[str, float]]:
        return (("attack", self.attack), ("decay", self.decay), ("sustain", self.sustain), ("release", self.release))


@dataclass
class LoopParameters:
    start: float = 0.0
    end: float = 1.0
    min_span: float = 0.02
    step: float = 0.01
    coarse_step: float = 0.05
    revision: int = 0

    def __post_init__(self) -> None:
        self.start = self._clamp(self.start)
        self.end = self._clamp(self.end)
        if (self.end - self.start) < self.min_span:
            self.end = min(1.0, self.start + self.min_span)
            self.start = max(0.0, self.end - self.min_span)

    @staticmethod
    def _clamp(value: float) -> float:
        return max(0.0, min(1.0, value))

    def adjust_start(self, delta: float) -> bool:
        new_start = self._clamp(self.start + delta)
        max_start = self.end - self.min_span
        if new_start > max_start:
            new_start = max_start
        if abs(new_start - self.start) < 1e-6:
            return False
        self.start = new_start
        self.revision += 1
        return True

    def adjust_end(self, delta: float) -> bool:
        new_end = self._clamp(self.end + delta)
        min_end = self.start + self.min_span
        if new_end < min_end:
            new_end = min_end
        if abs(new_end - self.end) < 1e-6:
            return False
        self.end = new_end
        self.revision += 1
        return True

    def payload(self) -> List[float]:
        return [self.start, self.end, float(self.revision)]


class VoiceTracker:
    def __init__(self, limit: int) -> None:
        self.limit = limit
        self._voices: OrderedDict[int, None] = OrderedDict()

    def note_on(self, note: int) -> None:
        if note in self._voices:
            self._voices.move_to_end(note)
        else:
            self._voices[note] = None

    def note_off(self, note: int) -> None:
        self._voices.pop(note, None)

    @property
    def count(self) -> int:
        return len(self._voices)

    def active_notes(self) -> List[int]:
        return list(self._voices.keys())


class MidiInput:
    def __init__(self, port_name: str) -> None:
        try:
            import mido
        except ModuleNotFoundError as exc:  # pragma: no cover
            raise SystemExit(MIDI_LIB_ERROR) from exc

        self._mido = mido
        resolved = self._resolve_port_name(port_name, mido)
        if resolved is None:
            available = "\n".join(f"  - {name}" for name in mido.get_input_names()) or "  (none)"
            raise SystemExit(
                "Unable to open MIDI port '{0}': unknown port.\nAvailable ports:\n{1}".format(
                    port_name, available
                )
            )
        try:
            self._port = mido.open_input(resolved)
        except IOError as err:  # pragma: no cover
            raise SystemExit(f"Unable to open MIDI port '{resolved}': {err}") from err
        self.name = self._port.name

    @staticmethod
    def list_ports() -> List[str]:  # pragma: no cover - CLI only
        try:
            import mido
        except ModuleNotFoundError:
            raise SystemExit(MIDI_LIB_ERROR) from None
        return mido.get_input_names()

    def iter_pending(self):
        return list(self._port.iter_pending())

    def close(self) -> None:
        self._port.close()

    @staticmethod
    def _resolve_port_name(requested: str, mido) -> Optional[str]:
        names = mido.get_input_names()
        if requested.lower() == "auto":
            for pattern in DEFAULT_MIDI_PATTERNS:
                target = pattern.lower()
                for name in names:
                    if target in name.lower():
                        return name
            return names[0] if names else None
        if requested in names:
            return requested
        requested_lower = requested.lower()
        for name in names:
            if name.lower() == requested_lower:
                return name
        for name in names:
            if requested_lower in name.lower():
                return name
        return None


class KeyboardNoteMapper:
    """Fallback mapping from computer keys to MIDI notes."""

    KEY_MAP: Dict[int, int] = {
        ord("z"): 60,
        ord("s"): 61,
        ord("x"): 62,
        ord("d"): 63,
        ord("c"): 64,
        ord("v"): 65,
        ord("g"): 66,
        ord("b"): 67,
        ord("h"): 68,
        ord("n"): 69,
        ord("j"): 70,
        ord("m"): 71,
        ord(","): 72,
    }

    def __init__(self) -> None:
        self.active: Dict[int, bool] = {}

    def translate(self, key_code: int) -> Optional[Tuple[int, bool]]:
        note = self.KEY_MAP.get(key_code)
        if note is None:
            return None
        state = self.active.get(note, False)
        self.active[note] = not state
        return note, not state


class SamplerTUI:
    def __init__(
        self,
        stdscr: "curses._CursesWindow",
        client: SimpleUDPClient,
        *,
        host: str,
        port: int,
        sample_key: str,
        voice_limit: int,
        default_velocity: float,
        adsr: ADSR,
        loop_params: LoopParameters,
        midi_port: Optional[str],
    ) -> None:
        self.stdscr = stdscr
        self.client = client
        self.host = host
        self.port = port
        self.sample_key = sample_key
        self.voice_limit = voice_limit
        self.default_velocity = max(0.0, min(1.0, default_velocity))
        self.adsr = adsr
        self.loop_params = loop_params
        self.msg_log = MessageLog()
        self.voice_tracker = VoiceTracker(voice_limit)
        self.keyboard_mapper = KeyboardNoteMapper()
        self.midi: Optional[MidiInput] = None
        if midi_port and midi_port.lower() != "none":
            self.midi = MidiInput(midi_port)
        self._init_curses()
        self._send_initial_params()
        self._send_loop_update(reason="init")

    def _init_curses(self) -> None:
        curses.curs_set(0)
        self.stdscr.nodelay(True)
        # Tighten the input polling window so MIDI events are serviced quickly.
        self.stdscr.timeout(1)
        curses.start_color()
        curses.use_default_colors()
        curses.init_pair(1, curses.COLOR_WHITE, -1)
        curses.init_pair(2, curses.COLOR_YELLOW, -1)
        curses.init_pair(3, curses.COLOR_RED, -1)

    def run(self) -> None:
        self.msg_log.push("Ready. Use MIDI keyboard or Z-M row for notes. T/Y/U/I adjust ADSR.")
        try:
            while True:
                self._poll_midi()
                self._draw()
                key = self.stdscr.getch()
                if key == -1:
                    continue
                if key in (ord("q"), ord("Q")):
                    self.msg_log.push("Quitting…")
                    self._draw()
                    time.sleep(0.2)
                    break
                if key == ord(";"):
                    self._panic_all_notes_off()
                    continue
                if self._handle_loop_keys(key):
                    continue
                if self._handle_adsr_keys(key):
                    continue
                if self._handle_keyboard_note(key):
                    continue
        finally:
            if self.midi:
                self.midi.close()

    def _poll_midi(self) -> None:
        if not self.midi:
            return
        for message in self.midi.iter_pending():
            if message.type == "note_on" and message.velocity > 0:
                velocity = message.velocity / 127.0
                self._send_note_on(message.note, velocity, source="MIDI")
            elif message.type in ("note_off",) or (message.type == "note_on" and message.velocity == 0):
                self._send_note_off(message.note, source="MIDI")

    def _handle_adsr_keys(self, key: int) -> bool:
        mapping = {
            ord("t"): ("attack", -1, False),
            ord("T"): ("attack", +1, True),
            ord("y"): ("decay", -1, False),
            ord("Y"): ("decay", +1, True),
            ord("u"): ("sustain", -1, False),
            ord("U"): ("sustain", +1, True),
            ord("i"): ("release", -1, False),
            ord("I"): ("release", +1, True),
        }
        entry = mapping.get(key)
        if not entry:
            return False
        name, direction, coarse = entry
        changed = self.adsr.adjust(name, direction, coarse=coarse)
        if not changed:
            return True
        value = self.adsr.get(name)
        self._send_param_update(name, value)
        self.msg_log.push(f"Param {name} -> {value:.3f}")
        return True

    def _handle_loop_keys(self, key: int) -> bool:
        step = self.loop_params.step
        coarse = self.loop_params.coarse_step
        if key in (ord("a"), curses.KEY_LEFT):
            return self._adjust_loop_start(-step)
        if key == ord("A"):
            return self._adjust_loop_start(-coarse)
        if key in (ord("d"), curses.KEY_RIGHT):
            return self._adjust_loop_start(step)
        if key == ord("D"):
            return self._adjust_loop_start(coarse)
        if key in (ord("j"), curses.KEY_DOWN):
            return self._adjust_loop_end(-step)
        if key == ord("J"):
            return self._adjust_loop_end(-coarse)
        if key in (ord("l"), curses.KEY_UP):
            return self._adjust_loop_end(step)
        if key == ord("L"):
            return self._adjust_loop_end(coarse)
        return False

    def _adjust_loop_start(self, delta: float) -> bool:
        if self.loop_params.adjust_start(delta):
            self._send_loop_update(reason="start")
            return True
        return False

    def _adjust_loop_end(self, delta: float) -> bool:
        if self.loop_params.adjust_end(delta):
            self._send_loop_update(reason="end")
            return True
        return False

    def _handle_keyboard_note(self, key: int) -> bool:
        translation = self.keyboard_mapper.translate(key)
        if translation is None:
            return False
        note, is_on = translation
        if is_on:
            self._send_note_on(note, self.default_velocity, source="KEY")
        else:
            self._send_note_off(note, source="KEY")
        return True

    def _send_note_on(self, note: int, velocity: float, *, source: str) -> None:
        payload = [self.sample_key, int(note), float(velocity)]
        t0 = time.perf_counter()
        try:
            self.client.send_message(OSC_NOTE_ON, payload)
        except OSError as err:
            self.msg_log.push(f"Failed to send note_on: {err}")
            return
        t1 = time.perf_counter()
        self.voice_tracker.note_on(note)
        elapsed_ms = (t1 - t0) * 1000.0
        self.msg_log.push(
            f"{source} note_on {note} vel {velocity:.2f} (OSC {elapsed_ms:.1f} ms)"
        )

    def _send_note_off(self, note: int, *, source: str) -> None:
        payload = [self.sample_key, int(note)]
        try:
            self.client.send_message(OSC_NOTE_OFF, payload)
        except OSError as err:
            self.msg_log.push(f"Failed to send note_off: {err}")
            return
        self.voice_tracker.note_off(note)
        self.msg_log.push(f"{source} note_off {note}")

    def _send_param_update(self, param: str, value: float) -> None:
        payload = [param, float(value)]
        try:
            self.client.send_message(OSC_PARAM, payload)
        except OSError as err:
            self.msg_log.push(f"Failed to send param update: {err}")

    def _send_initial_params(self) -> None:
        for name, value in self.adsr.items():
            self._send_param_update(name, value)

    def _send_loop_update(self, *, reason: str) -> None:
        payload = self.loop_params.payload()
        try:
            self.client.send_message(OSC_LOOP, payload)
        except OSError as err:
            self.msg_log.push(f"Failed to update loop: {err}")
            return
        self.msg_log.push(
            "Loop {0}: start {1:.3f}, end {2:.3f}".format(
                reason,
                self.loop_params.start,
                self.loop_params.end,
            )
        )

    def _panic_all_notes_off(self) -> None:
        active = list(self.voice_tracker.active_notes())
        if not active:
            self.msg_log.push("PANIC: no active notes")
            return
        for note in active:
            self._send_note_off(note, source="PANIC")
        self.msg_log.push(f"PANIC: sent note_off for {len(active)} note(s)")

    def _draw(self) -> None:
        self.stdscr.erase()
        max_y, max_x = self.stdscr.getmaxyx()

        title = "Loop Sampler Polyphonic Controller"
        self.stdscr.addstr(1, max(0, (max_x - len(title)) // 2), title, curses.A_BOLD)

        controls_start = 3
        controls = [
            "Q: quit",
            "MIDI keyboard: play notes (velocity sensitive)",
            "Keys Z-S-X-D ... , : fallback note (press again to release)",
            "T/t attack, Y/y decay, U/u sustain, I/i release (Shift = coarse)",
            "A/D (←/→): loop start, J/L (↓/↑): loop end (Shift = coarse)",
            ";: panic (all notes off)",
        ]
        for idx, line in enumerate(controls, start=controls_start):
            if idx >= max_y - 10:
                break
            self.stdscr.addstr(idx, 2, line)

        info_lines = [
            f"OSC target: {self.host}:{self.port}",
            f"Sample key: {self.sample_key}",
            f"MIDI input: {self.midi.name if self.midi else 'disabled'}",
        ]
        for idx, line in enumerate(info_lines, start=controls_start + len(controls) + 1):
            if idx >= max_y - 8:
                break
            self.stdscr.addstr(idx, 2, line)

        loop_y = controls_start + len(controls) + len(info_lines) + 2
        if loop_y < max_y - 8:
            self.stdscr.addstr(loop_y, 2, "Loop window:", curses.A_UNDERLINE)
            self.stdscr.addstr(
                loop_y + 1,
                4,
                f"Start {self.loop_params.start:.3f}  End {self.loop_params.end:.3f}  Δ {(self.loop_params.end - self.loop_params.start):.3f}"
            )
            self.stdscr.addstr(loop_y + 2, 4, self._loop_bar(max_x - 6))

        adsr_y = loop_y + 4
        if adsr_y < max_y - 6:
            self.stdscr.addstr(adsr_y, 2, "ADSR:", curses.A_UNDERLINE)
            for offset, (name, value) in enumerate(self.adsr.items(), start=1):
                rng = self.adsr.ranges[name]
                self.stdscr.addstr(
                    adsr_y + offset,
                    4,
                    f"{name.capitalize():<8} {value:>6.3f}  [{rng.minimum:.3f}, {rng.maximum:.3f}]"
                )

        voices_y = adsr_y + 6
        if voices_y < max_y - 4:
            level = self.voice_tracker.count
            ratio = level / float(self.voice_limit)
            if ratio >= 1.0:
                color = curses.color_pair(3) | curses.A_BOLD
            elif ratio >= 0.875:
                color = curses.color_pair(2) | curses.A_BOLD
            else:
                color = curses.A_NORMAL
            self.stdscr.addstr(voices_y, 2, f"Voices: {level}/{self.voice_limit}", color)
            if level:
                notes = ", ".join(str(n) for n in self.voice_tracker.active_notes())
                self.stdscr.addstr(voices_y + 1, 4, f"Active notes: {notes}")

        log_y = max_y - 6
        self.stdscr.addstr(log_y, 2, "Recent events:", curses.A_UNDERLINE)
        for idx, line in enumerate(self.msg_log.rendered(), start=log_y + 1):
            if idx >= max_y - 1:
                break
            self.stdscr.addstr(idx, 4, line[: max_x - 6])

        self.stdscr.refresh()

    def _loop_bar(self, width: int) -> str:
        usable = max(20, width)
        segments = usable - 2
        start_idx = int(round(self.loop_params.start * segments))
        end_idx = int(round(self.loop_params.end * segments))
        if end_idx <= start_idx:
            end_idx = min(segments, start_idx + 1)
        if end_idx > segments:
            end_idx = segments
        chars = ["-"] * (segments + 1)
        for idx in range(start_idx, end_idx):
            chars[idx] = "="
        chars[start_idx] = "|"
        chars[end_idx] = "|"
        return "[" + "".join(chars)[: segments + 1] + "]"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Polyphonic sampler OSC controller")
    parser.add_argument("--host", default="127.0.0.1", help="SuperCollider OSC host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=57120, help="SuperCollider OSC port (default: 57120)")
    parser.add_argument("--sample-key", default="plasma", help="Key for the buffer in SuperCollider (default: plasma)")
    parser.add_argument("--voice-limit", type=int, default=8, help="Expected max voices (display only)")
    parser.add_argument("--default-velocity", type=float, default=0.6, help="Velocity used for keyboard fallback (0-1)")
    parser.add_argument(
        "--midi-port",
        default="auto",
        help="MIDI input port (default: auto-detect Arturia MiniLab mkII; use 'none' to disable)",
    )
    parser.add_argument("--list-midi", action="store_true", help="List available MIDI input ports and exit")
    parser.add_argument("--loop-start", type=float, default=0.0, help="Initial loop start (0-1, default: 0.0)")
    parser.add_argument("--loop-end", type=float, default=1.0, help="Initial loop end (0-1, default: 1.0)")
    parser.add_argument("--loop-step", type=float, default=0.01, help="Fine adjustment step for loop controls")
    parser.add_argument(
        "--loop-coarse-step",
        type=float,
        default=0.05,
        help="Coarse adjustment step for loop controls",
    )
    parser.add_argument(
        "--loop-min-span",
        type=float,
        default=0.02,
        help="Minimum normalized span between loop start and end",
    )
    return parser.parse_args()


def maybe_list_midi(args: argparse.Namespace) -> None:
    if not args.list_midi:
        return
    ports = MidiInput.list_ports()
    if not ports:
        print("No MIDI input ports available.")
    else:
        print("Available MIDI input ports:")
        for name in ports:
            print(f"  - {name}")
    raise SystemExit(0)


def resolve_client(host: str, port: int) -> SimpleUDPClient:
    try:
        socket.gethostbyname(host)
    except socket.gaierror as err:
        raise SystemExit(f"Unable to resolve host '{host}': {err}") from err
    return SimpleUDPClient(host, port)


def curses_main(stdscr: "curses._CursesWindow", args: argparse.Namespace) -> None:
    client = resolve_client(args.host, args.port)
    adsr = ADSR()
    loop_params = LoopParameters(
        start=args.loop_start,
        end=args.loop_end,
        step=args.loop_step,
        coarse_step=args.loop_coarse_step,
        min_span=args.loop_min_span,
    )
    app = SamplerTUI(
        stdscr,
        client,
        host=args.host,
        port=args.port,
        sample_key=args.sample_key,
        voice_limit=args.voice_limit,
        default_velocity=args.default_velocity,
        adsr=adsr,
        loop_params=loop_params,
        midi_port=args.midi_port,
    )
    app.run()


def main() -> None:
    args = parse_args()
    maybe_list_midi(args)
    curses.wrapper(curses_main, args)


if __name__ == "__main__":
    main()
