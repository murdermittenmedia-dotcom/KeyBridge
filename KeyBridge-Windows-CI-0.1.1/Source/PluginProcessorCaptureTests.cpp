#include "PluginProcessor.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 480;
    constexpr int captureBlocks = 650; // 6.5 seconds at 48 kHz.

    bool waitUntilIdle (KeyBridgeAudioProcessor& processor, int timeoutMilliseconds)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMilliseconds);
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (! processor.isAnalysisActive() && processor.getCaptureState() == 0)
                return true;
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }
        return false;
    }

    void fillBeatBlock (juce::AudioBuffer<float>& buffer, int blockIndex)
    {
        buffer.clear();
        const auto startSample = blockIndex * blockSize;
        const auto samplesPerBeat = static_cast<int> (sampleRate * 60.0 / 120.0);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto absolute = startSample + sample;
            const auto inBeat = absolute % samplesPerBeat;
            const auto click = inBeat < 160 ? 0.82f * (1.0f - static_cast<float> (inBeat) / 160.0f) : 0.0f;
            const auto tone = 0.10f * std::sin (2.0 * juce::MathConstants<double>::pi * 261.6256 * absolute / sampleRate);
            buffer.setSample (0, sample, click + static_cast<float> (tone));
            buffer.setSample (1, sample, click + static_cast<float> (tone));
        }
    }

    void fillVocalBlock (juce::AudioBuffer<float>& buffer, int blockIndex)
    {
        const auto startSample = blockIndex * blockSize;
        for (int sample = 0; sample < blockSize; ++sample)
        {
            const auto absolute = startSample + sample;
            const auto value = static_cast<float> (0.28 * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * absolute / sampleRate));
            buffer.setSample (0, sample, value);
            buffer.setSample (1, sample, value);
        }
    }

    bool expect (bool condition, const char* message)
    {
        if (! condition)
            std::cerr << "FAIL: " << message << '\n';
        return condition;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    KeyBridgeAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, blockSize);

    processor.setAnalysisMode (0);
    processor.startFreshAnalysis();
    for (int block = 0; block < captureBlocks; ++block)
    {
        fillBeatBlock (buffer, block);
        processor.processBlock (buffer, midi);
    }

    auto ok = true;
    ok &= expect (processor.getAudioCallbackCount() >= static_cast<std::uint64_t> (captureBlocks), "Beat pass must receive host-style audio callbacks");
    ok &= expect (processor.getAnalysisDuration() >= 6.0f, "Beat pass must retain at least six seconds of captured audio");
    ok &= expect (processor.getCapturedSignalSeconds() >= 6.0f, "Beat pass must register source signal duration");
    ok &= expect (processor.getCaptureState() == 2, "Beat pass must remain capturable before explicit finish");
    processor.finishCapture();
    fillBeatBlock (buffer, captureBlocks);
    processor.processBlock (buffer, midi);
    ok &= expect (waitUntilIdle (processor, 8000), "Beat Finish Capture must queue and complete worker analysis");

    processor.setAnalysisMode (1);
    processor.startFreshAnalysis();
    for (int block = 0; block < captureBlocks; ++block)
    {
        fillVocalBlock (buffer, block);
        processor.processBlock (buffer, midi);
    }

    ok &= expect (processor.getAnalysisDuration() >= 6.0f, "Vocal pass must retain at least six seconds of captured audio");
    ok &= expect (processor.getCapturedSignalSeconds() >= 6.0f, "Vocal pass must register source signal duration");
    ok &= expect (processor.getCaptureState() == 2, "Vocal pass must remain capturable before explicit finish");
    processor.finishCapture();
    fillVocalBlock (buffer, captureBlocks);
    processor.processBlock (buffer, midi);
    ok &= expect (waitUntilIdle (processor, 8000), "Vocal Finish Capture must queue and complete worker analysis");
    ok &= expect (processor.getVocalConfidence() > 0.55f, "Vocal pass must publish a voiced pitch result");

    std::cout << "capture_callbacks=" << processor.getAudioCallbackCount()
              << " vocal_confidence=" << processor.getVocalConfidence()
              << " vocal_average_midi=" << processor.getVocalAverageMidi()
              << "\n";
    return ok ? 0 : 1;
}
