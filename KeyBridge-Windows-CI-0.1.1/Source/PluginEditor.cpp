#include "PluginEditor.h"

namespace
{
    constexpr std::array<const char*, 12> noteNames { { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" } };
}

KeyBridgeAudioProcessorEditor::KeyBridgeAudioProcessorEditor (KeyBridgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (620, 440);
    auto setupLabel = [this] (juce::Label& label, const juce::String& text, float size, juce::Justification justification)
    {
        addAndMakeVisible (label); label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (size, juce::Font::bold)); label.setColour (juce::Label::textColourId, juce::Colours::white);
        label.setJustificationType (justification);
    };
    setupLabel (title, "KEYBRIDGE  /  VOCAL FIT", 18.0f, juce::Justification::centredLeft);
    setupLabel (keyLabel, "Key: Listening", 28.0f, juce::Justification::centred);
    setupLabel (bpmLabel, "Host BPM: --", 18.0f, juce::Justification::centred);
    setupLabel (confidenceLabel, "Confidence: --", 14.0f, juce::Justification::centred);
    setupLabel (notesLabel, "Scale notes: --", 15.0f, juce::Justification::centred);
    setupLabel (recommendationLabel, "Vocal Fit: choose a profile and range", 16.0f, juce::Justification::centredLeft);

    profileLabel.setText ("PROFILE", juce::dontSendNotification); genreLabel.setText ("GENRE", juce::dontSendNotification); vibeLabel.setText ("VIBE", juce::dontSendNotification);
    for (auto* label : { &profileLabel, &genreLabel, &vibeLabel }) { addAndMakeVisible (label); label->setColour (juce::Label::textColourId, juce::Colours::lightgrey); label->setFont (juce::Font (11.0f, juce::Font::bold)); }
    addAndMakeVisible (profileBox); addAndMakeVisible (genreBox); addAndMakeVisible (vibeBox);
    profileBox.addItemList ({ "Male (starting estimate)", "Female (starting estimate)", "Custom", "Skip" }, 1);
    genreBox.addItemList ({ "Rap", "Melodic Rap", "Trap", "R&B", "Pop", "Gospel", "Soul", "Hip-Hop" }, 1);
    vibeBox.addItemList ({ "Happy", "Sad", "Dark", "Emotional", "Energetic", "Romantic", "Aggressive", "Confident", "Laid-back" }, 1);
    profileBox.setSelectedId (3); genreBox.setSelectedId (2); vibeBox.setSelectedId (4);

    addAndMakeVisible (analyzeButton); addAndMakeVisible (holdButton);
    analyzeButton.onClick = [this] { processor.setAnalysisEnabled (true); };
    holdButton.onClick = [this] { processor.setAnalysisEnabled (false); };
    for (int i = 0; i < 12; ++i)
    {
        noteButtons[static_cast<size_t>(i)] = std::make_unique<juce::TextButton> (noteNames[static_cast<size_t>(i)]);
        addAndMakeVisible (*noteButtons[static_cast<size_t>(i)]);
        noteButtons[static_cast<size_t>(i)]->onClick = [this, i] { playReferenceTone (60 + i); };
    }
    startTimerHz (8);
}

void KeyBridgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14161b));
    g.setColour (juce::Colour (0xffa51f3d)); g.fillRect (0, 0, getWidth(), 5);
    g.setColour (juce::Colour (0xff222630)); g.fillRoundedRectangle (18.0f, 55.0f, getWidth() - 36.0f, 122.0f, 10.0f);
    g.setColour (juce::Colour (0xff1d2028)); g.fillRoundedRectangle (18.0f, 188.0f, getWidth() - 36.0f, 92.0f, 10.0f);
    g.setColour (juce::Colours::grey); g.setFont (juce::Font (11.0f)); g.drawText ("CLICK A NOTE FOR A REFERENCE TONE", 26, 325, 250, 18, juce::Justification::left);
    g.drawText ("TRANSPARENT ANALYSIS  /  NO AUDIO ADJUSTMENTS", 330, 325, 260, 18, juce::Justification::right);
}

void KeyBridgeAudioProcessorEditor::resized()
{
    title.setBounds (20, 16, 400, 28);
    keyLabel.setBounds (28, 67, 265, 48); bpmLabel.setBounds (315, 72, 275, 34); confidenceLabel.setBounds (315, 108, 275, 24);
    notesLabel.setBounds (28, 130, 265, 28);
    recommendationLabel.setBounds (30, 198, 560, 28);
    profileLabel.setBounds (30, 235, 175, 16); genreLabel.setBounds (220, 235, 175, 16); vibeLabel.setBounds (410, 235, 175, 16);
    profileBox.setBounds (30, 253, 175, 26); genreBox.setBounds (220, 253, 175, 26); vibeBox.setBounds (410, 253, 175, 26);
    analyzeButton.setBounds (30, 355, 110, 30); holdButton.setBounds (150, 355, 90, 30);
    for (int i = 0; i < 12; ++i) noteButtons[static_cast<size_t>(i)]->setBounds (30 + i * 47, 345, 42, 42);
}

void KeyBridgeAudioProcessorEditor::timerCallback()
{
    const auto key = processor.getDetectedKey();
    const auto confidence = processor.getKeyConfidence();
    const auto bpm = processor.getHostBpm();
    keyLabel.setText ("Key: " + juce::String (noteNames[static_cast<size_t>(key)]) + " (prototype)", juce::dontSendNotification);
    bpmLabel.setText (bpm > 0.0 ? "Host BPM: " + juce::String (bpm, 2) : "Host BPM: --", juce::dontSendNotification);
    confidenceLabel.setText ("Confidence: " + juce::String (confidence * 100.0f, 0) + "%", juce::dontSendNotification);
    notesLabel.setText ("Scale notes: C  D  Eb  F  G  Ab  Bb  [demo]", juce::dontSendNotification);
    recommendationLabel.setText ("Vocal Fit: strongest starting note  " + juce::String (noteNames[static_cast<size_t>((key + 3) % 12)]) + "  |  choose profile / genre / vibe", juce::dontSendNotification);
}

void KeyBridgeAudioProcessorEditor::playReferenceTone (int midiNote)
{
    processor.requestReferenceTone (midiNote);
}
