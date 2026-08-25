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
    return (input.isDisabled() || input == output)
        && (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo());
}

void KeyBridgeAudioProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    // Intentionally empty. This diagnostic build must be bit-transparent.
}

juce::AudioProcessorEditor* KeyBridgeAudioProcessor::createEditor()
{
    return new KeyBridgeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KeyBridgeAudioProcessor();
}
