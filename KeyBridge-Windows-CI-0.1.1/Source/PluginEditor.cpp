#include "PluginEditor.h"
#include <array>

namespace
{
    constexpr std::array<const char*, 12> noteNames { { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" } };
    constexpr std::array<int, 12> majorScale { { 0, 2, 4, 5, 7, 9, 11, -1, -1, -1, -1, -1 } };
    constexpr std::array<int, 12> minorScale { { 0, 2, 3, 5, 7, 8, 10, -1, -1, -1, -1, -1 } };
}

KeyBridgeAudioProcessorEditor::KeyBridgeAudioProcessorEditor (KeyBridgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
    setResizable (false, false);
    setSize (720, 520);

    auto label = [this] (juce::Label& l, const juce::String& text, float size, juce::Justification j = juce::Justification::centred)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font (size, juce::Font::bold));
        l.setColour (juce::Label::textColourId, juce::Colours::white);
        l.setJustificationType (j);
        addAndMakeVisible (l);
    };

    label (title, "KEYBRIDGE  /  VOCAL FIT BETA", 22.0f, juce::Justification::centredLeft);
    label (keyLabel, "Key: Listening", 26.0f);
    label (bpmLabel, "Host BPM: --   |   Detected BPM: --", 16.0f);
    label (confidenceLabel, "Confidence: --", 13.0f);
    label (notesLabel, "Scale notes: listening...", 14.0f, juce::Justification::centredLeft);
    label (recommendationLabel, "Vocal Fit: choose a profile and range", 16.0f, juce::Justification::centredLeft);

    profileLabel.setText ("VOCAL PROFILE", juce::dontSendNotification);
    rangeLabel.setText ("COMFORTABLE RANGE (MIDI)", juce::dontSendNotification);
    genreLabel.setText ("GENRE", juce::dontSendNotification);
    vibeLabel.setText ("VIBE", juce::dontSendNotification);
    for (auto* l : { &profileLabel, &rangeLabel, &genreLabel, &vibeLabel })
    {
        addAndMakeVisible (*l);
        l->setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        l->setFont (juce::Font (11.0f, juce::Font::bold));
    }

    profileBox.addItemList ({ "Male preset", "Female preset", "Custom", "Skip" }, 1);
    genreBox.addItemList ({ "Rap", "Melodic Rap", "Trap", "R&B", "Pop", "Gospel", "Soul", "Hip-Hop" }, 1);
    vibeBox.addItemList ({ "Happy", "Sad", "Dark", "Emotional", "Energetic", "Romantic", "Aggressive", "Confident", "Laid-back" }, 1);
    profileBox.setSelectedId (3); genreBox.setSelectedId (2); vibeBox.setSelectedId (4);
    for (auto* box : { &profileBox, &genreBox, &vibeBox })
    {
        addAndMakeVisible (*box);
        box->onChange = [this] { refreshRecommendation(); };
    }

    auto configureRange = [this] (juce::Slider& s, double value)
    {
        addAndMakeVisible (s);
        s.setRange (36.0, 84.0, 1.0);
        s.setValue (value, juce::dontSendNotification);
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
        s.onValueChange = [this] { refreshRecommendation(); };
    };
    configureRange (lowNoteSlider, 48.0);
    configureRange (highNoteSlider, 72.0);

    addAndMakeVisible (analyzeButton);
    addAndMakeVisible (holdButton);
    analyzeButton.onClick = [this] { processor.setAnalysisEnabled (true); };
    holdButton.onClick = [this] { processor.setAnalysisEnabled (false); };

    for (int i = 0; i < 12; ++i)
    {
        noteButtons[static_cast<size_t> (i)] = std::make_unique<juce::TextButton> (noteNames[static_cast<size_t> (i)]);
        addAndMakeVisible (*noteButtons[static_cast<size_t> (i)]);
        noteButtons[static_cast<size_t> (i)]->onClick = [this, i] { playReferenceTone (60 + i); };
    }

    startTimerHz (4);
}

void KeyBridgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111318));
    g.setColour (juce::Colour (0xffa51f3d));
    g.fillRect (0, 0, getWidth(), 5);
    g.setColour (juce::Colour (0xff20242d));
    g.fillRoundedRectangle (16.0f, 52.0f, getWidth() - 32.0f, 120.0f, 10.0f);
    g.setColour (juce::Colour (0xff1b1f27));
    g.fillRoundedRectangle (16.0f, 182.0f, getWidth() - 32.0f, 132.0f, 10.0f);
    g.setColour (juce::Colours::grey);
    g.setFont (juce::Font (11.0f));
    g.drawText ("CLICK A NOTE FOR A REFERENCE TONE", 26, 430, 280, 18, juce::Justification::left);
    g.drawText ("TRANSPARENT ANALYSIS  /  NO AUTO-ADJUSTMENTS", 400, 430, 290, 18, juce::Justification::right);
}

