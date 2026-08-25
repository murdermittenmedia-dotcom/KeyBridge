# TuneRite by MurderMittenMedia

TuneRite is a transparent VST3 analysis effect for producers, vocalists, and engineers. It passes the main audio input through unchanged and provides guidance from separately captured beat and vocal performances. TuneRite does not retune audio, change Auto-Tune controls, or use the DAW project tempo as the detected audio BPM.

## Reliable single-input workflow

For the most reliable FL Studio workflow, place TuneRite on one mixer insert that can receive the beat and vocal, then analyze them in separate user-controlled passes. Do not analyze the combined beat-and-vocal signal as if it were a vocal performance.

### Vocal-only pass

Select **Vocal Only**. Mute the beat, leave the isolated vocal playing, and press **ANALYZE**. TuneRite captures the current input and estimates the lowest and highest reliable notes, comfortable range, average note, pitch confidence, sustained-note percentage, note-change speed, voiced percentage, and melodic-versus-spoken delivery. Press **SAVE VOCAL RESULT** when the pass is complete.

### Beat-only pass

Select **Beat Only**. Mute the vocal, leave the isolated beat playing, and press **ANALYZE**. TuneRite captures the current input and estimates audio-derived BPM, key, scale, energy, and confidence. Press **SAVE BEAT RESULT** when the pass is complete.

### Combined recommendation

Select **Combined Recommendation** only after both passes have been saved. TuneRite combines the saved beat key/scale/BPM result with the saved vocal range and performance metrics, plus the selected profile, genre, delivery, and vibe, to produce Auto-Tune-style recommendations. If either saved result is missing, TuneRite displays:

> Analyze and save the vocal result and beat result separately first.

Changing the genre, delivery, vibe, or vocalist profile can change recommendations, but it cannot change the measured beat key or detected audio BPM.

## Analysis states and uncertainty

TuneRite uses visible states such as **NOT ANALYZED**, **ANALYZING**, **SAVED**, **NOT READY**, **READY**, **Key Uncertain**, and **BPM Uncertain**. It must not present a fallback key, BPM, or confidence value as though it came from the audio. Ambiguous material may show alternative BPM or key candidates instead of a definite answer.

The **Project BPM** value is host metadata shown separately. **Detected Audio BPM** is derived from the captured audio input and is not copied from FL Studio’s tempo field. TuneRite’s standard VST3 interface cannot directly write the FL Studio project tempo; the copy action transfers the detected value for manual entry.

## Inputs and transparency

The primary workflow uses one normal stereo audio input. The main input is analyzed only during the selected pass. TuneRite also retains an optional Vocal Input sidechain for host configurations that expose it, but the single-input mute-and-analyze workflow is the recommended FL Studio path when sidechain delivery is unreliable.

The main audio passes through TuneRite unchanged. Temporary beat and vocal analysis buffers are kept separate. Vocal analysis is never performed by mixing a saved vocal buffer with a saved beat buffer, and vocal analysis does not feed the audible output.

## FL Studio test setup

Create a mixer insert for TuneRite Analysis. Route or send the beat and vocal sources to that insert as needed, but mute one source before each analysis pass. For the Vocal Only pass, mute the beat, select **Vocal Only**, press **ANALYZE**, then press **SAVE VOCAL RESULT**. For the Beat Only pass, mute the vocal, select **Beat Only**, press **ANALYZE**, then press **SAVE BEAT RESULT**. Finally select **Combined Recommendation** and confirm that the recommendation becomes ready.

Verify that clearing the vocal result does not erase the beat result, clearing the beat result does not erase the vocal result, and **RESET ALL** clears both. Verify that HOLD and LOCK prevent unwanted changes, that audio passes through unchanged, and that a low-confidence source produces uncertainty rather than a fabricated definite result.

## Windows installation

Use the supplied Windows x64 MSVC Release installer, or copy the complete VST3 bundle into:

```text
C:\Program Files\Common Files\VST3\
```

The final bundle path must be:

```text
C:\Program Files\Common Files\VST3\TuneRite.vst3\Contents\x86_64-win\TuneRite.vst3
```

After copying or installing, restart FL Studio and rescan plugins. The Windows build uses the static MSVC runtime (`/MT`) and the CI workflow validates the VST3 manifest as strict JSON before packaging.

## Development and testing

The repository contains the Windows GitHub Actions workflow and a preserved source history for the earlier KeyBridge/TuneRite iterations. Do not commit copyrighted beats or vocals. Use local WAV files and a ground-truth CSV for offline testing, recording the filename, true BPM, alternate BPM, true key, mode, genre, delivery, vibe, and ambiguity notes. Report detected candidates, confidence, analysis duration, and failed cases rather than claiming universal accuracy.

## Remaining limitations

TuneRite is a real-time mixer plugin and cannot reproduce every aspect of an offline file analyzer such as TuneBat. Key and BPM results are estimates and should be checked against verified reference material. Auto-Tune settings are recommendations only. Advanced vocal measurements such as precise vibrato speed, slide/bend segmentation, and chord movement require further validated DSP and should not be interpreted as fully accurate until measured against labeled test files.
