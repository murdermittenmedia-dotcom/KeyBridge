# TuneRite BPM and Key Detection Audit

## Scope

This audit covers the shared `AnalysisCore`, the VST3 capture worker, the offline WAV analyzer, and the UI-facing result gates. It records the starting point for the audio-only BPM/key rebuild. It is not an accuracy validation report.

| Requirement | Current finding | Required follow-up |
|---|---|---|
| Detected BPM must not use host BPM | `processBlock` stores host BPM only in `hostBpm`. `AnalysisCore::analyzeBeat` has no host-BPM parameter and no access to host state. | Preserve this separation and add regression evidence that audio-only input determines tempo. |
| Project BPM must remain secondary | The editor labels host BPM `Project BPM`; displayed detection fields source `detectedBpm` or saved audio results. | Preserve. Do not feed host BPM into candidate generation or tie-breaking. |
| Shared core for WAV and VST3 | The offline analyzer and worker both call `AnalysisCore::analyzeBeat`. | Preserve and add result-format parity checks. |
| Heavy work outside `processBlock` | The audio thread meters and fills preallocated capture buffers. The worker copies a completed capture and calls `AnalysisCore`. | Preserve. Improve capture mono policy without allocations or locks. |
| Filename/metadata independence | The input filename only selects the default output report filename. WAV sample data and sample rate are passed to `AnalysisCore`; no filename or metadata is used for BPM/key. | Preserve and test explicit report values. |
| Hardcoded key fallback | `BeatAnalysisResult` initializes root/mode to C/major numerically, but `keyUncertain` defaults true and the UI displays `UNCERTAIN` unless a valid saved key exists. | Replace ambiguous numeric defaults with explicit key-valid state and expand tests to reject percussion-only inputs. |
| Tempo path | Current core derives a single onset envelope from sample-difference energy, then autocorrelates it over the entire capture. It returns top candidates, but does not yet produce multi-window stability/phase diagnostics. | Implement multi-window, multi-band onset aggregation and explicit ambiguity/stability metrics. |
| Tonal path | Current core uses targeted Goertzel pitch energies with one profile family and 440 Hz note centers. It uses multiple frames, but does not yet estimate tuning, suppress transients, test multiple profile families, or report harmonic validity separately. | Implement tuned harmonic chroma, harmonic-content gates, candidate scores, and confidence diagnostics. |
| Capture mono conversion | VST3 capture uses `(left + right) * 0.5`, which can attenuate or cancel anti-phase stereo material. | Use an audio-only phase-robust mono capture policy. |
| Validity gate | Saving beat results currently requires a combined stable BPM/key flag. | Preserve strict gating while allowing BPM-valid/key-unknown reporting without an invented key. |

## Explicit non-goals during this rebuild

The project will not expand the UI or Auto-Tune recommendation logic while BPM/key accuracy and evidence are being rebuilt. No accuracy claim is valid until the documented synthetic and local ground-truth fixture results are produced.

## Verified prohibited paths at audit time

No source path found where `hostBpm`, filenames, file metadata, genre, vibe, or user profile is passed into `AnalysisCore::analyzeBeat` or used to rank BPM/key candidates. No FFT, key detection, tempo detection, I/O, logging, or allocation was found in `processBlock` beyond the existing preallocated capture write and lightweight metering. The current worker makes a temporary copy off the audio thread before calling the shared core.

## Known current limitations

The existing detector is a foundation, not an accuracy-proven BPM/key engine. Its C-E-G fixture correctly demonstrates relative-major/minor ambiguity but cannot establish C major as a unique key. Existing 101/120/200 BPM and A3 synthetic checks prove only narrow generated signal cases. They are not real-world music accuracy metrics.
