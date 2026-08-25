#include "PluginProcessor.h"
#include "PluginEditor.h"

KeyBridgeAudioProcessor::KeyBridgeAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
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

void KeyBridgeAudioProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (position->getBpm().hasValue())
                hostBpm.store (*position->getBpm(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* KeyBridgeAudioProcessor::createEditor()
{
    return new KeyBridgeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KeyBridgeAudioProcessor();
}
