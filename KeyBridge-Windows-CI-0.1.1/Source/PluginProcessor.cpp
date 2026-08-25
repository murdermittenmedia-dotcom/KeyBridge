#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr std::array<const char*, 12> noteNames { { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" } };
    constexpr std::array<float, 12> majorProfile { { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f } };
    constexpr std::array<float, 12> minorProfile { { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f } };
}

KeyBridgeAudioProcessor::KeyBridgeAudioProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    fft = std::make_unique<juce::dsp::FFT>(fftOrder);
}

void KeyBridgeAudioProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    fftFill = 0;
    chroma.fill (0.0f);
    fftData.fill (0.0f);
}

void KeyBridgeAudioProcessor::releaseResources() {}

bool KeyBridgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getChannelSet (true, 0);
    const auto mainOut = layouts.getChannelSet (false, 0);
    const auto inputIsUsable = mainIn.isDisabled() || mainIn == mainOut;
    const auto outputIsUsable = mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo();
    return inputIsUsable && outputIsUsable;
}

void KeyBridgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (position->getBpm().hasValue())
                hostBpm.store (*position->getBpm(), std::memory_order_relaxed);

    if (buffer.getNumChannels() == 0 || sampleRate <= 0.0)
        return;

    const auto frequency = toneFrequency.load (std::memory_order_relaxed);
    auto remaining = toneSamplesRemaining.load (std::memory_order_relaxed);
    if (frequency > 0.0 && remaining > 0)
    {
        for (int i = 0; i < buffer.getNumSamples() && remaining > 0; ++i, --remaining)
        {
            const auto fade = juce::jmin (1.0f, remaining / static_cast<float> (sampleRate * 0.03));
            const auto sample = static_cast<float> (0.12 * std::sin (tonePhase) * fade);
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.addSample (channel, i, sample);
            tonePhase += juce::MathConstants<double>::twoPi * frequency / sampleRate;
        }
        toneSamplesRemaining.store (remaining, std::memory_order_relaxed);
        if (remaining == 0) toneFrequency.store (0.0, std::memory_order_relaxed);
    }

    if (analysisEnabled.load (std::memory_order_relaxed) && buffer.getNumSamples() > 0)
    {
        const auto* left = buffer.getReadPointer (0);
        const auto* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : left;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            fftData[static_cast<size_t>(fftFill++)] = 0.5f * (left[i] + right[i]);
            if (fftFill == static_cast<int>(fftData.size() / 2))
            {
                std::fill (fftData.begin() + fftFill, fftData.end(), 0.0f);
                analyzeFrame();
                fftFill = 0;
            }
        }
    }
}

void KeyBridgeAudioProcessor::analyzeFrame()
{
    if (fft == nullptr || sampleRate <= 0.0)
        return;

    fft->performFrequencyOnlyForwardTransform (fftData.data());
    for (int bin = 2; bin < 1024; ++bin)
    {
        const auto magnitude = fftData[static_cast<size_t>(bin)];
        const auto frequency = static_cast<float>(bin) * static_cast<float>(sampleRate) / 2048.0f;
        if (frequency < 55.0f || frequency > 2000.0f || magnitude < 0.001f)
            continue;
        const auto midi = 69.0f + 12.0f * std::log2 (frequency / 440.0f);
        const auto pitchClass = static_cast<int>(std::lround (midi)) % 12;
        if (pitchClass >= 0)
            chroma[static_cast<size_t>(pitchClass)] += magnitude;
    }

    static int frameCounter = 0;
    if (++frameCounter < 24)
        return;
    frameCounter = 0;

    float total = 0.0f;
    for (auto value : chroma) total += value;
    if (total <= 0.0f) return;
    for (auto& value : chroma) value /= total;

    float best = -1.0f, second = -1.0f;
    int bestKey = detectedKey.load (std::memory_order_relaxed);
    for (int root = 0; root < 12; ++root)
    {
        float major = 0.0f, minor = 0.0f;
        for (int i = 0; i < 12; ++i)
        {
            major += chroma[static_cast<size_t>((root + i) % 12)] * majorProfile[static_cast<size_t>(i)];
            minor += chroma[static_cast<size_t>((root + i) % 12)] * minorProfile[static_cast<size_t>(i)];
        }
        const float candidate = juce::jmax (major, minor);
        if (candidate > best) { second = best; best = candidate; bestKey = root; }
        else if (candidate > second) second = candidate;
    }
    detectedKey.store (bestKey, std::memory_order_relaxed);
    keyConfidence.store (juce::jlimit (0.0f, 1.0f, (best - second) * 2.0f), std::memory_order_relaxed);
    for (auto& value : chroma) value *= 0.65f;
}

void KeyBridgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("KeyBridgeState");
    state.setProperty ("analysisEnabled", analysisEnabled.load(), nullptr);
    copyXmlToBinary (*state.createXml(), destData);
}

void KeyBridgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName ("KeyBridgeState"))
            analysisEnabled.store ((bool) xml->getBoolAttribute ("analysisEnabled", true));
}

juce::AudioProcessorEditor* KeyBridgeAudioProcessor::createEditor()
{
    try
    {
        return new KeyBridgeAudioProcessorEditor (*this);
    }
    catch (...)
    {
        return nullptr;
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KeyBridgeAudioProcessor();
}
