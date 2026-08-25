#include "AnalysisCore.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    constexpr double sampleRate = 44100.0;
    constexpr double pi = 3.14159265358979323846;

    std::vector<float> makeBeat (double bpm, double seconds)
    {
        const auto count = static_cast<size_t> (std::round (seconds * sampleRate));
        std::vector<float> output (count, 0.0f);
        const auto interval = static_cast<int> (std::round ((60.0 / bpm) * sampleRate));
        for (size_t i = 0; i < count; ++i)
        {
            const auto t = static_cast<double> (i) / sampleRate;
            const auto chord = 0.12 * std::sin (2.0 * pi * 130.8128 * t)
                             + 0.12 * std::sin (2.0 * pi * 164.8138 * t)
                             + 0.12 * std::sin (2.0 * pi * 195.9977 * t);
            output[i] = static_cast<float> (chord);
        }
        for (int start = 0; start < static_cast<int> (count); start += interval)
            for (int n = 0; n < 900 && start + n < static_cast<int> (count); ++n)
                output[static_cast<size_t> (start + n)] += static_cast<float> (0.85 * std::exp (-static_cast<double> (n) / 130.0));
        return output;
    }

    std::vector<float> makeReferenceTone (double hz, double seconds)
    {
        const auto count = static_cast<size_t> (std::round (seconds * sampleRate));
        std::vector<float> output (count, 0.0f);
        for (size_t i = 0; i < count; ++i)
        {
            const auto t = static_cast<double> (i) / sampleRate;
            output[i] = static_cast<float> (0.32 * std::sin (2.0 * pi * hz * t));
        }
        return output;
    }

    bool closeTo (double actual, double expected, double tolerance)
    {
        return std::abs (actual - expected) <= tolerance;
    }

    bool checkBeat (double expectedBpm)
    {
        const auto result = tunerite::AnalysisCore::analyzeBeat (makeBeat (expectedBpm, 16.0), sampleRate);
        std::cout << "Synthetic beat expected=" << expectedBpm << " detected=" << result.bpm
                  << " alternative=" << result.alternativeBpm << " bpmConfidence=" << result.bpmConfidence
                  << " keyRoot=" << result.keyRoot << " mode=" << result.keyMode
                  << " keyConfidence=" << result.keyConfidence << "\n";
        if (! result.usableAudio || ! closeTo (result.bpm, expectedBpm, 2.0))
        {
            std::cerr << "FAIL: expected " << expectedBpm << " BPM within 2 BPM.\n";
            return false;
        }
        const bool expectedRelativePair = (result.keyRoot == 0 && result.keyMode == 0)
                                       || (result.keyRoot == 4 && result.keyMode == 1);
        if (! expectedRelativePair || ! result.keyUncertain)
        {
            std::cerr << "FAIL: C-E-G material must be reported as the C-Major/E-Minor harmonic family with explicit mode uncertainty.\n";
            return false;
        }
        return true;
    }

    bool checkVocal (double hz, double expectedMidi, const std::string& name)
    {
        const auto result = tunerite::AnalysisCore::analyzeVocal (makeReferenceTone (hz, 6.0), sampleRate);
        std::cout << "Synthetic vocal " << name << " expectedMidi=" << expectedMidi << " detected=" << result.averageMidi
                  << " confidence=" << result.confidence << " voiced=" << result.voicedPercent << "\n";
        if (! result.usableAudio || ! closeTo (result.averageMidi, expectedMidi, 0.5))
        {
            std::cerr << "FAIL: " << name << " was not tracked within 0.5 MIDI note.\n";
            return false;
        }
        return true;
    }
}

int main()
{
    bool passed = true;
    passed = checkBeat (101.0) && passed;
    passed = checkBeat (120.0) && passed;
    passed = checkBeat (200.0) && passed;
    passed = checkVocal (220.0, 57.0, "A3") && passed;
    passed = checkVocal (261.625565, 60.0, "C4") && passed;

    const std::vector<float> silence (static_cast<size_t> (sampleRate * 4.0), 0.0f);
    const auto silenceResult = tunerite::AnalysisCore::analyzeBeat (silence, sampleRate);
    std::cout << "Synthetic silence usable=" << silenceResult.usableAudio << " warning=" << silenceResult.warning << "\n";
    if (silenceResult.usableAudio)
    {
        std::cerr << "FAIL: silence must not be reported as usable beat audio.\n";
        passed = false;
    }

    return passed ? 0 : 1;
}
