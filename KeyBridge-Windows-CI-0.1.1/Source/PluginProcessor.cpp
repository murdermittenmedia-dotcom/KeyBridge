#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr std::array<const char*, 12> noteNames { { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" } };
    constexpr std::array<float, 12> majorProfile { { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f } };
    constexpr std::array<float, 12> minorProfile { { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f } };
}

KeyBridgeAudioProcessor::KeyBridgeAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      fft (std::make_unique<juce::dsp::FFT> (fftOrder))
{
    fftData.allocate (fftSize * 2, true);
    std::fill (fftData.get(), fftData.get() + fftSize * 2, 0.0f);
}

void KeyBridgeAudioProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = juce::jmax (newSampleRate, 8000.0);
    fftData.allocate (fftSize * 2, true);
    fftFill = 0;
    analysisFrameCount = 0;
    previousEnergy = 0.0f;
    samplesSinceOnset = 0;
    adaptiveEnergy = 0.0001f;
    chroma.fill (0.0f);
    energyHistory.fill (0.0f);
    energyHistoryWrite = 0;
    energyHistoryCount = 0;
    captureSamples = 0;
    oneShotMode = false;
    captureProgress.store (0.0f, std::memory_order_relaxed);
    resetRequested.store (false, std::memory_order_relaxed);
    oneShotRequested.store (false, std::memory_order_relaxed);
    candidateKey = 0;
    candidateMode = 0;
    candidateWins = 0;
    stableKey = 0;
    stableMode = 0;
    hasStableKey = false;
    hasStableDetectionFlag.store (false, std::memory_order_relaxed);
    detectedKey.store (0, std::memory_order_relaxed);
    detectedMode.store (0, std::memory_order_relaxed);
    detectedBpm.store (0.0, std::memory_order_relaxed);
    keyConfidence.store (0.0f, std::memory_order_relaxed);
    bpmConfidence.store (0.0f, std::memory_order_relaxed);
    analysisFrames.store (0, std::memory_order_relaxed);
    inputLevel.store (0.0f, std::memory_order_relaxed);
    std::fill (fftData.get(), fftData.get() + fftSize * 2, 0.0f);
}

void KeyBridgeAudioProcessor::releaseResources()
{
    fftFill = 0;
}

bool KeyBridgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getChannelSet (true, 0);
    const auto output = layouts.getChannelSet (false, 0);
    const auto inputMatchesOutput = input.isDisabled() || input == output;
    const auto outputIsSupported = output == juce::AudioChannelSet::mono()
                                || output == juce::AudioChannelSet::stereo();
    return inputMatchesOutput && outputIsSupported;
}

void KeyBridgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (position->getBpm().hasValue())
                hostBpm.store (juce::jlimit (1.0, 999.0, *position->getBpm()), std::memory_order_relaxed);

    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0 || sampleRate <= 0.0 || fft == nullptr)
        return;
    if (!analysisEnabled.load (std::memory_order_relaxed))
        return;

    if (resetRequested.exchange (false, std::memory_order_acq_rel))
    {
        oneShotMode = oneShotRequested.exchange (false, std::memory_order_acq_rel);
        captureSamples = 0;
        captureProgress.store (0.0f, std::memory_order_relaxed);
        fftFill = 0;
        analysisFrameCount = 0;
        previousEnergy = 0.0f;
        samplesSinceOnset = 0;
        adaptiveEnergy = 0.0001f;
        chroma.fill (0.0f);
        energyHistory.fill (0.0f);
        energyHistoryWrite = 0;
        energyHistoryCount = 0;
        candidateWins = 0;
        hasStableKey = false;
        hasStableDetectionFlag.store (false, std::memory_order_relaxed);
        detectedBpm.store (0.0, std::memory_order_relaxed);
        keyConfidence.store (0.0f, std::memory_order_relaxed);
        bpmConfidence.store (0.0f, std::memory_order_relaxed);
    }

    const auto* left = buffer.getReadPointer (0);
    const auto* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : left;
    float blockPeak = 0.0f;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto sample = 0.5f * (left[i] + right[i]);
        blockPeak = juce::jmax (blockPeak, std::abs (sample));
        fftData[fftFill++] = sample;
        ++samplesSinceOnset;

        if (fftFill == fftSize)
        {
            float energy = 0.0f;
            for (int n = 0; n < fftFill; ++n)
                energy += fftData[static_cast<size_t> (n)] * fftData[static_cast<size_t> (n)];
            energy /= static_cast<float> (fftFill);
            adaptiveEnergy = 0.995f * adaptiveEnergy + 0.005f * energy;
            previousEnergy = energy;
            energyHistory[static_cast<size_t> (energyHistoryWrite)] = energy;
            energyHistoryWrite = (energyHistoryWrite + 1) % static_cast<int> (energyHistory.size());
            energyHistoryCount = juce::jmin (energyHistoryCount + 1, static_cast<int> (energyHistory.size()));
            if (energyHistoryCount >= 32)
                estimateAudioBpm();

            std::fill (fftData.get() + fftFill, fftData.get() + fftSize * 2, 0.0f);
            analyzeFrame();
            fftFill = 0;
        }
    }

    inputLevel.store (0.85f * inputLevel.load (std::memory_order_relaxed) + 0.15f * blockPeak, std::memory_order_relaxed);
    analysisFrames.fetch_add (1, std::memory_order_relaxed);
    if (oneShotMode)
    {
        captureSamples += buffer.getNumSamples();
        const auto progress = juce::jmin (1.0f, static_cast<float> (captureSamples) / static_cast<float> (sampleRate * 8.0));
        captureProgress.store (progress, std::memory_order_relaxed);
        if (progress >= 1.0f)
        {
            oneShotMode = false;
            analysisEnabled.store (false, std::memory_order_relaxed);
        }
    }
}

void KeyBridgeAudioProcessor::estimateAudioBpm()
{
    const auto frameSeconds = static_cast<double> (fftSize) / sampleRate;
    float bestCorrelation = 0.0f;
    int bestLag = 0;
    const auto newest = energyHistoryWrite - 1;
    for (int lag = 2; lag <= 24; ++lag)
    {
        const auto usable = juce::jmin (energyHistoryCount - lag, 96);
        if (usable < 24)
            continue;
        float mean = 0.0f;
        for (int i = 0; i < usable; ++i)
            mean += energyHistory[static_cast<size_t> ((newest - i + static_cast<int> (energyHistory.size())) % static_cast<int> (energyHistory.size()))];
        mean /= static_cast<float> (usable);
        float numerator = 0.0f, denomA = 0.0f, denomB = 0.0f;
        for (int i = 0; i < usable; ++i)
        {
            const auto a = energyHistory[static_cast<size_t> ((newest - i + static_cast<int> (energyHistory.size())) % static_cast<int> (energyHistory.size()))] - mean;
            const auto b = energyHistory[static_cast<size_t> ((newest - i - lag + static_cast<int> (energyHistory.size()) * 2) % static_cast<int> (energyHistory.size()))] - mean;
            numerator += a * b;
            denomA += a * a;
            denomB += b * b;
        }
        const auto correlation = numerator / std::sqrt (juce::jmax (1.0e-12f, denomA * denomB));
        if (correlation > bestCorrelation)
        {
            bestCorrelation = correlation;
            bestLag = lag;
        }
    }
    if (bestLag > 0 && bestCorrelation > 0.22f)
    {
        auto bpm = 60.0 / (bestLag * frameSeconds);
        while (bpm < 60.0) bpm *= 2.0;
        while (bpm > 180.0) bpm *= 0.5;
        const auto old = detectedBpm.load (std::memory_order_relaxed);
        detectedBpm.store (old <= 0.0 ? bpm : 0.75 * old + 0.25 * bpm, std::memory_order_relaxed);
        bpmConfidence.store (juce::jlimit (0.0f, 1.0f, bestCorrelation), std::memory_order_relaxed);
    }
}

