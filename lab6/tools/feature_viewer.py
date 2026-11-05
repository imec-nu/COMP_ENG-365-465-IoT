#!/usr/bin/env python3
"""Send test WAVs to the ESP32, or save and plot its microphone features.

Run from the lab6 folder. First use --list-ports, then select your board's
port with --port. Use --tone FILE for a supplied WAV or --listen for speech.
Requires pyserial, numpy, and matplotlib in your Python environment.
"""

from __future__ import annotations

import argparse
import csv
import io
import math
from pathlib import Path
import struct
import sys
import tempfile
import time
import wave

FRAME_SAMPLES = 512
SAMPLE_RATE = 16000
FEATURES = 8
SPECTRUM_BINS = 129  # 0 to 4000 Hz inclusive
MAX_FRAMES = 400
MAX_SAMPLES = 80000
VAD_MAGIC = b"VAD1"
FEATURE_MAGIC = b"FEA2"
VAD_HEADER = struct.Struct("<III")
FEATURE_HEADER = struct.Struct("<IIIIf")
BAND_LABELS = ["0–250", "250–500", "500–750", "750–1000",
               "1000–1500", "1500–2000", "2000–3000", "3000–4000"]


def read_exact(port, count: int, timeout: float = 15.0) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + timeout
    while len(data) < count:
        piece = port.read(min(count - len(data), 8192))
        if piece:
            data.extend(piece)
        elif time.monotonic() > deadline:
            raise TimeoutError(f"USB read stalled ({len(data)}/{count} bytes)")
    return bytes(data)


def read_status_tail(port, timeout: float = 10.0) -> bytes:
    # A short serial timeout can split a status line across reads. Never
    # discard the tail of "# CHUNK ...\\n" when that happens.
    data = bytearray()
    deadline = time.monotonic() + timeout
    while True:
        ch = port.read(1)
        if ch:
            data.extend(ch)
            if ch == b"\n":
                return bytes(data)
        elif time.monotonic() > deadline:
            raise TimeoutError(f"Incomplete status line after {len(data)} bytes")


def read_event(port):
    """Return ('text', str), ('wav', tuple), ('features', dict), or None."""
    first = port.read(1)
    if not first:
        return None
    if first == b"#":
        return "text", (first + read_status_tail(port)).decode("utf-8", errors="replace").strip()
    if first not in (b"V", b"F"):
        return "text", f"# Unexpected byte 0x{first[0]:02x}; awaiting packet boundary"
    magic = first + read_exact(port, 3)
    if magic == VAD_MAGIC:
        rate, sample_count, sequence = VAD_HEADER.unpack(read_exact(port, VAD_HEADER.size))
        if rate != SAMPLE_RATE or not 0 < sample_count <= MAX_SAMPLES:
            raise ValueError(f"Bad VAD1 header: rate={rate}, samples={sample_count}")
        pcm = read_exact(port, sample_count * 2)
        return "wav", (sequence, rate, pcm)
    if magic == FEATURE_MAGIC:
        rate, sequence, frame_count, bins, peak_hz = FEATURE_HEADER.unpack(
            read_exact(port, FEATURE_HEADER.size)
        )
        if rate != SAMPLE_RATE or not 0 < frame_count <= MAX_FRAMES or bins != SPECTRUM_BINS:
            raise ValueError(
                f"Bad FEA2 header: rate={rate}, frames={frame_count}, bins={bins}"
            )
        spectrum_data = read_exact(port, bins * 4)
        row_data = read_exact(port, frame_count * FEATURES * 4)
        spectrum = struct.unpack("<" + "f" * bins, spectrum_data)
        values = struct.unpack("<" + "f" * (frame_count * FEATURES), row_data)
        rows = [list(values[i * FEATURES: (i + 1) * FEATURES]) for i in range(frame_count)]
        if (not math.isfinite(peak_hz) or any(not math.isfinite(v) for v in values)
                or any(not math.isfinite(v) for v in spectrum)):
            raise ValueError("Feature packet contains non-finite data")
        return "features", dict(sequence=sequence, sample_rate=rate, frames=frame_count,
                                peak_hz=peak_hz, spectrum=spectrum, matrix=rows)
    raise ValueError(f"Unexpected packet magic {magic!r}; reconnect the board")


