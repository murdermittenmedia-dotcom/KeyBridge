#include "PluginEditor.h"

KeyBridgeAudioProcessorEditor::KeyBridgeAudioProcessorEditor (KeyBridgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
    setResizable (false, false);
    setSize (520, 180);

    statusLabel.setText ("KeyBridge\nLifecycle test build", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    statusLabel.setFont (juce::Font (juce::FontOptions (22.0f).withStyle (juce::Font::bold)));
    addAndMakeVisible (statusLabel);
}

void KeyBridgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14161b));
    g.setColour (juce::Colour (0xffa51f3d));
    g.fillRect (0, 0, getWidth(), 5);
}

void KeyBridgeAudioProcessorEditor::resized()
{
    statusLabel.setBounds (20, 30, getWidth() - 40, getHeight() - 50);
}
