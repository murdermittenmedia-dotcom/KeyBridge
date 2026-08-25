#include "PluginEditor.h"

KeyBridgeAudioProcessorEditor::KeyBridgeAudioProcessorEditor (KeyBridgeAudioProcessor& processor)
    : AudioProcessorEditor (&processor)
{
    setOpaque (true);
    setResizable (false, false);
    setSize (480, 220);
    status.setText ("KeyBridge diagnostic build\n\nHost-load and transparent pass-through test", juce::dontSendNotification);
    status.setColour (juce::Label::textColourId, juce::Colours::white);
    status.setFont (juce::Font (18.0f, juce::Font::bold));
    status.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (status);
}

void KeyBridgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111318));
    g.setColour (juce::Colour (0xffa51f3d));
    g.fillRect (0, 0, getWidth(), 5);
}

void KeyBridgeAudioProcessorEditor::resized()
{
    status.setBounds (20, 25, getWidth() - 40, getHeight() - 50);
}
