#!/usr/bin/env python3
"""Beta-only JSON process wrapper for the unchanged libraz/bpm-detector package.

This wrapper is not loaded by TuneRite's audio callback. A future reviewed bundle
places the unchanged ``bpm_detector`` package and its Python runtime beside this
script, invokes it on a private temporary WAV, and consumes the JSON file below.
"""

from __future__ import annotations

import argparse
import importlib.machinery
import importlib.util
import json
import math
import os
import sys
from pathlib import Path

import librosa

# bpm-detector's pinned source selects librosa.beat.tempo on current releases.
# Keep the reference source unchanged and bind that deprecated name to librosa's
# identical current function inside this local wrapper process only.
if not hasattr(librosa.beat, "tempo"):
    librosa.beat.tempo = librosa.feature.tempo

# Use only the local bundled package. No network access or host-DAW metadata is used.
BUNDLE_ROOT = Path(__file__).resolve().parent
PACKAGE_ROOT = BUNDLE_ROOT / "bpm_detector"
if not PACKAGE_ROOT.is_dir():
    raise RuntimeError("Bundled bpm_detector package is missing")
# Load the requested BPM/key modules without executing the package initializer,
# which imports CLI-only color/progress modules unrelated to the analyzer API.
package_spec = importlib.machinery.ModuleSpec("bpm_detector", loader=None, is_package=True)
package = importlib.util.module_from_spec(package_spec)
package.__path__ = [str(PACKAGE_ROOT)]
sys.modules["bpm_detector"] = package

from bpm_detector.music_analyzer import AudioAnalyzer  # noqa: E402

NOTE_TO_ROOT = {
    "C": 0, "C#": 1, "Db": 1, "D": 2, "Eb": 3, "D#": 3,
    "E": 4, "F": 5, "F#": 6, "Gb": 6, "G": 7, "Ab": 8,
    "G#": 8, "A": 9, "Bb": 10, "A#": 10, "B": 11,
}
MIN_CONFIDENCE = 0.15


def finite_number(value: object) -> float | None:
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    return number if math.isfinite(number) else None


def normalized_confidence(value: object) -> float:
    number = finite_number(value)
    if number is None:
        return 0.0
    return max(0.0, min(1.0, number / 100.0 if number > 1.0 else number))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    input_path = Path(args.input).resolve()
    output_path = Path(args.output).resolve()
    if not input_path.is_file():
        raise RuntimeError("Private input WAV does not exist")
    output_path.parent.mkdir(parents=True, exist_ok=True)

    analyzer = AudioAnalyzer(sr=22050, hop_length=128)
    analysis = analyzer.analyze_file(
        path=str(input_path),
        detect_key=True,
        comprehensive=False,
        min_bpm=40.0,
        max_bpm=300.0,
        start_bpm=150.0,
        analyze_rhythm=False,
        analyze_chords=False,
        analyze_structure=False,
        analyze_timbre=False,
        analyze_melody=False,
        analyze_dynamics=False,
    )
    basic = analysis.get("basic_info", analysis)

    bpm = finite_number(basic.get("bpm"))
    bpm_confidence = normalized_confidence(basic.get("bpm_confidence"))
    tempo_valid = bpm is not None and 40.0 <= bpm <= 300.0 and bpm_confidence >= MIN_CONFIDENCE
    raw_candidates = basic.get("bpm_candidates", [])
    total_votes = sum(max(0, int(votes)) for candidate, votes in raw_candidates if finite_number(candidate) is not None)
    tempo_candidates = [
        {"bpm": float(candidate), "score": max(0, int(votes)) / max(1, total_votes)}
        for candidate, votes in raw_candidates
        if finite_number(candidate) is not None and 40.0 <= float(candidate) <= 300.0 and int(votes) > 0
    ][:3]

    key_name = basic.get("key")
    key_confidence = normalized_confidence(basic.get("key_confidence"))
    parts = key_name.split() if isinstance(key_name, str) else []
    root = NOTE_TO_ROOT.get(parts[0]) if len(parts) == 2 else None
    mode = parts[1].lower() if len(parts) == 2 else ""
    key_valid = root is not None and mode in {"major", "minor"} and key_confidence >= MIN_CONFIDENCE

    payload = {
        "schema_version": 1,
        "tempo": {
            "valid": tempo_valid,
            "bpm": bpm if tempo_valid else None,
            "confidence": bpm_confidence,
            "candidates": tempo_candidates,
        },
        "key": {
            "valid": key_valid,
            "root": root if key_valid else None,
            "mode": mode if key_valid else "unknown",
            "confidence": key_confidence,
            "candidates": ([{"root": root, "mode": mode, "score": key_confidence}] if key_valid else []),
        },
    }
    temporary_output = output_path.with_suffix(output_path.suffix + ".tmp")
    temporary_output.write_text(json.dumps(payload, separators=(",", ":")), encoding="utf-8")
    os.replace(temporary_output, output_path)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"TuneRite reference analyzer error: {error}", file=sys.stderr)
        raise SystemExit(1)
