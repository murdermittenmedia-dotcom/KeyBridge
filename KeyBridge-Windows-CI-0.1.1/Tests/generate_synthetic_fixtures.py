#!/usr/bin/env python3
"""Create deterministic, local-only WAV fixtures for TuneRite development tests.

The fixtures are synthetic and contain no copyrighted music or vocals. Their
known ground truth is deliberately simple: 120 BPM C-E-G clicks and 220 Hz A3.
"""

from __future__ import annotations

import argparse
import math
import struct
import wave
from pathlib import Path

SAMPLE_RATE = 44_100
PI = math.pi


def write_pcm16(path: Path, samples: list[float]) -> None:
    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(SAMPLE_RATE)
        frames = bytearray()
        for sample in samples:
            value = max(-1.0, min(1.0, sample))
            frames.extend(struct.pack("<h", int(round(value * 32767.0))))
        handle.writeframes(frames)


def beat_fixture(bpm: float, seconds: float = 16.0) -> list[float]:
    count = round(seconds * SAMPLE_RATE)
    output: list[float] = [0.0] * count
    interval = round((60.0 / bpm) * SAMPLE_RATE)
    for index in range(count):
        time = index / SAMPLE_RATE
        output[index] = (
            0.12 * math.sin(2.0 * PI * 130.8128 * time)
            + 0.12 * math.sin(2.0 * PI * 164.8138 * time)
            + 0.12 * math.sin(2.0 * PI * 195.9977 * time)
        )
    for start in range(0, count, interval):
        for sample_index in range(900):
            position = start + sample_index
            if position >= count:
                break
            output[position] += 0.85 * math.exp(-sample_index / 130.0)
    return output


def reference_tone(hz: float, seconds: float = 6.0) -> list[float]:
    count = round(seconds * SAMPLE_RATE)
    return [0.32 * math.sin(2.0 * PI * hz * index / SAMPLE_RATE) for index in range(count)]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    write_pcm16(args.output / "c_eg_click_120bpm.wav", beat_fixture(120.0))
    write_pcm16(args.output / "a3_220hz.wav", reference_tone(220.0))


if __name__ == "__main__":
    main()
