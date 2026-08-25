# TuneRite analysis architecture research notes

## Tempo-estimation evaluation

Schreiber, Urbano, and Müller’s overview of music tempo estimation explains that global tempo estimation is an MIR task for recordings with stable tempo and highlights the importance of evaluation data, ground truth, and application-relevant metrics. It distinguishes a primary tempo estimate from metrical alternatives and warns against treating half/double-time errors as silently interchangeable for user-facing applications. TuneRite will therefore return a primary BPM, alternative candidates, an explicit half/double-time ambiguity state, and confidence that is not based solely on the best-versus-second score margin.

Source: https://transactions.ismir.net/articles/10.5334/tismir.43

## Real-time plugin threading

JUCE community discussion describes a common plugin pattern: use a lock-free FIFO or ring buffer to move audio from the real-time thread to an auxiliary worker, retain lightweight atomics for UI status, and avoid running long FFT/history processing on the audio callback. TuneRite will use preallocated capture storage on processBlock and move beat/key/vocal analysis to a background worker. The GUI reads published result snapshots rather than calling heavy algorithms.

Source: https://forum.juce.com/t/lock-free-queues-and-visualization-of-data/20659

## Design consequence

The current short, synchronous per-buffer analysis is not a suitable foundation for trustworthy beat key/BPM or vocal F0 results. The next implementation phase is a shared `AnalysisCore` used by a plugin capture worker and an offline WAV harness. It must produce diagnostics, uncertainty states, and reproducible measurements before UI claims are expanded.

Saved 2026-08-25.
