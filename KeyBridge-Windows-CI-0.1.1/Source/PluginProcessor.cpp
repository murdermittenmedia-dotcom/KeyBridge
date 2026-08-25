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
}

void KeyBridgeAudioProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = juce::jmax (newSampleRate, 8000.0);
    fftFill = 0;
    analysisFrameCount = 0;
    previousEnergy = 0.0f;
    samplesSinceOnset = 0;
    adaptiveEnergy = 0.0001f;
    chroma.fill (0.0f);
    fftData.fill (0.0f);
    tonePhase = 0.0;
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

    const auto* left = buffer.getReadPointer (0);
    const auto* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : left;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto sample = 0.5f * (left[i] + right[i]);
        fftData[static_cast<size_t> (fftFill++)] = sample;
        ++samplesSinceOnset;

        if (fftFill == static_cast<int> (fftData.size() / 2))
        {
            float energy = 0.0f;
            for (int n = 0; n < fftFill; ++n)
                energy += fftData[static_cast<size_t> (n)] * fftData[static_cast<size_t> (n)];
            energy /= static_cast<float> (fftFill);
            const auto rise = energy - previousEnergy;
            adaptiveEnergy = 0.995f * adaptiveEnergy + 0.005f * energy;
            if (rise > juce::jmax (0.00002f, adaptiveEnergy * 0.35f)
                && samplesSinceOnset > static_cast<int> (sampleRate * 0.25)
                && samplesSinceOnset < static_cast<int> (sampleRate * 1.0))
            {
                const auto bpm = 60.0 * sampleRate / static_cast<double> (samplesSinceOnset);
                const auto folded = bpm < 70.0 ? bpm * 2.0 : (bpm > 180.0 ? bpm * 0.5 : bpm);
                if (folded >= 60.0 && folded <= 180.0)
                {
                    const auto old = detectedBpm.load (std::memory_order_relaxed);
                    detectedBpm.store (old <= 0.0 ? folded : 0.82 * old + 0.18 * folded, std::memory_order_relaxed);
                    bpmConfidence.store (juce::jmin (1.0f, bpmConfidence.load (std::memory_order_relaxed) + 0.05f), std::memory_order_relaxed);
                }
                samplesSinceOnset = 0;
            }
            previousEnergy = energy;

            std::fill (fftData.begin() + fftFill, fftData.end(), 0.0f);
            analyzeFrame();
            fftFill = 0;
        }
    }

    const auto frequency = toneFrequency.load (std::memory_order_relaxed);
    auto remaining = toneSamplesRemaining.load (std::memory_order_relaxed);
    if (frequency > 0.0 && remaining > 0)
    {
        for (int i = 0; i < buffer.getNumSamples() && remaining > 0; ++i, --remaining)
        {
            const auto fade = juce::jmin (1.0f, remaining / static_cast<float> (sampleRate * 0.03));
            const auto sample = static_cast<float> (0.10 * std::sin (tonePhase) * fade);
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.addSample (channel, i, sample);
            tonePhase += juce::MathConstants<double>::twoPi * frequency / sampleRate;
        }
        toneSamplesRemaining.store (remaining, std::memory_order_relaxed);
        if (remaining == 0)
            toneFrequency.store (0.0, std::memory_order_relaxed);
    }
}

void KeyBridgeAudioProcessor::analyzeFrame()
{
    fft->performFrequencyOnlyForwardTransform (fftData.data());
    for (int bin = 2; bin < 1024; ++bin)
    {
        const auto magnitude = fftData[static_cast<size_t> (bin)];
        const auto frequency = static_cast<float> (bin) * static_cast<float> (sampleRate) / 2048.0f;
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
    for (int root = 0; root < 12; ++root)
    {
        float major = 0.0f, minor = 0.0f;
        for (int i = 0; i < 12; ++i)
        {
            major += chroma[static_cast<size_t> ((root + i) % 12)] * majorProfile[static_cast<size_t> (i)];
            minor += chroma[static_cast<size_t> ((root + i) % 12)] * minorProfile[static_cast<size_t> (i)];
        }
        if (major > best) { second = best; best = major; bestKey = root; bestMode = 0; }
        if (minor > best) { second = best; best = minor; bestKey = root; bestMode = 1; }
    }

    detectedKey.store (bestKey, std::memory_order_relaxed);
    detectedMode.store (bestMode, std::memory_order_relaxed);
    keyConfidence.store (juce::jlimit (0.0f, 1.0f, (best - juce::jmax (0.0f, second)) * 2.0f), std::memory_order_relaxed);
    for (auto& value : chroma) value *= 0.65f;
}

void KeyBridgeAudioProcessor::requestReferenceTone (int midiNote) noexcept
{
    toneFrequency.store (440.0 * std::pow (2.0, (midiNote - 69) / 12.0), std::memory_order_relaxed);
    toneSamplesRemaining.store (static_cast<int> (sampleRate * 0.65), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* KeyBridgeAudioProcessor::createEditor()
{
    return new KeyBridgeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KeyBridgeAudioProcessor();
}
