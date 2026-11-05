#!/usr/bin/env python3
"""Receive XIAO ESP32S3 Sense VAD clips over USB Serial and save WAV files.

Requires Python 3.9+ and pyserial for normal use:
    python -m pip install pyserial
    python3 tools/receive_audio.py --list-ports
    py tools/receive_audio.py --port YOUR_PORT    # Windows
    python3 tools/receive_audio.py --port YOUR_PORT  # macOS

Only one application can own the serial port. Close Arduino Serial Monitor
before running this script. The receiver sends READY\\n to the board.

Protocol: ASCII '# ...\\n' status lines and binary packets:
    4 bytes magic 'VAD1'
    uint32 little-endian sample_rate
    uint32 little-endian sample_count
    uint32 little-endian clip_number
    sample_count * signed int16 PCM (little endian)
"""

from __future__ import annotations

import argparse
import io
import math
from pathlib import Path
import struct
import sys
import tempfile
import time
import wave

MAGIC = b"VAD1"
HEADER = struct.Struct("<III")
MAX_SAMPLES = 80_000  # 5 seconds at 16 kHz


def read_exact(port, byte_count: int, timeout_seconds: float = 15.0) -> bytes:
    """Get a complete binary field, allowing the serial port to read partially."""
    result = bytearray()
    deadline = time.monotonic() + timeout_seconds
    while len(result) < byte_count:
        chunk = port.read(min(8192, byte_count - len(result)))
        if chunk:
            result.extend(chunk)
        elif time.monotonic() >= deadline:
            raise TimeoutError(
                f"Timed out after receiving {len(result)} / {byte_count} bytes. "
                "Check USB cable and restart the receiver."
            )
    return bytes(result)


def available_filename(directory: Path, sequence: int) -> Path:
    """Never overwrite recordings from an earlier board reset."""
    base = directory / f"voice_{sequence:04d}"
    path = base.with_suffix(".wav")
    suffix = 1
    while path.exists():
        path = directory / f"voice_{sequence:04d}_{suffix:02d}.wav"
        suffix += 1
    return path


def save_wav(path: Path, audio: bytes, sample_rate: int) -> None:
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        wav.writeframes(audio)


def receive_loop(port, output_directory: Path, max_clips: int = 0) -> int:
    """Parse mixed text/binary traffic; return after max_clips if nonzero."""
    output_directory.mkdir(parents=True, exist_ok=True)
    clips_saved = 0
    while True:
        first = port.read(1)
        if not first:
            continue
        if first == b"#":
            line = port.readline().decode("utf-8", errors="replace").strip()
            print("# " + line, flush=True)
            continue
        if first != MAGIC[:1]:
            # Ignore unsolicited boot bytes or partial data after reconnect.
            continue
        rest = read_exact(port, len(MAGIC) - 1)
        if first + rest != MAGIC:
            print("Warning: ignored unrecognized serial prefix", file=sys.stderr)
            continue

        sample_rate, n_samples, sequence = HEADER.unpack(
            read_exact(port, HEADER.size)
        )
        if sample_rate != 16_000 or not (0 < n_samples <= MAX_SAMPLES):
            raise ValueError(
                f"Invalid packet header: rate={sample_rate}, samples={n_samples}. "
                "Check baud/settings or reconnect."
            )

        pcm = read_exact(port, n_samples * 2)
        path = available_filename(output_directory, sequence)
        save_wav(path, pcm, sample_rate)
        clips_saved += 1
        print(
            f"[SAVED] {path}  |  {n_samples} samples  |  "
            f"{n_samples / sample_rate:.3f} seconds",
            flush=True,
        )
        if max_clips and clips_saved >= max_clips:
            return clips_saved


def self_test() -> None:
    """Exercise binary parsing and WAV output without hardware or pyserial."""
    frames = []
    for clip_id, size in ((1, 4000), (2, 8000)):
        pcm = b"".join(
            struct.pack("<h", round(8000 * math.sin(2 * math.pi * 440 * i / 16000)))
            for i in range(size)
        )
        frames.append(
            f"# END clip={clip_id} reason=silence\n".encode()
            + MAGIC
            + HEADER.pack(16000, size, clip_id)
            + pcm
        )
    stream = io.BytesIO(b"# RECEIVER_READY\n" + b"".join(frames))
    with tempfile.TemporaryDirectory() as temp:
        folder = Path(temp)
        assert receive_loop(stream, folder, max_clips=2) == 2
        for clip_id, size in ((1, 4000), (2, 8000)):
            with wave.open(str(folder / f"voice_{clip_id:04d}.wav"), "rb") as wav:
                assert wav.getframerate() == 16000
                assert wav.getnchannels() == 1
                assert wav.getsampwidth() == 2
                assert wav.getnframes() == size
    print("SELF-TEST PASSED: two mock packets produced valid WAV files.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Your board's port from --list-ports")
    parser.add_argument("--baud", type=int, default=115200, help="USB CDC setting (default 115200)")
    parser.add_argument("--output", type=Path, default=Path("recordings"), help="WAV folder, relative to your current directory")
    parser.add_argument("--max-clips", type=int, default=0, help="Exit after N recordings (0 = unlimited)")
    parser.add_argument("--list-ports", action="store_true", help="Show serial ports and exit")
    parser.add_argument("--self-test", action="store_true", help="Test parser/WAV writing without a board")
    args = parser.parse_args()

    if args.self_test:
        self_test()
        return

    try:
        import serial  # type: ignore
        from serial.tools import list_ports  # type: ignore
    except ImportError:
        parser.error("pyserial is not installed. Run: python -m pip install pyserial")

    if args.list_ports:
        found = list(list_ports.comports())
        if not found:
            print("No serial ports found.")
        for item in found:
            print(f"{item.device}: {item.description}")
        return

    if not args.port:
        parser.error("--port is required (try --list-ports)")
    if args.max_clips < 0:
        parser.error("--max-clips must be 0 or positive")

    args.output = args.output.expanduser().resolve()
    print(f"WAV output: {args.output}", flush=True)
    print(f"Opening {args.port} at {args.baud} (native USB CDC)...", flush=True)
    try:
        with serial.Serial(args.port, args.baud, timeout=0.3, write_timeout=3) as port:
            # Opening a port can reset some boards. Let setup() settle and
            # allow the microphone to calibrate in a quiet room.
            time.sleep(3.5)
            port.reset_input_buffer()
            port.write(b"READY\n")
            port.flush()
            print("Receiver armed. Speak near the board; press Ctrl+C to stop.")
            receive_loop(port, args.output, args.max_clips)
    except KeyboardInterrupt:
        print("\nStopped by user.")
    except (OSError, TimeoutError, ValueError, serial.SerialException) as exc:
        print(f"Receiver error: {exc}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
