# TuneRite Local Analyzer Beta Contract

## Scope

This is a **non-commercial beta architecture**. It does not replace TuneRite's native `AnalysisCore`, alter pass-through audio, use host tempo, or call any network service. The adapter is run only after the existing worker owns a finalized private capture buffer.

## Data flow

```text
finalized worker-owned mono capture + sample rate + generation
  -> private unique temporary WAV
  -> local bundled TuneRiteReferenceAnalyzer executable
  -> private unique JSON output
  -> strict JSON schema/validity check
  -> immutable BeatAnalysisResult snapshot
  -> existing generation guard
  -> existing independent BPM/key save publication
  -> delete temporary WAV, JSON, and process diagnostics
```

The adapter must never run on `processBlock`. The existing capture callback remains limited to pass-through, meters, and preallocated copying. A stale generation is discarded by the current `publishBeatResult` generation check after the child process returns.

## Analyzer process contract

The installer-approved analyzer binary will be located in the per-user TuneRite application-data directory. An environment-path override exists only for development tests.

Arguments:

```text
TuneRiteReferenceAnalyzer --input <private-temp-wav> --output <private-temp-json>
```

The reference `bpm-detector` code remains unchanged inside the bundled analyzer. A small wrapper owns WAV loading, calls its documented `AudioAnalyzer` API at 22,050 Hz/hop 128/min BPM 40/max BPM 300, and writes the output JSON.

## Required JSON schema

```json
{
  "schema_version": 1,
  "tempo": {
    "valid": true,
    "bpm": 137.5,
    "confidence": 0.72,
    "candidates": [{"bpm": 137.5, "score": 0.62}]
  },
  "key": {
    "valid": true,
    "root": 7,
    "mode": "major",
    "confidence": 0.67,
    "candidates": [{"root": 7, "mode": "major", "score": 0.64}]
  }
}
```

`tempo.valid` and `key.valid` are independent. A false validity value may retain candidates for diagnostics but must not populate a saved answer. A missing, malformed, non-finite, out-of-range, or inconsistent field is a process-result error and publishes no answer. The adapter must never substitute C major, C minor, a host tempo, or a numerical BPM multiplier.

## Failure behavior

An absent executable, failed launch, timeout, non-zero exit, missing JSON, invalid JSON, or invalid schema publishes a worker result with `usableAudio=false`, `tempoValid=false`, `keyValid=false`, and a concise visible error warning. The existing saved BPM/key values are not overwritten by that failed generation. Cleanup always removes the temporary WAV and JSON.

## Packaging status

No installer or commercial release is authorized under this contract. A future package needs a reviewed, self-contained Windows analyzer runtime plus its exact dependencies and license notices. This repository intentionally contains only the integration contract and test shim until that approval exists.
