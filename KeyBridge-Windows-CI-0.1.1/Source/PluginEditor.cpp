#include "PluginEditor.h"
#include <array>

namespace
{
    constexpr std::array<const char*, 12> noteNames { { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" } };
    constexpr std::array<int, 7> majorScale { { 0, 2, 4, 5, 7, 9, 11 } };
    constexpr std::array<int, 7> minorScale { { 0, 2, 3, 5, 7, 8, 10 } };
}

KeyBridgeAudioProcessorEditor::KeyBridgeAudioProcessorEditor (KeyBridgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
    setResizable (false, false);
    setSize (760, 560);

    auto setupLabel = [this] (juce::Label& label, const juce::String& text, float size, juce::Justification justification)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (size, juce::Font::bold));
        label.setColour (juce::Label::textColourId, juce::Colours::white);
        label.setJustificationType (justification);
        addAndMakeVisible (label);
    };

    setupLabel (title, "KEYBRIDGE  /  VOCAL FIT BETA", 22.0f, juce::Justification::centredLeft);
    setupLabel (keyLabel, "Key: Listening", 26.0f, juce::Justification::centred);
    setupLabel (bpmLabel, "Project BPM: --   |   Detected Audio BPM: --", 15.0f, juce::Justification::centred);
    setupLabel (confidenceLabel, "Key confidence: --   |   BPM confidence: --", 13.0f, juce::Justification::centred);
    setupLabel (notesLabel, "Scale notes: listening...", 15.0f, juce::Justification::centredLeft);
    setupLabel (recommendationLabel, "Vocal Fit: choose a profile and range", 16.0f, juce::Justification::centredLeft);
    setupLabel (guidanceLabel, "Safe notes remain inside the detected scale. Expressive notes add color; tension notes should resolve.", 12.0f, juce::Justification::centredLeft);

    const auto setupCaption = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (11.0f, juce::Font::bold));
        label.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible (label);
    };
    setupCaption (profileLabel, "VOCAL PROFILE");
    setupCaption (rangeLabel, "COMFORTABLE RANGE (MIDI)");
    setupCaption (genreLabel, "GENRE");
    setupCaption (deliveryLabel, "DELIVERY");
    setupCaption (vibeLabel, "VIBE");

    const auto addOptions = [] (juce::ComboBox& box, std::initializer_list<const char*> options)
    {
        int id = 1;
        for (const auto* option : options)
            box.addItem (option, id++);
    };
    addOptions (profileBox, { "Male", "Female", "Custom", "Skip" });
    addOptions (genreBox, { "Rap", "Melodic Rap", "Trap", "R&B", "Pop", "Gospel", "Soul", "Hip-Hop" });
    addOptions (deliveryBox, { "Rap", "Melodic", "Sung", "Spoken", "Chant" });
    addOptions (vibeBox, { "Happy", "Sad", "Dark", "Emotional", "Energetic", "Romantic", "Aggressive", "Confident", "Laid-back" });
    profileBox.setSelectedId (1); genreBox.setSelectedId (3); deliveryBox.setSelectedId (2); vibeBox.setSelectedId (5);
    for (auto* box : { &profileBox, &genreBox, &deliveryBox, &vibeBox })
    {
        addAndMakeVisible (*box);
        box->onChange = [this] { refreshRecommendation(); };
    }

    const auto setupRange = [this] (juce::Slider& slider, double value)
    {
        slider.setRange (36.0, 84.0, 1.0);
        slider.setValue (value, juce::dontSendNotification);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
        addAndMakeVisible (slider);
        slider.onValueChange = [this] { refreshRecommendation(); };
    };
    setupRange (lowNoteSlider, 48.0);
    setupRange (highNoteSlider, 72.0);

    addAndMakeVisible (analyzeButton);
    addAndMakeVisible (holdButton);
    addAndMakeVisible (lockButton);
    analyzeButton.onClick = [this] { processor.setAnalysisEnabled (true); };
    holdButton.onClick = [this] { processor.setAnalysisEnabled (false); };
    lockButton.onClick = [this] { processor.setAnalysisEnabled (false); };

    for (int i = 0; i < 12; ++i)
    {
        auto& button = noteButtons[static_cast<size_t> (i)];
        button.setButtonText (noteNames[static_cast<size_t> (i)]);
        addAndMakeVisible (button);
        button.onClick = [this, i] { playReferenceTone (60 + i); };
    }

}

void KeyBridgeAudioProcessorEditor::parentHierarchyChanged()
{
    if (isShowing())
    {
        if (! isTimerRunning())
            startTimerHz (4);
    }
    else
    {
        stopTimer();
    }
}

void KeyBridgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111318));
    g.setColour (juce::Colour (0xffa51f3d));
    g.fillRect (0, 0, getWidth(), 5);
    g.setColour (juce::Colour (0xff20242d));
    g.fillRoundedRectangle (16.0f, 52.0f, getWidth() - 32.0f, 125.0f, 10.0f);
    g.setColour (juce::Colour (0xff1b1f27));
    g.fillRoundedRectangle (16.0f, 188.0f, getWidth() - 32.0f, 165.0f, 10.0f);
    g.setColour (juce::Colours::grey);
    g.setFont (juce::Font (11.0f));
    g.drawText ("REFERENCE NOTE BUTTONS", 28, 425, 260, 18, juce::Justification::left);
    g.drawText ("TRANSPARENT ANALYSIS  /  NO AUTO-ADJUSTMENTS", 420, 425, 310, 18, juce::Justification::right);
}

