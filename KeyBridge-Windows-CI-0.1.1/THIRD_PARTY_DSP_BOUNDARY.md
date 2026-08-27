# Third-Party DSP Boundary — TuneRite

## Purpose

TuneRite is a commercial JUCE VST3. Its production implementation remains in the shared C++ `Source/AnalysisCore.cpp` and is compiled only with JUCE DSP and the project’s own source. This document records the licensing boundary for external beat/key-analysis projects inspected during development.

## Inspected repositories and boundary

| Repository | Inspected revision | Repository license observed | Permitted TuneRite use | Prohibited TuneRite use |
|---|---:|---|---|---|
| [Essentia](https://github.com/MTG/essentia) | `b9fa6cb674ca43dfb94d28d293aeda441c6745db` | AGPLv3 (`COPYING.txt`) | High-level design review and independent offline experimentation | Copying, adapting, linking, embedding, or distributing Essentia source/binaries in the proprietary VST3 without a separately cleared commercial license |
| [BTrack](https://github.com/adamstark/BTrack) | `9d6127618a5679e9caa74c594b88f1d74f0e035f` | GPLv3 (`LICENSE.txt`) | High-level design review and independent offline experimentation | Copying, adapting, linking, embedding, or distributing BTrack source/binaries in the proprietary VST3 without a separately cleared license |
| [libKeyFinder](https://github.com/mixxxdj/libkeyfinder) | `941e517ebf853c2153a8b9d6efcc0c729199aa0b` | GPLv3 (`LICENSE`) | High-level design review and independent offline experimentation | Copying, adapting, linking, embedding, or distributing libKeyFinder source/binaries in the proprietary VST3 without a separately cleared license |
| [librosa](https://github.com/librosa/librosa) | `f808bac0812469049fd8167c87f45813b7d57b32` | ISC (`LICENSE.md`) | Local-only offline comparison oracle | Using Python, librosa, or any external service in VST processing or worker analysis |

## Clean-room design requirements

TuneRite implements original code and uses no imported identifiers, source fragments, headers, build targets, binary dependencies, or upstream test vectors from the GPL/AGPL repositories. The production architecture uses these independently implementable DSP ideas only:

| Area | Clean-room TuneRite behavior |
|---|---|
| Input quality | Replace non-finite source samples with zero only in a private analysis copy. If source `maxAbs > 0.99`, multiply that copy by `0.99 / maxAbs`. The audio buffer passed through the VST is not changed. Clipping is reported as `clippingDetected`, `clippingAmount`, `analysisBufferScale`, and `inputQuality`; it is never a hard tempo/key rejection. |
| Tempo | Use a JUCE STFT with log-magnitude, multi-band positive spectral flux plus time-domain attack evidence. Aggregate windowed onset autocorrelation/period candidates, phase support, and direct transient timing. Report primary BPM, alternate candidate, and half/double interpretations. |
| Tonal evidence | Use non-transient, non-noise-like STFT frames, finite/normalized pitch-class evidence, frequency-weighted soft chroma, robust trimmed aggregation, and multiple major/minor profile families. Persist the top three key candidates and signal profile-family disagreement separately from clipping. |
| Validity | Do not use host/project BPM, filename, tags, labels, genre, or a fixed key/note fallback. Emit unknown or low confidence only when audio evidence is insufficient or internally ambiguous—not merely because source samples exceeded unity. |
| Runtime isolation | `processBlock` remains limited to transparent pass-through metering/copying. Shared-core analysis runs in the existing worker and uses no Python, external process, web service, or upstream library. |

## Verification protocol

The deterministic suite includes regular tempo/key fixtures, ambiguity/percussion/silence rejection, detuned tonal fixtures, and a clipped/non-finite 120 BPM fixture. The clipped fixture must remain usable, log a scale below one, replace the injected non-finite sample, and detect 120 BPM within ±0.25 BPM. Authorized real audio is evaluated only through a local manifest and is not committed.

Any proposal to directly reuse Essentia, BTrack, or libKeyFinder source, binaries, model data, or test assets must be reviewed and licensed before it enters source control or the released installer.
