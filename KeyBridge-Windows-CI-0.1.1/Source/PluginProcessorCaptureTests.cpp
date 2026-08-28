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
    constexpr int captureBlocks = 1250; // 12.5 seconds at 48 kHz: two overlapping eight-second tempo windows.

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
    ok &= expect (processor.getAnalysisDuration() >= 12.0f, "Beat pass must retain the 12 seconds required for tempo consensus");
    ok &= expect (processor.getCapturedSignalSeconds() >= 12.0f, "Beat pass must register the required source signal duration");
    ok &= expect (processor.getCaptureState() == 2, "Beat pass must remain capturable before explicit finish");
    // Finish must queue the fully written capture even if transport stops immediately and there
    // is no subsequent processBlock callback.
    processor.finishCapture();
    ok &= expect (waitUntilIdle (processor, 65000), "Beat Finish Capture must queue and complete local worker analysis without another callback");
    ok &= expect (processor.hasSavedBeatTempo(), "Valid Beat capture must automatically save BPM");
    ok &= expect (processor.hasSavedBeatResult(), "Valid Beat capture must publish a saved beat outcome");
    ok &= expect (processor.getSavedBeatBpm() > 0.0, "Saved Beat BPM must contain a measured value");

    processor.setAnalysisMode (1);
    processor.startFreshAnalysis();
    for (int block = 0; block < captureBlocks; ++block)
    {
        fillVocalBlock (buffer, block);
        processor.processBlock (buffer, midi);
    }

    ok &= expect (processor.getAnalysisDuration() >= 12.0f, "Vocal pass must retain the reliable capture duration");
    ok &= expect (processor.getCapturedSignalSeconds() >= 12.0f, "Vocal pass must register the required source signal duration");
    ok &= expect (processor.getCaptureState() == 2, "Vocal pass must remain capturable before explicit finish");
    processor.finishCapture();
    ok &= expect (waitUntilIdle (processor, 8000), "Vocal Finish Capture must queue and complete worker analysis without another callback");
    ok &= expect (processor.getVocalConfidence() > 0.55f, "Vocal pass must publish a voiced pitch result");
    ok &= expect (processor.hasSavedVocalResult(), "Valid Vocal capture must automatically save its result");
    ok &= expect (processor.getSavedVocalGeneration() > 0, "Saved Vocal result must retain its capture generation");

    // Saved analysis evidence must survive a state round-trip for a reopened DAW project.
    juce::MemoryBlock savedState;
    processor.getStateInformation (savedState);
    KeyBridgeAudioProcessor restored;
    restored.prepareToPlay (sampleRate, blockSize);
    restored.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize()));
    ok &= expect (restored.hasSavedBeatTempo(), "Saved Beat BPM must survive state reload");
    ok &= expect (restored.hasSavedVocalResult(), "Saved Vocal result must survive state reload");
    ok &= expect (std::abs (restored.getSavedBeatBpm() - processor.getSavedBeatBpm()) < 0.001,
                  "Reloaded Beat BPM must match the saved snapshot");
    ok &= expect (std::abs (restored.getSavedVocalAverageMidi() - processor.getSavedVocalAverageMidi()) < 0.001f,
                  "Reloaded Vocal average MIDI must match the saved snapshot");

    std::cout << "capture_callbacks=" << processor.getAudioCallbackCount()
              << " saved_beat_tempo=" << processor.hasSavedBeatTempo()
              << " saved_beat_key=" << processor.hasSavedBeatKey()
              << " saved_vocal=" << processor.hasSavedVocalResult()
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
        processor.finishCapture();
    ok &= expect (waitUntilIdle (processor, 30000), "Fixture capture must complete background analysis without another callback");
    ok &= expect (processor.getCapturedSignalSeconds() >= 12.0f, "Fixture replay must present the reliable source signal duration");

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
