# TuneRite analysis-core design

## Scope

TuneRite will analyze the normal mixer input in one selected pass at a time. **Beat Only** captures a beat while the vocal is muted. **Vocal Only** captures a lead vocal while the beat is muted. The plugin preserves the audio buffer exactly; all nontrivial analysis happens outside `processBlock`.

## Components

| Component | Responsibility | Real-time thread? |
|---|---|---|
| `AudioCapture` | Preallocated single-producer/single-consumer capture buffer, lightweight RMS/peak updates, capture state | Producer only |
| `AnalysisWorker` | Reads a finished capture snapshot and invokes `AnalysisCore` | No |
| `AnalysisCore` | Shared preprocessing and reusable beat/vocal entry points | No |
| `TempoAnalyzer` | Onset envelope, tempo candidates, half/double-time candidates, window agreement | No |
| `TonalAnalyzer` | Windowed tonal frames, chroma aggregation, 24 key hypotheses, alternatives, uncertainty | No |
| `VocalF0Analyzer` | Voicing-gated autocorrelation/YIN-style F0 candidates and octave rejection | No |
| `VocalFeatureAnalyzer` | Range statistics, sustains, note changes, pitch stability, vibrato/slide candidates | No |
| `RecommendationEngine` | Explainable starting recommendations from saved beat and saved vocal results | No |
| `AnalysisReportWriter` | JSON/text diagnostics for the offline harness and future export | No |

## Capture state machine

```text
Idle
  -> CapturingBeat -> BeatCaptureComplete -> BeatResultSaved
  -> CapturingVocal -> VocalCaptureComplete -> VocalResultSaved
BeatResultSaved + VocalResultSaved -> RecommendationReady
Any state -> AnalysisError
```

Capture begins from the explicit Analyze command. A default minimum duration will be 16 seconds, with a user stop action planned after the first worker integration. A shorter recording remains possible but is reported as low-context rather than receiving a falsely confident result.

## Threading constraints

`processBlock` will only copy mono analysis samples into preallocated storage while a capture is active, update simple RMS/peak atomics, and leave the host audio buffer unchanged. It must not allocate, run an FFT, lock, perform file I/O, or call the GUI. The worker receives a completed snapshot and publishes results through atomic snapshot fields.

## Reliability rules

A key is displayed as **Uncertain** when its confidence is below the threshold or when the runner-up/relative major-minor candidate is too close. BPM output contains a primary and alternative metrical candidate; it does not silently classify a double-time result as the only answer. Recommendations are unavailable until both passes have saved results and must be capped by the weaker relevant analysis confidence.

## Shared test path

The same `AnalysisCore` library will be compiled into the VST3 and a Windows offline command-line analyzer. Synthetic WAV tests test known tempo, chords, pitch, slides, vibrato, and sustained-note math. User-provided audio belongs only in a private ground-truth manifest and is never committed to the public repository.
