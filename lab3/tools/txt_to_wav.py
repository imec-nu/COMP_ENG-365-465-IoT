#!/usr/bin/env python3
"""Convert Lab 3 raw TXT samples to mono 16 kHz WAV files.

Example: python3 tools/txt_to_wav.py recordings --join
Outputs go into the input folder's wav subfolder unless --output-dir is supplied.
"""
import argparse
from array import array
from pathlib import Path
import re
import sys
import wave

SAMPLE_RATE = 16000
EXPECTED_SAMPLES = SAMPLE_RATE * 5


def natural_key(path):
    return [int(part) if part.isdigit() else part.lower()
            for part in re.split(r"(\d+)", path.name)]


def read_samples(path):
    samples = array("h")
    with path.open("r", encoding="ascii") as source:
        for line_number, line in enumerate(source, 1):
            value_text = line.strip()
            if not re.fullmatch(r"[+-]?\d+", value_text):
                raise ValueError(f"{path}:{line_number}: expected one integer per line")
            value = int(value_text)
            if not -32768 <= value <= 32767:
                raise ValueError(f"{path}:{line_number}: sample is outside signed 16-bit range")
            samples.append(value)
    if samples.itemsize != 2:
        raise RuntimeError("This Python platform does not have 16-bit array('h') values")
    return samples


def write_wav(path, pcm):
    # Exclusive creation protects a previous conversion from being overwritten.
    with path.open("xb") as output:
        with wave.open(output, "wb") as wav:
            wav.setnchannels(1)
            wav.setsampwidth(2)
            wav.setframerate(SAMPLE_RATE)
            wav.writeframes(pcm)


def join_wavs(paths, output_path):
    total = 0
    with output_path.open("xb") as output:
        with wave.open(output, "wb") as joined:
            joined.setnchannels(1)
            joined.setsampwidth(2)
            joined.setframerate(SAMPLE_RATE)
            for path in paths:
                with wave.open(str(path), "rb") as source:
                    while True:
                        data = source.readframes(4096)
                        if not data:
                            break
                        joined.writeframesraw(data)
                        total += len(data) // 2
    return total


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="a TXT file or a folder of TXT files")
    parser.add_argument("--output-dir", type=Path, help="output folder (default: input folder/wav)")
    parser.add_argument("--join", action="store_true", help="also write joined.wav in filename order")
    args = parser.parse_args()

    if args.input.is_dir():
        sources = sorted(args.input.glob("*.txt"), key=natural_key)
        default_output = args.input / "wav"
    elif args.input.is_file() and args.input.suffix.lower() == ".txt":
        sources = [args.input]
        default_output = args.input.parent / "wav"
    else:
        parser.error("input must be an existing TXT file or directory")
    if not sources:
        parser.error("no TXT files were found")

    output_dir = args.output_dir or default_output
    output_dir.mkdir(parents=True, exist_ok=True)
    destinations = [output_dir / (path.stem + ".wav") for path in sources]
    planned = destinations + ([output_dir / "joined.wav"] if args.join else [])
    if len(set(planned)) != len(planned):
        parser.error("output names conflict; rename a source file before converting")
    for path in planned:
        if path.exists():
            parser.error(f"{path} already exists; choose a new --output-dir")

    for source, destination in zip(sources, destinations):
        samples = read_samples(source)
        count = len(samples)
        if count != EXPECTED_SAMPLES:
            print(f"WARNING: {source.name} has {count} samples, expected {EXPECTED_SAMPLES}; "
                  "all samples are retained.", file=sys.stderr)
        if sys.byteorder != "little":
            samples.byteswap()
        write_wav(destination, samples.tobytes())
        print(f"{source.name} -> {destination.name}: {count} samples, "
              f"{count / SAMPLE_RATE:.6f} seconds")

    if args.join:
        total = join_wavs(destinations, output_dir / "joined.wav")
        print(f"joined.wav: {total} samples in filename order, "
              f"{total / SAMPLE_RATE:.6f} seconds (including any partial files)")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, UnicodeError, RuntimeError, wave.Error) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