def unique_base(folder: Path, stem: str) -> Path:
    candidate = folder / stem
    index = 1
    while any((Path(str(candidate) + ext)).exists() for ext in
              (".wav", ".csv", "_spectrum.png", "_features.png")):
        candidate = folder / f"{stem}_{index:02d}"
        index += 1
    return candidate


def save_wav(path: Path, rate: int, pcm: bytes) -> None:
    with wave.open(str(path), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(rate)
        f.writeframes(pcm)


def save_outputs(info: dict, base: Path) -> None:
    # Only visualization is done on the computer. All feature numbers come
    # directly from the ESP32, not a duplicate Python FFT implementation.
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np

    freq = np.arange(SPECTRUM_BINS) * SAMPLE_RATE / FRAME_SAMPLES
    spectrum = np.asarray(info["spectrum"])
    matrix = np.asarray(info["matrix"])
    peak_hz = info["peak_hz"]

    fig, ax = plt.subplots(figsize=(9, 4))
    ax.plot(freq, spectrum, lw=1.1)
    ax.axvline(peak_hz, linestyle="--", lw=1.1,
               label=f"ESP32 detected peak: {peak_hz:.1f} Hz")
    ax.set(xlabel="Frequency (Hz)", ylabel="Mean FFT magnitude",
           title=f"ESP32 frequency spectrum ({info['frames']} frames)", xlim=(0, 4000))
    ax.legend()
    fig.tight_layout()
    fig.savefig(str(base) + "_spectrum.png", dpi=160)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(9, 4.6))
    duration = matrix.shape[0] * FRAME_SAMPLES / SAMPLE_RATE
    image = ax.imshow(matrix.T, aspect="auto", origin="lower", interpolation="nearest",
                      extent=(0, duration, -0.5, FEATURES - 0.5))
    ax.set_yticks(range(FEATURES), BAND_LABELS)
    ax.set(xlabel="Time (seconds)", ylabel="Frequency band (Hz)",
           title=f"ESP32 log-band energy features ({info['frames']} × 8)")
    fig.colorbar(image, ax=ax, label="log(1 + band power)")
    fig.tight_layout()
    fig.savefig(str(base) + "_features.png", dpi=160)
    plt.close(fig)

    with open(str(base) + ".csv", "w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(["time_s"] + [f"band_{i+1}_{BAND_LABELS[i]}Hz" for i in range(FEATURES)])
        for i, row in enumerate(matrix):
            writer.writerow([f"{i * FRAME_SAMPLES / SAMPLE_RATE:.3f}"] +
                            [f"{v:.6f}" for v in row])

    print(f"[ESP32] frames={info['frames']}, peak={peak_hz:.2f} Hz", flush=True)
    print(f"[SAVED] {base}_spectrum.png; {base}_features.png; {base}.csv", flush=True)


def load_wav_frames(path: Path) -> list[bytes]:
    with wave.open(str(path), "rb") as f:
        if f.getframerate() != SAMPLE_RATE or f.getnchannels() != 1 or f.getsampwidth() != 2 or f.getcomptype() != "NONE":
            raise ValueError("Test WAV must be 16kHz mono, 16-bit uncompressed PCM")
        audio = f.readframes(f.getnframes())
    frame_bytes = FRAME_SAMPLES * 2
    frames = [audio[i:i + frame_bytes] for i in range(0, len(audio), frame_bytes)]
    if not frames:
        raise ValueError("WAV is empty")
    if len(frames) > MAX_FRAMES:
        raise ValueError(f"WAV too long: {len(frames)} frames; max {MAX_FRAMES}")
    frames[-1] = frames[-1].ljust(frame_bytes, b"\0") # 10s = 312.5 frames
    return frames


def send_tone(port, wav: Path, output: Path) -> None:
    frames = load_wav_frames(wav)
    print(f"Sending {wav.name}: {len(frames)} frames to ESP32 ({len(frames) * 32}ms incl. padding)")
    print("Protocol v2: USB 128-byte chunk requests (8 requests per frame)")
    port.write(f"TEST {len(frames)}\n".encode("ascii"))
    port.flush()
    sent_frames = 0
    sent_chunks = 0
    next_frame = 0
    next_offset = 0
    last_progress = time.monotonic()
    while True:
        event = read_event(port)
        if event is None:
            if time.monotonic() - last_progress > 12:
                raise TimeoutError("No response during TEST. Check USB port and reboot ESP32.")
            continue
        last_progress = time.monotonic()
        kind, value = event
        if kind == "text":
            if value.startswith("# CHUNK "):
                try:
                    _, label, frame_str, offset_str, size_str = value.split()
                    frame_i = int(frame_str)
                    offset = int(offset_str)
                    size = int(size_str)
                except (ValueError, IndexError) as exc:
                    raise ValueError(f"Bad ESP32 chunk request: {value!r}") from exc
                if (frame_i != next_frame or not 0 <= frame_i < len(frames) or
                        offset != next_offset or not 0 < size <= 128):
                    raise ValueError(
                        f"Unexpected chunk request frame={frame_i} offset={offset} size={size}; "
                        f"expected frame={next_frame} offset={next_offset} (reflash BOTH files)"
                    )
                if offset + size > len(frames[frame_i]):
                    raise ValueError("ESP32 requested bytes beyond the end of a frame")
                chunk = frames[frame_i][offset:offset + size]
                written = port.write(chunk)
                if written != size:
                    raise OSError(f"Partial USB write: {written}/{size} bytes")
                port.flush()
                sent_chunks += 1
                if sent_chunks == 1:
                    print("  First chunk sent successfully (128 bytes)")
                next_offset += size
                if next_offset == len(frames[frame_i]):
                    sent_frames += 1
                    next_frame += 1
                    next_offset = 0
                    if sent_frames % 50 == 0 or sent_frames == len(frames):
                        print(f"  Uploaded {sent_frames}/{len(frames)} frames")
            elif value == "# NEXT":
                raise RuntimeError(
                    "ESP32 is running the OLD sketch (# NEXT). Upload the fixed "
                    "audio_features.ino before running this Python script."
                )
            else:
                print(value)
                if "# ERROR:" in value:
                    raise RuntimeError(f"ESP32 error during TEST: {value}")
        elif kind == "features":
            if sent_frames != len(frames) or value["frames"] != len(frames):
                raise ValueError("Mismatch in TEST frame count")
            base = unique_base(output, wav.stem)
            save_outputs(value, base)
            return
        elif kind == "wav":
            raise ValueError("Unexpected microphone audio while testing WAV")


def receive_live(port, output: Path, max_clips: int = 0) -> None:
    port.write(b"READY\n")
    port.flush()
    received = 0
    bases = {}
    print("Live VAD enabled: speak near the XIAO microphone. Ctrl+C to quit.")
    while True:
        event = read_event(port)
        if event is None:
            continue
        kind, value = event
        if kind == "text":
            print(value)
        elif kind == "wav":
            seq, rate, pcm = value
            base = unique_base(output, f"voice_{seq:04d}")
            bases[seq] = base
            save_wav(base.with_suffix(".wav"), rate, pcm)
            print(f"[WAV] {base.with_suffix('.wav')} ({len(pcm) / 2 / rate:.2f}s)")
        elif kind == "features":
            seq = value["sequence"]
            if seq not in bases:
                raise ValueError(f"Feature packet {seq} arrived without corresponding VAD1 audio")
            save_outputs(value, bases.pop(seq))
            received += 1
            if max_clips and received >= max_clips:
                return


def self_test() -> None:
    # Validates file parsing and plot output with a simulated ESP32 binary stream.
    # Also independently verifies theoretical FFT peaks from provided WAV files.
    import numpy as np
    for expected in (440, 1200):
        path = Path(__file__).resolve().parent.parent / "test_audio" / f"tone_{expected}hz_10s.wav"
        frames = load_wav_frames(path)
        assert len(frames) == 313, (expected, len(frames))
        # This host-side FFT exists ONLY in the offline self-test, never normal mode.
        segment = np.frombuffer(frames[100], dtype="<i2").astype(float)
        signal = (segment - segment.mean()) * np.hanning(FRAME_SAMPLES)
        spec = np.abs(np.fft.rfft(signal))
        detected = int(np.argmax(spec[1:129]) + 1) * SAMPLE_RATE / FRAME_SAMPLES
        assert abs(detected - expected) <= SAMPLE_RATE / FRAME_SAMPLES
        print(f"[SELF-TEST] {expected} Hz WAV -> expected FFT peak bin {detected} Hz")

    sine = [math.sin(2 * math.pi * 440 * i / SAMPLE_RATE) for i in range(FRAME_SAMPLES)]
    mock_spec = [float(i == 14) * 999.0 for i in range(SPECTRUM_BINS)]
    row = [float(i) for i in range(FEATURES)]
    blob = (b"# DEMO\n" + VAD_MAGIC + VAD_HEADER.pack(SAMPLE_RATE, FRAME_SAMPLES, 7)
            + b"".join(struct.pack("<h", round(v * 12000)) for v in sine)
            + FEATURE_MAGIC + FEATURE_HEADER.pack(SAMPLE_RATE, 7, 2, SPECTRUM_BINS, 437.5)
            + struct.pack("<" + "f" * SPECTRUM_BINS, *mock_spec)
            + struct.pack("<" + "f" * (FEATURES * 2), *(row + row)))
    port = io.BytesIO(blob)
    assert read_event(port) == ("text", "# DEMO")
    k1, wav = read_event(port)
    k2, features = read_event(port)
    assert k1 == "wav" and wav[0] == 7
    assert k2 == "features" and features["frames"] == 2
    with tempfile.TemporaryDirectory() as dirname:
        base = Path(dirname) / "mock"
        save_wav(base.with_suffix(".wav"), SAMPLE_RATE, wav[2])
        save_outputs(features, base)
        assert base.with_suffix(".wav").exists()
        assert Path(str(base) + "_features.png").exists()
        assert Path(str(base) + "_spectrum.png").exists()
        assert base.with_suffix(".csv").exists()
    print("[SELF-TEST] PASS: parser, WAV, FFT bins, and visualization")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Your board's USB port; find it with --list-ports")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--tone", type=Path, help="Send a 16kHz mono 16-bit WAV to ESP32")
    parser.add_argument("--listen", action="store_true", help="Receive live microphone VAD clips")
    parser.add_argument("--max-clips", type=int, default=0)
    parser.add_argument("--output", type=Path, default=Path("results"))
    parser.add_argument("--list-ports", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        self_test()
        return
    try:
        import serial  # type: ignore
        from serial.tools import list_ports  # type: ignore
    except ImportError:
        parser.error("Install pyserial, numpy, and matplotlib in the Python environment running this script.")

    if args.list_ports:
        ports = list(list_ports.comports())
        if not ports:
            print("No serial ports found. Connect the XIAO with a USB data cable.")
        for port in ports:
            print(f"{port.device}: {port.description}")
        return
    if not args.port or bool(args.tone) == bool(args.listen):
        parser.error("Specify --port and exactly one of --tone FILE or --listen")
    if args.max_clips < 0:
        parser.error("--max-clips must be nonnegative")
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    print(f"Saving results in {args.output}")
    try:
        print(f"Connecting to {args.port}; keep room quiet during 2-second VAD calibration...")
        with serial.Serial(args.port, args.baud, timeout=0.25, write_timeout=10) as port:
            time.sleep(3.6)  # Arduino may reset when USB port opens
            port.reset_input_buffer()
            try:
                if args.tone:
                    send_tone(port, args.tone, args.output)
                else:
                    receive_live(port, args.output, args.max_clips)
            finally:
                try:
                    port.write(b"STOP\n")
                    port.flush()
                except (OSError, serial.SerialException):
                    pass
    except KeyboardInterrupt:
        print("\nStopped by user")
    except (OSError, ValueError, RuntimeError, TimeoutError, serial.SerialException) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
