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
    setSize (900, 760);

    auto setupLabel = [this] (juce::Label& label, const juce::String& text, float size, juce::Justification justification)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (size, juce::Font::bold));
        label.setColour (juce::Label::textColourId, juce::Colours::white);
        label.setJustificationType (justification);
        addAndMakeVisible (label);
    };

    setupLabel (title, "TUNERITE", 24.0f, juce::Justification::centredLeft);
    setupLabel (sectionLabel, "SINGLE INPUT  /  SEPARATE BEAT + VOCAL PASSES", 12.0f, juce::Justification::centredLeft);
    setupLabel (beatStatusLabel, "BEAT INPUT: NO SIGNAL", 12.0f, juce::Justification::centredLeft);
    setupLabel (vocalStatusLabel, "VOCAL INPUT: NO SIGNAL", 12.0f, juce::Justification::centredLeft);
    setupLabel (keyLabel, "Key: Listening", 26.0f, juce::Justification::centred);
    setupLabel (bpmLabel, "Project BPM: --   |   Detected Audio BPM: --", 15.0f, juce::Justification::centred);
    setupLabel (confidenceLabel, "Key confidence: --   |   BPM confidence: --", 13.0f, juce::Justification::centred);
    setupLabel (notesLabel, "Scale notes: listening...", 15.0f, juce::Justification::centredLeft);
    setupLabel (recommendationLabel, "Vocal Fit guidance: choose a profile and range", 16.0f, juce::Justification::centredLeft);
    setupLabel (guidanceLabel, "Vocal Fit is guidance from the detected beat key; Tunerite does not analyze or modify vocals.", 12.0f, juce::Justification::centredLeft);
    setupLabel (bpmActionLabel, "Host BPM is read-only in a standard VST3. This button copies the detected BPM for manual DAW entry.", 11.0f, juce::Justification::centredLeft);
    setupLabel (vocalMetricsLabel, "Vocal metrics: connect the Vocal Input sidechain to analyze range and delivery.", 12.0f, juce::Justification::centredLeft);
    setupLabel (settingsLabel, "Auto-Tune recommendations will appear after both Beat Input and Vocal Input are analyzed.", 12.0f, juce::Justification::centredLeft);

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
    setupCaption (analysisModeLabel, "ANALYSIS MODE");
    setupLabel (resultStatusLabel, "BEAT RESULT: NOT ANALYZED  |  VOCAL RESULT: NOT ANALYZED", 11.0f, juce::Justification::centredRight);

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
    addOptions (analysisModeBox, { "Beat Only", "Vocal Only", "Combined Recommendation" });
    profileBox.setSelectedId (1); genreBox.setSelectedId (3); deliveryBox.setSelectedId (2); vibeBox.setSelectedId (5);
    displayModeBox.setSelectedId (1); themeBox.setSelectedId (1); analysisModeBox.setSelectedId (1);
    analysisModeBox.onChange = [this]
    {
        processor.setAnalysisMode (analysisModeBox.getSelectedId() - 1);
        refreshRecommendation();
    };
    addAndMakeVisible (analysisModeBox);
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
    addAndMakeVisible (copySettingsButton);
    addAndMakeVisible (saveBeatButton);
    addAndMakeVisible (saveVocalButton);
    addAndMakeVisible (clearBeatButton);
    addAndMakeVisible (clearVocalButton);
    addAndMakeVisible (resetButton);
    analyzeButton.onClick = [this] { processor.startFreshAnalysis(); bpmActionLabel.setText ("Capturing a fresh 8-second beat window...", juce::dontSendNotification); };
    setBpmButton.onClick = [this] { copyDetectedBpm(); };
    copySettingsButton.onClick = [this] { copySettings(); };
    saveBeatButton.onClick = [this] { processor.saveBeatResult(); resultStatusLabel.setText ("BEAT RESULT: SAVED", juce::dontSendNotification); };
    saveVocalButton.onClick = [this] { processor.saveVocalResult(); resultStatusLabel.setText ("VOCAL RESULT: SAVED", juce::dontSendNotification); };
    clearBeatButton.onClick = [this] { processor.clearBeatResult(); };
    clearVocalButton.onClick = [this] { processor.clearVocalResult(); };
    resetButton.onClick = [this] { processor.resetAllResults(); resultStatusLabel.setText ("BEAT RESULT: NOT ANALYZED  |  VOCAL RESULT: NOT ANALYZED", juce::dontSendNotification); };
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
    g.fillRoundedRectangle (16.0f, 418.0f, getWidth() - 32.0f, 132.0f, 10.0f);
    g.setColour (juce::Colours::grey);
    g.setFont (juce::Font (11.0f));
    g.drawText ("DISPLAY OPTIONS", 28, 585, 180, 18, juce::Justification::left);
    g.drawText ("REFERENCE NOTE BUTTONS", 28, 635, 260, 18, juce::Justification::left);
    g.drawText ("TRANSPARENT ANALYSIS  /  NO AUDIO MODIFICATION", 420, 585, 360, 18, juce::Justification::right);
}

