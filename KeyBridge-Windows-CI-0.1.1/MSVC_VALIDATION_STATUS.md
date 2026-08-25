# KeyBridge MSVC Validation Status

The earlier Windows VST3 was built with MinGW and was not accepted by FL Studio. Its PE imports included MinGW runtime DLLs, and its build configuration was not the requested MSVC x64 Release configuration.

This environment has no Microsoft Visual C++ compiler, Visual Studio linker, Windows SDK installation, Wine, or Windows copy of Steinberg VST3PluginTestHost. Therefore an MSVC binary cannot honestly be compiled or run here.

The project now contains `CMakeLists-msvc.txt` and `build_windows_msvc.bat`. On Windows with Visual Studio 2022, the script uses an MSVC-only VST3 target and requests the static `/MT` runtime. It restores the original CMake file after the build, including on failure.

The requested final validation remains to be run on Windows:

1. Build the x64 Release VST3 with `build_windows_msvc.bat`.
2. Load the bundle in Steinberg `VST3PluginTestHost`.
3. Verify factory creation, `IPluginFactory::getClassInfo`, component creation, `initialize`, `setBusArrangements`, `activate`, editor creation, editor attachment, and clean shutdown.
4. Load it in FL Studio and confirm scan, insertion, editor opening, audio pass-through, and state save/restore.

No MSVC binary is included in the accompanying source package because none was produced in this environment.
