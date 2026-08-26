#!/usr/bin/env python3
"""Score local, authorized TuneRite audio fixtures without committing media.

The script invokes the compiled shared OfflineAnalyzer and produces a JSON
scorecard. It deliberately refuses the locked split unless --allow-locked is
provided, so routine parameter tuning cannot silently consume held-out data.
"""

from __future__ import annotations

import argparse
import csv
import json
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass
class Counters:
    rows: int = 0
    skipped: int = 0
    bpm_assessed: int = 0
    bpm_correct: int = 0
    bpm_abstained: int = 0
    key_assessed: int = 0
    key_correct: int = 0
    key_abstained: int = 0

    def as_dict(self) -> dict[str, Any]:
        return {
            "rows": self.rows,
            "skipped": self.skipped,
            "bpm_assessed": self.bpm_assessed,
            "bpm_correct": self.bpm_correct,
            "bpm_accuracy": self.bpm_correct / self.bpm_assessed if self.bpm_assessed else None,
            "bpm_abstained": self.bpm_abstained,
            "key_assessed": self.key_assessed,
            "key_correct": self.key_correct,
            "key_accuracy": self.key_correct / self.key_assessed if self.key_assessed else None,
            "key_abstained": self.key_abstained,
        }


def parse_number_list(value: str) -> list[float]:
    return [float(token.strip()) for token in value.split(";") if token.strip()]


def expected_mode(value: str) -> int:
    normalized = value.strip().lower()
    if normalized == "major":
        return 0
    if normalized == "minor":
        return 1
    raise ValueError(f"Expected key mode must be major or minor, got {value!r}")


def analyze(analyzer: Path, input_path: Path, mode: str, report_path: Path) -> dict[str, Any]:
    report_path.parent.mkdir(parents=True, exist_ok=True)
    completed = subprocess.run(
        [str(analyzer), "--input", str(input_path), "--mode", mode, "--output", str(report_path)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(f"OfflineAnalyzer failed for {input_path}:\n{completed.stdout}")
    if not report_path.is_file():
        raise RuntimeError(f"OfflineAnalyzer returned success but report is missing: {report_path}")
    return json.loads(report_path.read_text(encoding="utf-8"))


def score_beat(row: dict[str, str], report: dict[str, Any], counter: Counters) -> dict[str, Any]:
    result: dict[str, Any] = {
        "detected_bpm": report.get("detected_bpm"),
        "tempo_valid": bool(report.get("tempo_valid")),
        "key_root": report.get("key_root"),
        "key_mode": report.get("key_mode"),
        "key_valid": bool(report.get("key_valid")),
    }
    accepted_bpms = parse_number_list(row.get("accepted_bpms", "") or row.get("true_bpm", ""))
    tolerance = float(row.get("bpm_tolerance", "0.25") or "0.25")
    if accepted_bpms:
        counter.bpm_assessed += 1
        if not result["tempo_valid"]:
            counter.bpm_abstained += 1
            result["bpm_pass"] = False
        else:
            result["bpm_pass"] = any(abs(float(result["detected_bpm"]) - accepted) <= tolerance for accepted in accepted_bpms)
            counter.bpm_correct += int(result["bpm_pass"])
        result["accepted_bpms"] = accepted_bpms
        result["bpm_tolerance"] = tolerance

    if row.get("true_key_root", "").strip() and row.get("true_key_mode", "").strip():
        counter.key_assessed += 1
        if not result["key_valid"]:
            counter.key_abstained += 1
            result["key_pass"] = False
        else:
            expected_root = int(row["true_key_root"])
            expected_key_mode = expected_mode(row["true_key_mode"])
            result["key_pass"] = int(result["key_root"]) == expected_root and int(result["key_mode"]) == expected_key_mode
            counter.key_correct += int(result["key_pass"])
        result["expected_key_root"] = int(row["true_key_root"])
        result["expected_key_mode"] = row["true_key_mode"]
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description="Evaluate local TuneRite ground-truth rows using OfflineAnalyzer.")
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--analyzer", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--split", choices=("development", "tuning", "locked"), required=True)
    parser.add_argument("--allow-locked", action="store_true", help="Required for the locked split; do not use while tuning parameters.")
    args = parser.parse_args()

    if args.split == "locked" and not args.allow_locked:
        raise SystemExit("Refusing locked evaluation without --allow-locked. Keep held-out rows out of tuning.")
    if not args.manifest.is_file():
        raise SystemExit(f"Manifest does not exist: {args.manifest}")
    if not args.analyzer.is_file():
        raise SystemExit(f"OfflineAnalyzer does not exist: {args.analyzer}")

    counter = Counters()
    fixture_results: list[dict[str, Any]] = []
    manifest_dir = args.manifest.parent
    reports_dir = args.output.parent / "offline-fixture-reports" / args.split

    with args.manifest.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            if row.get("split", "").strip() != args.split:
                continue
            counter.rows += 1
            item: dict[str, Any] = {"id": row.get("id", ""), "split": args.split, "mode": row.get("mode", "")}
            if row.get("rights_confirmed", "").strip().lower() != "yes":
                item.update({"status": "skipped", "reason": "rights_confirmed must be yes"})
                counter.skipped += 1
                fixture_results.append(item)
                continue
            input_path = (manifest_dir / row.get("relative_path", "")).resolve()
            if not input_path.is_file():
                item.update({"status": "skipped", "reason": f"missing local file: {input_path}"})
                counter.skipped += 1
                fixture_results.append(item)
                continue
            mode = row.get("mode", "").strip().lower()
            if mode not in {"beat", "vocal"}:
                item.update({"status": "skipped", "reason": f"unsupported mode: {mode}"})
                counter.skipped += 1
                fixture_results.append(item)
                continue
            report = analyze(args.analyzer.resolve(), input_path, mode, reports_dir / f"{row.get('id', 'fixture')}.json")
            item["status"] = "analyzed"
            item["report"] = report
            if mode == "beat":
                item["score"] = score_beat(row, report, counter)
            fixture_results.append(item)

    payload = {
        "split": args.split,
        "manifest": str(args.manifest.resolve()),
        "analyzer": str(args.analyzer.resolve()),
        "summary": counter.as_dict(),
        "fixtures": fixture_results,
        "notice": "Local manifest results are only valid for the selected split. Do not tune against locked rows.",
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(payload["summary"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