void KeyBridgeAudioProcessorEditor::resized()
{
    title.setBounds (24, 14, 300, 30);
    analysisModeLabel.setBounds (560, 10, 120, 18);
    analysisModeBox.setBounds (680, 8, 200, 28);
    resultStatusLabel.setBounds (560, 38, 320, 18);
    sectionLabel.setBounds (30, 42, 500, 18);
    beatStatusLabel.setBounds (30, 58, 240, 18);
    vocalStatusLabel.setBounds (540, 58, 240, 18);
    keyLabel.setBounds (30, 78, 330, 48);
    recommendationLabel.setBounds (30, 230, 760, 32);
    guidanceLabel.setBounds (30, 266, 760, 24);
    bpmActionLabel.setBounds (30, 202, 760, 20);
    vocalMetricsLabel.setBounds (30, 455, 840, 28);
    settingsLabel.setBounds (30, 490, 840, 44);
    bpmLabel.setBounds (365, 80, 410, 28);
    confidenceLabel.setBounds (365, 116, 410, 24);
    notesLabel.setBounds (30, 140, 740, 28);
    displayLabel.setBounds (30, 585, 80, 18);
    themeLabel.setBounds (200, 585, 80, 18);
    displayModeBox.setBounds (30, 606, 130, 28);
    themeBox.setBounds (200, 606, 130, 28);

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
    copySettingsButton.setBounds (620, 370, 140, 32);
    saveBeatButton.setBounds (30, 410, 145, 32);
    saveVocalButton.setBounds (185, 410, 155, 32);
    clearBeatButton.setBounds (350, 410, 110, 32);
    clearVocalButton.setBounds (470, 410, 120, 32);
    resetButton.setBounds (600, 410, 100, 32);
    for (int i = 0; i < 12; ++i)
        noteButtons[static_cast<size_t> (i)].setBounds (30 + i * 70, 660, 62, 38);
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
    resultStatusLabel.setText ("BEAT RESULT: " + juce::String (processor.hasSavedBeatResult() ? "SAVED" : "NOT ANALYZED")
        + "   |   VOCAL RESULT: " + juce::String (processor.hasSavedVocalResult() ? "SAVED" : "NOT ANALYZED")
        + "   |   RECOMMENDATION: " + juce::String (processor.hasSavedBeatResult() && processor.hasSavedVocalResult() ? "READY" : "NOT READY"), juce::dontSendNotification);

    keyLabel.setText (processor.getAnalysisMode() == 1
        ? juce::String ("Key: Beat analysis paused")
        : (processor.hasStableDetection()
            ? juce::String ("Key: ") + noteNames[static_cast<size_t> (key)] + " " + modeName
            : juce::String ("Key: Analyzing beat...")), juce::dontSendNotification);
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
    confidenceLabel.setText ("Beat confidence: " + juce::String (keyConfidence * 100.0f, 0) + "% key / "
                             + juce::String (bpmConfidence * 100.0f, 0) + "% BPM | RMS "
                             + juce::String (processor.getBeatRms(), 3) + " | Frames "
                             + juce::String (processor.getAnalysisFrames()) + " | Duration "
                             + juce::String (processor.getAnalysisDuration(), 1) + "s", juce::dontSendNotification);
    const auto beatActive = processor.getInputLevel() > 0.0001f;
    const auto vocalPass = processor.getAnalysisMode() == 1;
    const auto vocalActive = vocalPass ? beatActive : processor.getVocalInputLevel() > 0.0001f;
    beatStatusLabel.setText (vocalPass ? "BEAT INPUT: MUTED FOR VOCAL PASS" : (beatActive ? "BEAT INPUT: ACTIVE" : "BEAT INPUT: NO SIGNAL"), juce::dontSendNotification);
    vocalStatusLabel.setText (vocalPass ? (vocalActive ? "VOCAL INPUT: ACTIVE" : "VOCAL INPUT: NO SIGNAL") : "VOCAL INPUT: MUTED FOR BEAT PASS", juce::dontSendNotification);
    if (vocalPass && vocalActive && processor.getVocalConfidence() > 0.0f)
    {
        vocalMetricsLabel.setText ("Vocal range: " + juce::String (processor.getVocalLowestMidi(), 0) + "-" + juce::String (processor.getVocalHighestMidi(), 0)
            + " MIDI | Avg: " + juce::String (processor.getVocalAverageMidi(), 0)
            + " | Pitch confidence: " + juce::String (processor.getVocalConfidence() * 100.0f, 0) + "%"
            + " | RMS " + juce::String (processor.getVocalRms(), 3)
            + " | Voiced: " + juce::String (processor.getVocalVoicedPercent() * 100.0f, 0) + "%"
            + " | Sustained: " + juce::String (processor.getVocalSustainedPercent() * 100.0f, 0) + "%"
            + " | Changes: " + juce::String (processor.getVocalNoteChangeSpeed(), 1) + "/sec"
            + " | Frames: " + juce::String (processor.getVocalFrames())
            + " | " + (processor.isVocalMelodic() ? "Melodic" : "Spoken/Rap"), juce::dontSendNotification);
    }
    else
    {
        vocalMetricsLabel.setText (vocalPass ? "ANALYZE THE ISOLATED VOCAL, THEN SAVE VOCAL RESULT" : "Vocal metrics appear after a saved Vocal Only pass.", juce::dontSendNotification);
        settingsLabel.setText (vocalPass ? "Vocal Only uses the normal input and ignores beat detection." : "Use Vocal Only and Beat Only passes before requesting a combined recommendation.", juce::dontSendNotification);
    }

    juce::String scaleText = "Scale notes: ";
    for (int i = 0; i < 7; ++i)
        scaleText += juce::String (noteNames[static_cast<size_t> ((key + scale[static_cast<size_t> (i)]) % 12)]) + (i == 6 ? juce::String() : juce::String ("  "));
    notesLabel.setText (processor.hasStableDetection() ? scaleText : "Scale notes: waiting for a stable beat key...", juce::dontSendNotification);
    refreshRecommendation();
}

