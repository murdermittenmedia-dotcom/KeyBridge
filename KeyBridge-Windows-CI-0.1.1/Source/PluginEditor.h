#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class KeyBridgeAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit KeyBridgeAudioProcessorEditor (KeyBridgeAudioProcessor&);
    ~KeyBridgeAudioProcessorEditor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Label status;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBridgeAudioProcessorEditor)
};
