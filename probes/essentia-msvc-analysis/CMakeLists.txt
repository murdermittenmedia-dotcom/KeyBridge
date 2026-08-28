cmake_minimum_required(VERSION 3.22)
project(essentia_msvc_analysis_probe LANGUAGES CXX)

if (NOT DEFINED ESSENTIA_SOURCE_DIR)
    message(FATAL_ERROR "ESSENTIA_SOURCE_DIR must point to the pinned Essentia checkout")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(essentia_bpm_key_probe STATIC
    ${ESSENTIA_SOURCE_DIR}/src/algorithms/rhythm/rhythmextractor2013.cpp
    ${ESSENTIA_SOURCE_DIR}/src/algorithms/extractor/keyextractor.cpp
)

target_include_directories(essentia_bpm_key_probe PRIVATE
    ${ESSENTIA_SOURCE_DIR}/src
    ${ESSENTIA_SOURCE_DIR}/src/essentia
    ${ESSENTIA_SOURCE_DIR}/src/essentia/streaming
    ${ESSENTIA_SOURCE_DIR}/src/essentia/streaming/algorithms
    ${ESSENTIA_SOURCE_DIR}/src/essentia/scheduler
    ${ESSENTIA_SOURCE_DIR}/src/essentia/utils
)

target_compile_definitions(essentia_bpm_key_probe PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)

if (MSVC)
    target_compile_options(essentia_bpm_key_probe PRIVATE /W4)
endif()
