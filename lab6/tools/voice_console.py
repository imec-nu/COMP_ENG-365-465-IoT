#!/usr/bin/env python3
"""Receive the board's enrollment prompts and save voice-command results to CSV.

Run from the lab6 folder:
    python tools/voice_console.py --list-ports
    python tools/voice_console.py --port YOUR_PORT

The board records three WAKE examples, then three SLEEP examples. Wait for
its next PROMPT before speaking. Recognition starts after all six examples.
The console logs results; the board does the audio processing and matching.
"""

from __future__ import annotations
import argparse
import csv
from datetime import datetime
from pathlib import Path
import re
import sys
import time

RESULT_COLUMNS = [
    "timestamp", "seq", "predicted", "candidate", "score", "runner",
    "threshold", "action", "led", "processing_ms",
]
RESULT_PATTERN = re.compile(r"^# RESULT\s+(.*)$")
TEMPLATE_PATTERN = re.compile(r"^# TEMPLATE\s+label=(WAKE|SLEEP)\s+saved=(\d)/3\s+frames=(\d+)$")


def parse_kv(line: str) -> dict[str, str]:
    """Parse key=value tokens without assuming any fixed column order."""
    return dict(re.findall(r"\b([a-z_]+)=([^\s]+)", line))


def parse_result(line: str) -> dict[str, str] | None:
    if not RESULT_PATTERN.match(line):
        return None
    values = parse_kv(line)
    missing = [k for k in RESULT_COLUMNS if k != "timestamp" and k not in values]
    if missing:
        raise ValueError(f"Malformed # RESULT; missing {missing}: {line!r}")
    if values["predicted"] not in ("WAKE", "SLEEP", "UNKNOWN"):
        raise ValueError(f"Bad predicted label: {values['predicted']}")
    for key in ("score", "runner", "threshold"):
        float(values[key])
    for key in ("seq", "processing_ms"):
        int(values[key])
    values["timestamp"] = datetime.now().isoformat(timespec="seconds")
    return {k: values[k] for k in RESULT_COLUMNS}


def list_ports() -> None:
    try:
        from serial.tools import list_ports as serial_ports
    except ImportError as exc:
        raise RuntimeError("Install pyserial: python -m pip install pyserial") from exc
    ports = serial_ports.comports()
    if not ports:
        print("No serial ports found. Connect the XIAO with a USB data cable and try again.")
    for port in ports:
        print(f"{port.device}: {port.description}")


def run(port_name: str, baud: int, output: Path, max_results: int) -> None:
    try:
        import serial
    except ImportError as exc:
        raise RuntimeError("Install pyserial: python -m pip install pyserial") from exc

    output.parent.mkdir(parents=True, exist_ok=True)
    target = output
    if target.exists():
        stem, suffix = target.stem, target.suffix
        number = 1
        while target.exists():
            target = output.with_name(f"{stem}_{number:02d}{suffix}")
            number += 1
    print(f"Results will be saved to: {target.resolve()}")
    print(f"Opening {port_name} at {baud} baud; remain QUIET during initial 2-second calibration.")
    with serial.Serial(port_name, baud, timeout=0.25, write_timeout=5) as connection:
        # Some ESP32 USB-CDC configurations reset when the COM port opens.
        time.sleep(3.8)
        connection.reset_input_buffer()
        connection.write(b"ENROLL\n")
        connection.flush()
        print("Enrollment started. Speak WAKE 3 times, then SLEEP 3 times.")
        print("After EACH utterance, pause for silence and the next # PROMPT message.")
        print("When you see '# READY mode=RECOGNIZE', test commands and unrelated words.")
        print("Press Ctrl+C to stop. Close Arduino Serial Monitor first.\n")
        count = 0
        pending = bytearray()  # USB reads may split a status line across transfers.
        with target.open("w", newline="", encoding="utf-8") as file:
            writer = csv.DictWriter(file, fieldnames=RESULT_COLUMNS)
            writer.writeheader()
            finished = False
            while not finished:
                piece = connection.read(max(1, min(512, connection.in_waiting)))
                if not piece:
                    continue
                pending.extend(piece)
                if len(pending) > 4096:
                    raise RuntimeError("Serial line exceeded 4096 bytes; check board/port")
                while b"\n" in pending:
                    raw, _, rest = pending.partition(b"\n")
                    pending = bytearray(rest)
                    line = raw.decode("utf-8", errors="replace").strip()
                    if not line:
                        continue
                    print(line, flush=True)
                    record = parse_result(line)
                    if record is None:
                        continue
                    writer.writerow(record)
                    file.flush()
                    count += 1
                    if max_results and count >= max_results:
                        print(f"Saved {count} results; reached --max-results={max_results}")
                        finished = True
                        break
    print(f"CSV saved: {target}")


def self_test() -> None:
    sample = ("# RESULT seq=17 predicted=WAKE candidate=WAKE score=0.141 "
              "runner=0.450 threshold=0.300 action=LED_ON led=ON processing_ms=83")
    v = parse_result(sample)
    assert v and v["seq"] == "17" and v["predicted"] == "WAKE"
    assert v["action"] == "LED_ON" and float(v["score"]) == 0.141
    assert parse_result("# PROMPT label=WAKE sample=1/3") is None
    assert parse_result("# SCORES seq=1 wake=0.1,0.2,0.3 sleep=0.7,0.8,0.9") is None
    reject = ("# RESULT seq=18 predicted=UNKNOWN candidate=SLEEP score=0.80 "
              "runner=0.90 threshold=0.21 action=UNKNOWN led=ON processing_ms=95")
    assert parse_result(reject)["predicted"] == "UNKNOWN"
    print("PASS: structured result parsing, unknown rejection, CSV column contract")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", help="XIAO USB serial port shown by --list-ports")
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--output", type=Path, default=Path("results/task3_recognition.csv"))
    p.add_argument("--max-results", type=int, default=0,
                   help="Exit after this many recognition attempts (0 = unlimited)")
    p.add_argument("--list-ports", action="store_true")
    p.add_argument("--self-test", action="store_true")
    args = p.parse_args()
    if args.max_results < 0:
        p.error("--max-results must be >= 0")
    try:
        if args.self_test:
            self_test()
        elif args.list_ports:
            list_ports()
        elif args.port:
            run(args.port, args.baud, args.output, args.max_results)
        else:
            p.error("Use --list-ports to find your XIAO, then run with --port YOUR_PORT")
    except KeyboardInterrupt:
        print("\nStopped. Completed results are saved in the CSV file.")
    except (RuntimeError, ValueError, OSError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
