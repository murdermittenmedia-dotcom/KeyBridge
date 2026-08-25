#include "AnalysisCore.h"

#include <cmath>
#include <iostream>
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

    std::vector<float> makeVocalLikeTone (double hz, double seconds)
    {
        const auto count = static_cast<size_t> (std::round (seconds * sampleRate));
        std::vector<float> output (count, 0.0f);
        for (size_t i = 0; i < count; ++i)
        {
            const auto t = static_cast<double> (i) / sampleRate;
            const auto phase = 2.0 * pi * hz * t;
            output[i] = static_cast<float> (0.32 * std::sin (phase));
        }
        return output;
    }

    bool closeTo (double actual, double expected, double tolerance)
    {
        return std::abs (actual - expected) <= tolerance;
    }
}

int main()
{
    bool passed = true;

    const auto beat = tunerite::AnalysisCore::analyzeBeat (makeBeat (120.0, 16.0), sampleRate);
    std::cout << "Synthetic beat: BPM=" << beat.bpm << " keyRoot=" << beat.keyRoot << " mode=" << beat.keyMode
              << " bpmConfidence=" << beat.bpmConfidence << " keyConfidence=" << beat.keyConfidence << "\n";
    if (! beat.usableAudio || ! closeTo (beat.bpm, 120.0, 2.0))
    {
        std::cerr << "FAIL: 120 BPM synthetic beat was not estimated within 2 BPM.\n";
        passed = false;
    }
    const bool expectedRelativePair = (beat.keyRoot == 0 && beat.keyMode == 0)
                                   || (beat.keyRoot == 4 && beat.keyMode == 1);
    if (! expectedRelativePair || ! beat.keyUncertain)
    {
        std::cerr << "FAIL: C-E-G material must be reported as the C-Major/E-Minor harmonic family with explicit mode uncertainty.\n";
        passed = false;
    }

    const auto vocal = tunerite::AnalysisCore::analyzeVocal (makeVocalLikeTone (220.0, 6.0), sampleRate);
    std::cout << "Synthetic vocal: averageMidi=" << vocal.averageMidi << " confidence=" << vocal.confidence
              << " voiced=" << vocal.voicedPercent << "\n";
    if (! vocal.usableAudio || ! closeTo (vocal.averageMidi, 57.0, 0.8))
    {
        std::cerr << "FAIL: A3 synthetic vocal was not tracked near MIDI 57.\n";
        passed = false;
    }

    return passed ? 0 : 1;
}