void KeyBridgeAudioProcessorEditor::refreshRecommendation()
{
    const auto combined = processor.getAnalysisMode() == 2;
    const auto key = juce::jlimit (0, 11, combined ? processor.getSavedBeatKey() : processor.getDetectedKey());
    const auto mode = combined ? processor.getSavedBeatMode() : processor.getDetectedMode();
    auto low = static_cast<int> (lowNoteSlider.getValue());
    auto high = juce::jmax (low, static_cast<int> (highNoteSlider.getValue()));
    const auto vocalConfidence = combined ? processor.getSavedVocalConfidence() : processor.getVocalConfidence();
    const auto vocalLow = combined ? processor.getSavedVocalLowestMidi() : processor.getVocalLowestMidi();
    const auto vocalHigh = combined ? processor.getSavedVocalHighestMidi() : processor.getVocalHighestMidi();
    if (vocalConfidence >= 0.45f && vocalHigh > vocalLow)
    {
        low = juce::jlimit (36, 84, static_cast<int> (std::lround (vocalLow)));
        high = juce::jlimit (low, 96, static_cast<int> (std::lround (vocalHigh)));
    }
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

    if (! combined)
    {
        recommendationLabel.setText (processor.getAnalysisMode() == 1 ? "VOCAL-ONLY ANALYSIS" : "BEAT-ONLY ANALYSIS", juce::dontSendNotification);
        guidanceLabel.setText (processor.getAnalysisMode() == 1
            ? "Mute the beat, play the isolated vocal, press ANALYZE, then SAVE VOCAL RESULT."
            : "Mute the vocal, play the isolated beat, press ANALYZE, then SAVE BEAT RESULT.", juce::dontSendNotification);
        return;
    }
    if (! processor.hasSavedBeatResult() || ! processor.hasSavedVocalResult())
    {
        recommendationLabel.setText ("RECOMMENDATION: NOT READY", juce::dontSendNotification);
        guidanceLabel.setText ("Analyze and save the vocal result and beat result separately first.", juce::dontSendNotification);
        return;
    }

    recommendationLabel.setText (juce::String ("STRONGEST: ") + noteNames[static_cast<size_t> (preferred % 12)] + juce::String (preferred)
        + "   |   SAFE: " + noteNames[static_cast<size_t> (safe1 % 12)] + ", " + noteNames[static_cast<size_t> (safe2 % 12)]
        + "   |   " + profile + " / " + genre + " / " + delivery + " / " + vibe, juce::dontSendNotification);
    guidanceLabel.setText ("Suggested range " + juce::String (low) + "-" + juce::String (high)
        + " MIDI   |   EXPRESSIVE: " + noteNames[static_cast<size_t> (expressive % 12)]
        + "   |   TENSION: " + noteNames[static_cast<size_t> (tension % 12)] + " (resolve deliberately)", juce::dontSendNotification);
    const auto melodicText = processor.isVocalMelodic() ? juce::String ("Medium") : juce::String ("Fast");
    const auto humanizeText = processor.isVocalMelodic() ? juce::String ("35") : juce::String ("10");
    const auto flexText = processor.isVocalMelodic() ? juce::String ("25") : juce::String ("10");
    const auto modeText = delivery == "Sung" ? juce::String ("Modern") : juce::String ("Classic");
    const auto qualityText = vibe == "Laid-back" ? juce::String ("HQ") : juce::String ("Low Latency");
    settingsLabel.setText (juce::String ("Auto-Tune: ") + juce::String (noteNames[static_cast<size_t> (key)]) + " "
        + juce::String (mode == 0 ? "Major" : "Minor")
        + " | Range " + juce::String (low) + "-" + juce::String (high) + " MIDI"
        + " | Retune: " + melodicText
        + " | Humanize: " + humanizeText + "%"
        + " | Flex-Tune: " + flexText + "%"
        + " | Mode: " + modeText
        + " | Quality: " + qualityText, juce::dontSendNotification);
}

