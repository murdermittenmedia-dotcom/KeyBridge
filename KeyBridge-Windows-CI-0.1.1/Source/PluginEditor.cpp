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
    setSize (820, 640);

    auto setupLabel = [this] (juce::Label& label, const juce::String& text, float size, juce::Justification justification)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (size, juce::Font::bold));
        label.setColour (juce::Label::textColourId, juce::Colours::white);
        label.setJustificationType (justification);
        addAndMakeVisible (label);
    };

    setupLabel (title, "TUNERITE", 24.0f, juce::Justification::centredLeft);
    setupLabel (sectionLabel, "BEAT ANALYSIS  /  LISTEN TO THE AUDIO ON THIS MIXER TRACK", 12.0f, juce::Justification::centredLeft);
    setupLabel (keyLabel, "Key: Listening", 26.0f, juce::Justification::centred);
    setupLabel (bpmLabel, "Project BPM: --   |   Detected Audio BPM: --", 15.0f, juce::Justification::centred);
    setupLabel (confidenceLabel, "Key confidence: --   |   BPM confidence: --", 13.0f, juce::Justification::centred);
    setupLabel (notesLabel, "Scale notes: listening...", 15.0f, juce::Justification::centredLeft);
    setupLabel (recommendationLabel, "Vocal Fit guidance: choose a profile and range", 16.0f, juce::Justification::centredLeft);
    setupLabel (guidanceLabel, "Vocal Fit is guidance from the detected beat key; Tunerite does not analyze or modify vocals.", 12.0f, juce::Justification::centredLeft);
    setupLabel (bpmActionLabel, "Host BPM is read-only in a standard VST3. This button copies the detected BPM for manual DAW entry.", 11.0f, juce::Justification::centredLeft);

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
    setupCaption (displayLabel, "VIEW");
    setupCaption (themeLabel, "THEME");

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
    addOptions (displayModeBox, { "Full", "Compact", "Engineering" });
    addOptions (themeBox, { "Dark", "Slate", "High Contrast" });
    profileBox.setSelectedId (1); genreBox.setSelectedId (3); deliveryBox.setSelectedId (2); vibeBox.setSelectedId (5);
    displayModeBox.setSelectedId (1); themeBox.setSelectedId (1);
    for (auto* box : { &profileBox, &genreBox, &deliveryBox, &vibeBox, &displayModeBox, &themeBox })
    {
        addAndMakeVisible (*box);
        box->onChange = [this] { refreshRecommendation(); repaint(); };
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

    analyzeButton.setButtonText ("ANALYZE CURRENT AUDIO");
    setBpmButton.setButtonText ("COPY DETECTED BPM");
    addAndMakeVisible (analyzeButton);
    addAndMakeVisible (holdButton);
    addAndMakeVisible (lockButton);
    addAndMakeVisible (setBpmButton);
    analyzeButton.onClick = [this] { processor.startFreshAnalysis(); bpmActionLabel.setText ("Capturing a fresh 8-second beat window...", juce::dontSendNotification); };
    setBpmButton.onClick = [this] { copyDetectedBpm(); };
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
    const auto theme = themeBox.getText();
    const auto background = theme == "High Contrast" ? juce::Colour (0xff050505)
                            : (theme == "Slate" ? juce::Colour (0xff17212b) : juce::Colour (0xff111318));
    g.fillAll (background);
    g.setColour (juce::Colour (0xffa51f3d));
    g.fillRect (0, 0, getWidth(), 5);
    g.setColour (juce::Colour (0xff20242d));
    g.fillRoundedRectangle (16.0f, 54.0f, getWidth() - 32.0f, 150.0f, 10.0f);
    g.setColour (juce::Colour (0xff1b1f27));
    g.fillRoundedRectangle (16.0f, 216.0f, getWidth() - 32.0f, 190.0f, 10.0f);
    g.setColour (juce::Colour (0xff161a21));
    g.fillRoundedRectangle (16.0f, 418.0f, getWidth() - 32.0f, 100.0f, 10.0f);
    g.setColour (juce::Colours::grey);
    g.setFont (juce::Font (11.0f));
    g.drawText ("DISPLAY OPTIONS", 28, 535, 180, 18, juce::Justification::left);
    g.drawText ("REFERENCE NOTE BUTTONS", 28, 570, 260, 18, juce::Justification::left);
    g.drawText ("TRANSPARENT ANALYSIS  /  NO AUDIO MODIFICATION", 420, 535, 360, 18, juce::Justification::right);
}

