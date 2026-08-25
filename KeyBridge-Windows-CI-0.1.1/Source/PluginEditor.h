#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class KeyBridgeAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit KeyBridgeAudioProcessorEditor (KeyBridgeAudioProcessor&);
    ~KeyBridgeAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    KeyBridgeAudioProcessor& processor;
    juce::Label title;
    juce::Label keyLabel;
    juce::Label bpmLabel;
    juce::Label notesLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBridgeAudioProcessorEditor)
};