void KeyBridgeAudioProcessorEditor::resized()
{
    title.setBounds (20, 16, 500, 28);
    keyLabel.setBounds (30, 66, 300, 42);
    bpmLabel.setBounds (330, 68, 360, 28);
    confidenceLabel.setBounds (330, 102, 360, 22);
    notesLabel.setBounds (30, 124, 660, 24);
    recommendationLabel.setBounds (30, 192, 660, 28);

    profileLabel.setBounds (30, 230, 150, 18);
    rangeLabel.setBounds (200, 230, 220, 18);
    genreLabel.setBounds (435, 230, 120, 18);
    vibeLabel.setBounds (565, 230, 125, 18);
    profileBox.setBounds (30, 250, 150, 26);
    lowNoteSlider.setBounds (200, 250, 105, 26);
    highNoteSlider.setBounds (310, 250, 110, 26);
    genreBox.setBounds (435, 250, 120, 26);
    vibeBox.setBounds (565, 250, 125, 26);

    analyzeButton.setBounds (30, 285, 100, 28);
    holdButton.setBounds (140, 285, 85, 28);
    for (int i = 0; i < 12; ++i)
        noteButtons[static_cast<size_t> (i)]->setBounds (30 + i * 55, 350, 48, 42);
}

void KeyBridgeAudioProcessorEditor::timerCallback()
{
    static constexpr std::array<const char*, 12> noteNames { { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" } };
    const auto key = processor.getDetectedKey();
    const auto mode = processor.getDetectedMode();
    const auto host = processor.getHostBpm();
    const auto detected = processor.getDetectedBpm();
    const auto confidence = processor.getKeyConfidence();
    const auto modeName = mode == 0 ? "major" : "minor";
    const auto scale = mode == 0 ? majorScale : minorScale;

    keyLabel.setText ("Key: " + juce::String (noteNames[static_cast<size_t> (key)]) + " " + modeName, juce::dontSendNotification);
    bpmLabel.setText ("Host BPM: " + (host > 0.0 ? juce::String (host, 2) : "--")
                      + "   |   Detected BPM: " + (detected > 0.0 ? juce::String (detected, 2) : "--"), juce::dontSendNotification);
    confidenceLabel.setText ("Key confidence: " + juce::String (confidence * 100.0f, 0) + "%   |   BPM confidence: "
                             + juce::String (processor.getBpmConfidence() * 100.0f, 0) + "%", juce::dontSendNotification);

    juce::String scaleText = "Scale notes: ";
    for (int i = 0; i < 7; ++i)
        scaleText += noteNames[static_cast<size_t> ((key + scale[static_cast<size_t> (i)]) % 12)] + (i == 6 ? "" : "  ");
    notesLabel.setText (scaleText, juce::dontSendNotification);
    refreshRecommendation();
}

void KeyBridgeAudioProcessorEditor::refreshRecommendation()
{
    const auto key = processor.getDetectedKey();
    const auto mode = processor.getDetectedMode();
    const auto low = static_cast<int> (lowNoteSlider.getValue());
    const auto high = static_cast<int> (highNoteSlider.getValue());
    const auto profile = profileBox.getText();
    const auto genre = genreBox.getText();
    const auto vibe = vibeBox.getText();
    const auto rootMidi = 60 + key;
    const auto preferred = juce::jlimit (low, high, rootMidi + (vibe == "Energetic" ? 7 : (vibe == "Sad" || vibe == "Dark" ? 3 : 0)));
    const auto modeName = mode == 0 ? "major" : "minor";
    recommendationLabel.setText ("Vocal Fit: starting note " + juce::String (noteNames[static_cast<size_t> (preferred % 12)])
                                 + juce::String (preferred) + "  |  " + profile + " / " + genre + " / " + vibe
                                 + "  |  fit range " + juce::String (low) + "-" + juce::String (high)
                                 + "  |  " + noteNames[static_cast<size_t> (key)] + " " + modeName,
                                 juce::dontSendNotification);
}

void KeyBridgeAudioProcessorEditor::playReferenceTone (int midiNote)
{
    processor.requestReferenceTone (midiNote);
}