void KeyBridgeAudioProcessor::analyzeFrame()
{
    fft->performFrequencyOnlyForwardTransform (fftData.get());
    for (int bin = 2; bin < fftSize / 2; ++bin)
    {
        const auto magnitude = fftData[bin];
        const auto frequency = static_cast<float> (bin) * static_cast<float> (sampleRate) / static_cast<float> (fftSize);
        if (frequency < 55.0f || frequency > 2000.0f || magnitude < 0.001f)
            continue;
        const auto midi = 69.0f + 12.0f * std::log2 (frequency / 440.0f);
        const auto pitchClass = static_cast<int> (std::lround (midi)) % 12;
        if (pitchClass >= 0)
            chroma[static_cast<size_t> (pitchClass)] += magnitude;
    }

    if (++analysisFrameCount < 24)
        return;
    analysisFrameCount = 0;

    float total = 0.0f;
    for (const auto value : chroma) total += value;
    if (total <= 0.0f)
        return;
    for (auto& value : chroma) value /= total;

    float best = -1.0f, second = -1.0f;
    int bestKey = detectedKey.load (std::memory_order_relaxed);
    int bestMode = detectedMode.load (std::memory_order_relaxed);
    const auto consider = [&] (float score, int root, int mode)
    {
        if (score > best)
        {
            second = best;
            best = score;
            bestKey = root;
            bestMode = mode;
        }
        else if (score > second)
        {
            second = score;
        }
    };
    for (int root = 0; root < 12; ++root)
    {
        float major = 0.0f, minor = 0.0f;
        for (int i = 0; i < 12; ++i)
        {
            major += chroma[static_cast<size_t> ((root + i) % 12)] * majorProfile[static_cast<size_t> (i)];
            minor += chroma[static_cast<size_t> ((root + i) % 12)] * minorProfile[static_cast<size_t> (i)];
        }
        consider (major, root, 0);
        consider (minor, root, 1);
    }

    const auto confidence = juce::jlimit (0.0f, 1.0f,
        ((best - juce::jmax (0.0f, second)) / juce::jmax (0.000001f, std::abs (best))) * 8.0f);
    if (bestKey == candidateKey && bestMode == candidateMode)
        ++candidateWins;
    else
    {
        candidateKey = bestKey;
        candidateMode = bestMode;
        candidateWins = 1;
    }

    const auto currentKey = detectedKey.load (std::memory_order_relaxed);
    const auto currentMode = detectedMode.load (std::memory_order_relaxed);
    const auto isCurrentCandidate = bestKey == currentKey && bestMode == currentMode;
    if ((! hasStableKey && candidateWins >= 2 && confidence >= 0.18f)
        || (hasStableKey && candidateWins >= 3 && confidence >= 0.24f && ! isCurrentCandidate))
    {
        stableKey = bestKey;
        stableMode = bestMode;
        hasStableKey = true;
        detectedKey.store (stableKey, std::memory_order_relaxed);
        detectedMode.store (stableMode, std::memory_order_relaxed);
        hasStableDetectionFlag.store (true, std::memory_order_relaxed);
    }
    keyConfidence.store (confidence, std::memory_order_relaxed);
    for (auto& value : chroma) value *= 0.65f;
}

void KeyBridgeAudioProcessor::startFreshAnalysis() noexcept
{
    oneShotRequested.store (true, std::memory_order_release);
    resetRequested.store (true, std::memory_order_release);
    analysisEnabled.store (true, std::memory_order_release);
}

void KeyBridgeAudioProcessor::requestReferenceTone (int) noexcept
{
    // Intentionally output-neutral: KeyBridge must never inject audio into the host bus.
}

juce::AudioProcessorEditor* KeyBridgeAudioProcessor::createEditor()
{
    return new KeyBridgeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KeyBridgeAudioProcessor();
}
