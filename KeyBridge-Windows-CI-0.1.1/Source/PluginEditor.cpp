#include "PluginEditor.h"

KeyBridgeAudioProcessorEditor::KeyBridgeAudioProcessorEditor (KeyBridgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
    setResizable (false, false);
    setSize (520, 220);

    auto configure = [this] (juce::Label& label, const juce::String& text, float fontSize)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (fontSize, juce::Font::bold));
        label.setColour (juce::Label::textColourId, juce::Colours::white);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);
    };

    configure (title, "KEYBRIDGE", 24.0f);
    configure (keyLabel, "Key: Listening", 20.0f);
    configure (bpmLabel, "Host BPM: --", 18.0f);
    configure (notesLabel, "Scale notes: waiting for analysis", 14.0f);

    startTimerHz (4);
}

void KeyBridgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14161b));
    g.setColour (juce::Colour (0xffa51f3d));
    g.fillRect (0, 0, getWidth(), 5);
    g.setColour (juce::Colour (0xff222630));
    g.fillRoundedRectangle (18.0f, 14.0f, getWidth() - 36.0f, getHeight() - 28.0f, 10.0f);
}

void KeyBridgeAudioProcessorEditor::resized()
{
    title.setBounds (24, 28, getWidth() - 48, 34);
    keyLabel.setBounds (24, 76, getWidth() - 48, 32);
    bpmLabel.setBounds (24, 112, getWidth() - 48, 30);
    notesLabel.setBounds (24, 154, getWidth() - 48, 26);
}

void KeyBridgeAudioProcessorEditor::timerCallback()
{
    const auto bpm = processor.getHostBpm();
    bpmLabel.setText (bpm > 0.0 ? "Host BPM: " + juce::String (bpm, 2) : "Host BPM: --",
                      juce::dontSendNotification);
    keyLabel.setText ("Key: analysis module next", juce::dontSendNotification);
    notesLabel.setText ("Scale notes: analysis module next", juce::dontSendNotification);
}
