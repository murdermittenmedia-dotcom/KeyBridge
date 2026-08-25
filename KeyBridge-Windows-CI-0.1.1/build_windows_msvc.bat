@echo off
setlocal EnableExtensions
cd /d "%~dp0"
if not exist build-msvc rmdir /s /q build-msvc
copy /Y CMakeLists.txt CMakeLists.before-msvc.txt >nul
copy /Y CMakeLists-msvc.txt CMakeLists.txt >nul
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
if errorlevel 1 set BUILD_STATUS=1
if defined BUILD_STATUS goto :restore
cmake --build build-msvc --config Release --target Tunerite_VST3 --parallel
if errorlevel 1 set BUILD_STATUS=1
:restore
copy /Y CMakeLists.before-msvc.txt CMakeLists.txt >nul
if defined BUILD_STATUS (
  echo MSVC build failed.
  exit /b 1
)
echo.
echo MSVC x64 Release VST3 build complete.
echo Output: build-msvc\Tunerite_artefacts\Release\VST3\Tunerite.vst3
pause
