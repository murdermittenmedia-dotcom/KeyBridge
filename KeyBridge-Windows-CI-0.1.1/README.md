# KeyBridge Prototype 0.1.0

This is the first test build of KeyBridge, a transparent music-analysis plug-in with an initial Vocal Fit interface.

## Included in this prototype

The build includes a VST3 effect and a standalone target. It passes audio through, reads host BPM when the host supplies it, performs a lightweight spectral key estimate, displays confidence, provides Vocal Fit profile/genre/vibe controls, shows a starting-note recommendation, and provides note buttons for the reference-tone workflow.

The prototype is intentionally an engineering test build. The reference-tone buttons now generate short sine tones through the plug-in output. Vocal Fit currently ranks from the detected key plus the selected creative profile; vocal-bus pitch extraction and chord-aware recommendations are not yet implemented.

## Test targets

The included Linux artifact is useful for Linux VST3 hosts, but FL Studio on Windows requires a Windows VST3 build. JUCE’s current source explicitly rejects MinGW, so the sandbox cannot produce a supported Windows binary with its Linux toolchain. The package therefore includes a Windows build script for Visual Studio 2022.

On Windows, install Visual Studio 2022 with **Desktop development with C++**, install CMake, and install Git. Open the extracted folder in File Explorer and double-click `build_windows.bat`, or run it from a Visual Studio Developer Command Prompt. The script creates a 64-bit Release VST3 at:

```text
build-windows\\KeyBridge_artefacts\\Release\\VST3\\KeyBridge.vst3
```

Copy the complete `KeyBridge.vst3` folder to:

```text
C:\\Program Files\\Common Files\\VST3\\
```

Then restart or rescan plug-ins in FL Studio. The source is also portable to an AAX build, but Pro Tools requires the AAX SDK and signing workflow separately.

To build on a development machine:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
```

The standalone executable is under `build/KeyBridge_artefacts/Release/Standalone/KeyBridge`. The VST3 bundle is under `build/KeyBridge_artefacts/Release/VST3/KeyBridge.vst3`.

## First test checklist

Insert KeyBridge on a stereo beat or master bus and confirm that the audio is unchanged. Start playback and confirm that Host BPM appears when the DAW exposes tempo. Watch the key and confidence fields update. Test ANALYZE and HOLD. Change the profile, genre, and vibe selections and verify that the Vocal Fit text updates. Click note buttons to test the current reference-note interaction.

Please treat the key result as a prototype estimate, especially for drum-heavy, highly chromatic, or modulating material. The next engineering pass should move analysis off the real-time thread, add stable rolling-window decisions, implement actual reference-tone playback, and add a labeled audio-derived BPM detector.