void KeyBridgeAudioProcessorEditor::resized()
{
    title.setBounds (24, 14, 300, 30);
    sectionLabel.setBounds (30, 42, 760, 18);
    keyLabel.setBounds (30, 70, 330, 48);
    recommendationLabel.setBounds (30, 230, 760, 32);
    guidanceLabel.setBounds (30, 266, 760, 24);
    bpmActionLabel.setBounds (30, 202, 760, 20);
    bpmLabel.setBounds (365, 72, 410, 28);
    confidenceLabel.setBounds (365, 108, 410, 24);
    notesLabel.setBounds (30, 132, 740, 28);
    displayLabel.setBounds (30, 438, 80, 18);
    themeLabel.setBounds (200, 438, 80, 18);
    displayModeBox.setBounds (30, 459, 130, 28);
    themeBox.setBounds (200, 459, 130, 28);

    profileLabel.setBounds (30, 310, 125, 18);
    rangeLabel.setBounds (170, 310, 180, 18);
    genreLabel.setBounds (365, 310, 100, 18);
    deliveryLabel.setBounds (475, 310, 100, 18);
    vibeLabel.setBounds (585, 310, 180, 18);
    profileBox.setBounds (30, 331, 125, 28);
    lowNoteSlider.setBounds (170, 331, 85, 28);
    highNoteSlider.setBounds (260, 331, 90, 28);
    genreBox.setBounds (365, 331, 100, 28);
    deliveryBox.setBounds (475, 331, 100, 28);
    vibeBox.setBounds (585, 331, 180, 28);

    analyzeButton.setBounds (30, 370, 190, 32);
    holdButton.setBounds (230, 370, 80, 32);
    lockButton.setBounds (320, 370, 80, 32);
    setBpmButton.setBounds (420, 370, 180, 32);
    for (int i = 0; i < 12; ++i)
        noteButtons[static_cast<size_t> (i)].setBounds (30 + i * 63, 589, 57, 38);
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

    keyLabel.setText (processor.hasStableDetection()
        ? juce::String ("Key: ") + noteNames[static_cast<size_t> (key)] + " " + modeName
        : juce::String ("Key: Analyzing beat..."), juce::dontSendNotification);
    const auto progress = processor.getCaptureProgress();
    if (processor.isAnalysisActive() && progress > 0.0f && progress < 1.0f)
        bpmLabel.setText ("Analyzing current audio... " + juce::String (progress * 100.0f, 0) + "%", juce::dontSendNotification);
    else
    {
        bpmLabel.setText ("Project BPM: " + (host > 0.0 ? juce::String (host, 2) : "--")
                          + "   |   Detected Audio BPM: " + (detected > 0.0 ? juce::String (detected, 2) : "--"), juce::dontSendNotification);
        if (detected > 0.0 && progress >= 1.0f)
            bpmActionLabel.setText ("Analysis complete. Use COPY DETECTED BPM to place this value on the DAW tempo field.", juce::dontSendNotification);
    }
    confidenceLabel.setText ("Key confidence: " + juce::String (keyConfidence * 100.0f, 0) + "%   |   BPM confidence: "
                             + juce::String (bpmConfidence * 100.0f, 0) + "%   |   Input: "
                             + (processor.getInputLevel() > 0.0001f ? "ACTIVE" : "SILENT")
                             + "   |   Frames: " + juce::String (processor.getAnalysisFrames()), juce::dontSendNotification);

    juce::String scaleText = "Scale notes: ";
    for (int i = 0; i < 7; ++i)
        scaleText += juce::String (noteNames[static_cast<size_t> ((key + scale[static_cast<size_t> (i)]) % 12)]) + (i == 6 ? juce::String() : juce::String ("  "));
    notesLabel.setText (processor.hasStableDetection() ? scaleText : "Scale notes: waiting for a stable beat key...", juce::dontSendNotification);
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

    if (! processor.hasStableDetection())
    {
        recommendationLabel.setText ("VOCAL FIT GUIDANCE: waiting for a stable beat key. Tunerite does not analyze vocals.", juce::dontSendNotification);
        guidanceLabel.setText ("Play the beat, then press ANALYZE CURRENT AUDIO to begin a fresh beat-only measurement.", juce::dontSendNotification);
        return;
    }

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

void KeyBridgeAudioProcessorEditor::copyDetectedBpm()
{
    const auto detected = processor.getDetectedBpm();
    if (detected <= 0.0)
    {
        bpmActionLabel.setText ("No detected BPM yet. Analyze the current beat first.", juce::dontSendNotification);
        return;
    }

    juce::SystemClipboard::copyTextToClipboard (juce::String (detected, 2));
    bpmActionLabel.setText ("Detected BPM " + juce::String (detected, 2) + " copied. Paste it into FL Studio's tempo field.", juce::dontSendNotification);
}
