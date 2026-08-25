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
            const auto vibrato = 0.003 * std::sin (2.0 * pi * 5.5 * t);
            const auto phase = 2.0 * pi * hz * t * (1.0 + vibrato);
            output[i] = static_cast<float> (0.32 * std::sin (phase) + 0.08 * std::sin (phase * 2.0));
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
    if (beat.keyRoot != 0 || beat.keyMode != 0)
    {
        std::cerr << "FAIL: C-major synthetic chord was not identified as C Major.\n";
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