void KeyBridgeAudioProcessorEditor::resized()
{
    title.setBounds (20, 16, 500, 28);
    keyLabel.setBounds (30, 65, 300, 42);
    bpmLabel.setBounds (330, 68, 400, 26);
    confidenceLabel.setBounds (330, 103, 400, 22);
    notesLabel.setBounds (30, 125, 700, 25);
    recommendationLabel.setBounds (30, 198, 700, 30);
    guidanceLabel.setBounds (30, 230, 700, 22);

    profileLabel.setBounds (30, 263, 125, 18);
    rangeLabel.setBounds (170, 263, 180, 18);
    genreLabel.setBounds (365, 263, 100, 18);
    deliveryLabel.setBounds (475, 263, 100, 18);
    vibeLabel.setBounds (585, 263, 140, 18);
    profileBox.setBounds (30, 284, 125, 26);
    lowNoteSlider.setBounds (170, 284, 85, 26);
    highNoteSlider.setBounds (260, 284, 90, 26);
    genreBox.setBounds (365, 284, 100, 26);
    deliveryBox.setBounds (475, 284, 100, 26);
    vibeBox.setBounds (585, 284, 140, 26);

    analyzeButton.setBounds (30, 322, 95, 28);
    holdButton.setBounds (132, 322, 80, 28);
    lockButton.setBounds (219, 322, 80, 28);
    for (int i = 0; i < 12; ++i)
        noteButtons[static_cast<size_t> (i)].setBounds (30 + i * 58, 450, 52, 38);
}

void KeyBridgeAudioProcessorEditor::timerCallback()
{
    const auto key = juce::jlimit (0, 11, processor.getDetectedKey());
    const auto mode = processor.getDetectedMode();
    const auto host = processor.getHostBpm();
    const auto detected = processor.getDetectedBpm();
    const auto keyConfidence = processor.getKeyConfidence();
    const auto bpmConfidence = processor.getBpmConfidence();
    const auto modeName = mode == 0 ? "major" : "minor";
    const auto& scale = mode == 0 ? majorScale : minorScale;

    keyLabel.setText (juce::String ("Key: ") + noteNames[static_cast<size_t> (key)] + " " + modeName, juce::dontSendNotification);
    bpmLabel.setText ("Project BPM: " + (host > 0.0 ? juce::String (host, 2) : "--")
                      + "   |   Detected Audio BPM: " + (detected > 0.0 ? juce::String (detected, 2) : "--"), juce::dontSendNotification);
    confidenceLabel.setText ("Key confidence: " + juce::String (keyConfidence * 100.0f, 0) + "%   |   BPM confidence: "
                             + juce::String (bpmConfidence * 100.0f, 0) + "%", juce::dontSendNotification);

    juce::String scaleText = "Scale notes: ";
    for (int i = 0; i < 7; ++i)
        scaleText += juce::String (noteNames[static_cast<size_t> ((key + scale[static_cast<size_t> (i)]) % 12)]) + (i == 6 ? juce::String() : juce::String ("  "));
    notesLabel.setText (scaleText, juce::dontSendNotification);
    refreshRecommendation();
}

void KeyBridgeAudioProcessorEditor::refreshRecommendation()
{
    const auto key = juce::jlimit (0, 11, processor.getDetectedKey());
    const auto mode = processor.getDetectedMode();
    const auto low = static_cast<int> (lowNoteSlider.getValue());
    const auto high = juce::jmax (low, static_cast<int> (highNoteSlider.getValue()));
    const auto profile = profileBox.getText();
    const auto genre = genreBox.getText();
    const auto delivery = deliveryBox.getText();
    const auto vibe = vibeBox.getText();
    const auto root = 60 + key;
    const auto preferred = juce::jlimit (low, high, root + (vibe == "Energetic" ? 7 : (vibe == "Sad" || vibe == "Dark" ? 3 : 0)));
    const auto safe1 = juce::jlimit (low, high, preferred + 2);
    const auto safe2 = juce::jlimit (low, high, preferred + (mode == 0 ? 4 : 3));
    const auto expressive = juce::jlimit (low, high, preferred + 1);
    const auto tension = juce::jlimit (low, high, preferred + 6);

    recommendationLabel.setText (juce::String ("STRONGEST: ") + noteNames[static_cast<size_t> (preferred % 12)] + juce::String (preferred)
        + "   |   SAFE: " + noteNames[static_cast<size_t> (safe1 % 12)] + ", " + noteNames[static_cast<size_t> (safe2 % 12)]
        + "   |   " + profile + " / " + genre + " / " + delivery + " / " + vibe, juce::dontSendNotification);
    guidanceLabel.setText ("Suggested range " + juce::String (low) + "-" + juce::String (high)
        + " MIDI   |   EXPRESSIVE: " + noteNames[static_cast<size_t> (expressive % 12)]
        + "   |   TENSION: " + noteNames[static_cast<size_t> (tension % 12)] + " (resolve deliberately)", juce::dontSendNotification);
}

void KeyBridgeAudioProcessorEditor::playReferenceTone (int midiNote)
{
    processor.requestReferenceTone (midiNote);
}
