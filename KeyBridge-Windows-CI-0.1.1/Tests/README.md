# TuneRite Analysis Test Data Protocol

TuneRite’s test media is deliberately **local only**. Do not commit commercial beats, vocal takes, unreleased sessions, licensed loops, or any audio whose redistribution rights are unclear. The committed Python generator creates only deterministic synthetic WAVs, which are appropriate for regression testing mechanics but are not representative of production music.

## Ground Truth Manifest

Use `ground_truth_template.csv` as a starting point for a local manifest. Each row references a file outside the repository and records the intended task, reference labels, and rights status. The semicolon-delimited `accepted_bpms` field allows deliberate half-time/double-time equivalence to be labeled rather than silently scored as wrong. The expected-key candidates field can represent an honest relative-major/minor ambiguity where the source material does not establish a clear mode.

| Split | Purpose | Permitted use |
|---|---|---|
| `development` | Bring up decoding, report formatting, and baseline algorithms. | Initial implementation and debugging. |
| `tuning` | Select thresholds and resolve known algorithmic faults. | Parameter selection only. |
| `locked` | Estimate performance after algorithm choices are frozen. | Final measurement only; do not tune against it. |

A final accuracy claim must report the number of **locked** fixtures, scoring tolerances, the complete failures, abstentions/uncertain outcomes, and whether tempo alternatives or key ambiguities were considered acceptable. Synthetic fixture results must be labeled as synthetic regression outcomes, never as production-music accuracy.

## Current Committed Synthetic Coverage

The CI-generated WAV fixtures cover exact repetitive onsets at 120 BPM with C–E–G harmonic content and a steady 220 Hz A3 reference tone. Direct shared-core regression coverage additionally checks 101, 120, and 200 BPM pulse trains; A3 and C4 reference tones; C-major/E-minor mode ambiguity; and silence rejection. This coverage verifies deterministic behavior and guards specific repaired bugs, but it does **not** validate beat/key accuracy on diverse commercial or real-world recordings.

## Local Scorecard Runner

`evaluate_local_manifest.py` executes the compiled `TuneriteOfflineAnalyzer` against the local paths listed in a manifest, writes one JSON report per fixture, and creates a consolidated JSON scorecard. It does not copy local audio into the repository. The runner scores BPM only when `tempo_valid` is true and scores key only when `key_valid` is true; invalid results are counted separately as **abstentions**, never coerced into a key or BPM.

```text
python Tests/evaluate_local_manifest.py \
  --manifest /absolute/path/to/authorized_manifest.csv \
  --analyzer /absolute/path/to/TuneriteOfflineAnalyzer.exe \
  --split development \
  --output /absolute/path/to/development_scorecard.json
```

Use `--split tuning` only to select thresholds or behavior after development. The runner refuses `--split locked` unless `--allow-locked` is supplied explicitly. Run locked evaluation only after the implementation and thresholds are frozen, and report every skipped row, abstention, accepted tempo equivalence, and failure alongside the aggregate result.

The manifest may include an optional `bpm_tolerance` column. If absent, the scorecard uses an absolute BPM tolerance of `0.25`.

> The scorecard is an **evaluation tool**, not a training-data downloader. It does not make a real-world accuracy claim until it has been run on an authorized locked fixture set with results retained for review.
