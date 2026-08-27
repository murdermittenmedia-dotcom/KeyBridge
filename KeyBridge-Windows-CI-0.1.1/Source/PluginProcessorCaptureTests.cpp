#include "PluginProcessor.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
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

    bool fillFixtureBlock (juce::AudioBuffer<float>& destination, const juce::AudioBuffer<float>& fixture, int startSample)
    {
        destination.clear();
        if (startSample >= fixture.getNumSamples()) return false;
        const auto available = fixture.getNumSamples() - startSample;
        const auto toCopy = juce::jmin (destination.getNumSamples(), available);
        for (int channel = 0; channel < destination.getNumChannels(); ++channel)
        {
            const auto sourceChannel = juce::jmin (channel, fixture.getNumChannels() - 1);
            destination.copyFrom (channel, 0, fixture, sourceChannel, startSample, toCopy);
        }
        return toCopy > 0;
    }

    bool bufferUnchanged (const juce::AudioBuffer<float>& before, const juce::AudioBuffer<float>& after)
    {
        if (before.getNumChannels() != after.getNumChannels() || before.getNumSamples() != after.getNumSamples()) return false;
        for (int channel = 0; channel < before.getNumChannels(); ++channel)
            if (std::memcmp (before.getReadPointer (channel), after.getReadPointer (channel),
                             static_cast<size_t> (before.getNumSamples()) * sizeof (float)) != 0)
                return false;
        return true;
    }

    bool loadFixture (const juce::String& path, juce::AudioBuffer<float>& destination, double& fixtureSampleRate)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (path)));
        if (reader == nullptr || reader->numChannels < 1 || reader->lengthInSamples <= 0) return false;
        fixtureSampleRate = reader->sampleRate;
        destination.setSize (static_cast<int> (reader->numChannels), static_cast<int> (reader->lengthInSamples));
        return reader->read (&destination, 0, destination.getNumSamples(), 0, true, true);
    }

    bool expect (bool condition, const char* message)
    {
        if (! condition)
            std::cerr << "FAIL: " << message << '\n';
        return condition;
    }
}

int main (int argc, char* argv[])
{
    juce::String fixturePath;
    for (int index = 1; index < argc; ++index)
        if (juce::String (argv[index]) == "--beat-file" && index + 1 < argc)
            fixturePath = argv[++index];

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    KeyBridgeAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buffer (2, blockSize);

    // Existing deterministic processor-level workflow checks.
    processor.setAnalysisMode (0);
    processor.startFreshAnalysis();
    for (int block = 0; block < captureBlocks; ++block)
    {
        fillBeatBlock (buffer, block);
        processor.processBlock (buffer, midi);
    }

    auto ok = true;
    ok &= expect (processor.getAudioCallbackCount() > 0, "Beat pass must receive host-style audio callbacks");
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

    if (fixturePath.isEmpty())
        return ok ? 0 : 1;

    // Optional local-only fixture replay: stream actual decoded audio through processBlock and its worker.
    juce::AudioBuffer<float> fixture;
    double fixtureRate = 0.0;
    ok &= expect (loadFixture (fixturePath, fixture, fixtureRate), "Optional beat fixture must decode");
    ok &= expect (fixtureRate >= 8000.0 && fixtureRate <= 192000.0, "Optional beat fixture must have a supported sample rate");
    if (! ok) return 1;

    processor.prepareToPlay (fixtureRate, blockSize);
    processor.setAnalysisMode (0);
    processor.startFreshAnalysis();
    juce::AudioBuffer<float> fixtureBlock (2, blockSize);
    const auto maximumCaptureSamples = static_cast<int> (std::ceil (fixtureRate * 16.0));
    const auto samplesToStream = juce::jmin (fixture.getNumSamples(), maximumCaptureSamples);
    for (int start = 0; start < samplesToStream; start += blockSize)
    {
        if (! fillFixtureBlock (fixtureBlock, fixture, start)) break;
        const auto before = fixtureBlock;
        processor.processBlock (fixtureBlock, midi);
        ok &= expect (bufferUnchanged (before, fixtureBlock), "Transparent processor path must not alter fixture audio");
    }
    if (processor.getCaptureState() == 2)
    {
        processor.finishCapture();
        fillFixtureBlock (fixtureBlock, fixture, juce::jmax (0, samplesToStream - blockSize));
        processor.processBlock (fixtureBlock, midi);
    }
    ok &= expect (waitUntilIdle (processor, 30000), "Fixture capture must complete background analysis");
    ok &= expect (processor.getCapturedSignalSeconds() >= 6.0f, "Fixture replay must present at least six seconds of source signal");

    const auto fixtureResult = processor.getLastPublishedBeatAnalysisForDiagnostics();
    std::cout << "fixture_capture_seconds=" << processor.getAnalysisDuration()
              << " fixture_callbacks=" << processor.getAudioCallbackCount()
              << " fixture_generation=" << processor.getLastPublishedBeatGenerationForDiagnostics()
              << " fixture_tempo_valid=" << fixtureResult.tempoValid
              << " fixture_detected_bpm=" << fixtureResult.bpm
              << " fixture_alternative_bpm=" << fixtureResult.alternativeBpm
              << " fixture_half_time_bpm=" << fixtureResult.halfTimeBpm
              << " fixture_double_time_bpm=" << fixtureResult.doubleTimeBpm
              << " fixture_bpm_confidence=" << fixtureResult.bpmConfidence
              << " fixture_key_valid=" << fixtureResult.keyValid
              << " fixture_key_root=" << fixtureResult.keyRoot
              << " fixture_key_mode=" << fixtureResult.keyMode
              << " fixture_key_confidence=" << fixtureResult.keyConfidence
              << " fixture_clipping=" << fixtureResult.clippingDetected
              << " fixture_input_quality=" << fixtureResult.inputQuality
              << " fixture_warning=\"" << fixtureResult.warning << "\"\n";
    for (size_t index = 0; index < fixtureResult.tempoCandidates.size(); ++index)
        std::cout << "fixture_tempo_candidate_" << index << "=" << fixtureResult.tempoCandidates[index].bpm
                  << " score=" << fixtureResult.tempoCandidates[index].score << "\n";
    for (size_t index = 0; index < fixtureResult.keyCandidates.size(); ++index)
        std::cout << "fixture_key_candidate_" << index << "=" << fixtureResult.keyCandidates[index] << "\n";
    return ok ? 0 : 1;
}