void KeyBridgeAudioProcessorEditor::playReferenceTone (int midiNote)
{
    processor.requestReferenceTone (midiNote);
}

void KeyBridgeAudioProcessorEditor::copySettings()
{
    const auto key = juce::jlimit (0, 11, processor.getDetectedKey());
    const auto settings = juce::String ("Tunerite by MurderMittenMedia\n")
        + "Auto-Tune Key: " + juce::String (key) + "\n"
        + "Auto-Tune Scale: " + (processor.getDetectedMode() == 0 ? "Major" : "Minor") + "\n"
        + "Detected BPM: " + juce::String (processor.getDetectedBpm(), 2) + "\n"
        + "Vocal MIDI Range: " + juce::String (processor.getVocalLowestMidi(), 0) + "-" + juce::String (processor.getVocalHighestMidi(), 0) + "\n"
        + "Vocal confidence: " + juce::String (processor.getVocalConfidence() * 100.0f, 0) + "%\n"
        + "Retune Speed: Medium | Humanize: 35% | Flex-Tune: 25%\n"
        + "Mode: Modern | Quality: Low Latency";
    juce::SystemClipboard::copyTextToClipboard (settings);
    settingsLabel.setText ("Recommended settings copied to clipboard. Tunerite does not control Auto-Tune directly.", juce::dontSendNotification);
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
