cmake_minimum_required(VERSION 3.22)
project(libsonare_msvc_analysis_probe LANGUAGES CXX)

if (NOT DEFINED LIBSONARE_SOURCE_DIR)
    message(FATAL_ERROR "LIBSONARE_SOURCE_DIR must point to the pinned libsonare checkout")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(libsonare_analysis_probe
    main.cpp
    ${LIBSONARE_SOURCE_DIR}/src/analysis/bpm_analyzer.cpp
    ${LIBSONARE_SOURCE_DIR}/src/analysis/key_analyzer.cpp
    ${LIBSONARE_SOURCE_DIR}/src/analysis/key_profiles.cpp
)

target_include_directories(libsonare_analysis_probe PRIVATE
    ${LIBSONARE_SOURCE_DIR}/src
    ${LIBSONARE_SOURCE_DIR}/include
    ${LIBSONARE_SOURCE_DIR}/third_party/kissfft
    ${LIBSONARE_SOURCE_DIR}/third_party/dr_libs
    ${LIBSONARE_SOURCE_DIR}/third_party/minimp3
    ${LIBSONARE_SOURCE_DIR}/third_party/r8brain
)

target_compile_definitions(libsonare_analysis_probe PRIVATE
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

if (MSVC)
    target_compile_options(libsonare_analysis_probe PRIVATE /W4 /WX /wd4244 /wd4267 /wd4305 /wd4324 /external:W0)
endif()
