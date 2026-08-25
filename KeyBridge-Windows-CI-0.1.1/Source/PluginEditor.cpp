#include "PluginEditor.h"
#include <array>
#include <cmath>

namespace
{
    constexpr std::array<const char*, 12> noteNames { { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" } };
    constexpr std::array<int, 7> majorScale { { 0, 2, 4, 5, 7, 9, 11 } };
    constexpr std::array<int, 7> minorScale { { 0, 2, 3, 5, 7, 8, 10 } };

    juce::Colour cardColour (int variant)
    {
        return variant == 0 ? juce::Colour (0xff18212c)
             : variant == 1 ? juce::Colour (0xff1b252f)
                            : juce::Colour (0xff20202b);
    }
}

KeyBridgeAudioProcessorEditor::KeyBridgeAudioProcessorEditor (KeyBridgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (840, 580, 1280, 900);
    setSize (1040, 660);

    const auto addLabel = [this] (juce::Label& label, float size, juce::Justification justification, juce::Colour colour)
    {
        label.setFont (juce::Font (size, juce::Font::bold));
        label.setJustificationType (justification);
        label.setColour (juce::Label::textColourId, colour);
        addAndMakeVisible (label);
    };

    addLabel (title, 26.0f, juce::Justification::centredLeft, juce::Colours::white);
    addLabel (subtitle, 12.0f, juce::Justification::centredLeft, juce::Colour (0xffa9b8c7));
    addLabel (modeStatus, 12.0f, juce::Justification::centredRight, juce::Colour (0xff9cc6ff));
    addLabel (beatCardTitle, 13.0f, juce::Justification::centredLeft, juce::Colour (0xffa7d8ff));
    addLabel (vocalCardTitle, 13.0f, juce::Justification::centredLeft, juce::Colour (0xffb8ebc4));
    addLabel (recommendationCardTitle, 13.0f, juce::Justification::centredLeft, juce::Colour (0xffffcf8a));

    for (auto* label : { &beatStatus, &vocalStatus, &beatResultStatus, &vocalResultStatus, &recommendationStatus,
                         &keyLabel, &bpmLabel, &confidenceLabel, &notesLabel, &beatMetrics, &vocalMetrics,
                         &guidanceLabel, &settingsLabel })
        addLabel (*label, 12.0f, juce::Justification::centredLeft, juce::Colour (0xffe6edf5));

    for (auto* label : { &profileCaption, &genreCaption, &deliveryCaption, &vibeCaption })
        addLabel (*label, 10.0f, juce::Justification::centredLeft, juce::Colour (0xff94a3b8));

    title.setText ("TUNERITE", juce::dontSendNotification);
    subtitle.setText ("Evidence-based beat and vocal analysis for Auto-Tune starting recommendations", juce::dontSendNotification);
    beatCardTitle.setText ("01  BEAT RESULT", juce::dontSendNotification);
    vocalCardTitle.setText ("02  VOCAL RESULT", juce::dontSendNotification);
    recommendationCardTitle.setText ("03  COMBINED RECOMMENDATION", juce::dontSendNotification);

    const auto addOptions = [] (juce::ComboBox& box, std::initializer_list<const char*> options)
    {
        int id = 1;
        for (const auto* option : options)
            box.addItem (option, id++);
    };
    addOptions (analysisModeBox, { "Beat Only", "Vocal Only", "Combined Recommendation" });
    addOptions (profileBox, { "Auto Range", "Male Hint", "Female Hint", "Custom" });
    addOptions (genreBox, { "Rap", "Melodic Rap", "Trap", "R&B", "Pop", "Gospel", "Soul", "Hip-Hop" });
    addOptions (deliveryBox, { "Rap", "Melodic", "Sung", "Spoken", "Chant" });
    addOptions (vibeBox, { "Natural", "Hard Tune", "Dark", "Emotional", "Energetic", "Romantic", "Aggressive", "Laid-back" });

    analysisModeBox.setSelectedId (1, juce::dontSendNotification);
    profileBox.setSelectedId (1, juce::dontSendNotification);
    genreBox.setSelectedId (3, juce::dontSendNotification);
    deliveryBox.setSelectedId (2, juce::dontSendNotification);
    vibeBox.setSelectedId (1, juce::dontSendNotification);

    for (auto* box : { &analysisModeBox, &profileBox, &genreBox, &deliveryBox, &vibeBox })
    {
        box->setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff111820));
        box->setColour (juce::ComboBox::textColourId, juce::Colours::white);
        box->setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3c4b5c));
        addAndMakeVisible (*box);
    }

    analysisModeBox.onChange = [this]
    {
        processor.setAnalysisMode (analysisModeBox.getSelectedId() - 1);
        processor.setAnalysisEnabled (true);
        updateModeControls();
        refreshRecommendation();
    };
    for (auto* box : { &profileBox, &genreBox, &deliveryBox, &vibeBox })
        box->onChange = [this] { refreshRecommendation(); };

    for (auto* button : { &analyzeButton, &saveButton, &clearBeatButton, &clearVocalButton, &resetButton,
                          &copyBpmButton, &copySettingsButton, &holdButton, &lockButton })
    {
        button->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff263848));
        button->setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff356e86));
        button->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        addAndMakeVisible (*button);
    }

    analyzeButton.onClick = [this]
    {
        processor.startFreshAnalysis();
        modeStatus.setText ("CAPTURING CURRENT AUDIO…", juce::dontSendNotification);
    };
    saveButton.onClick = [this]
    {
        if (processor.getAnalysisMode() == 0)
            processor.saveBeatResult();
        else if (processor.getAnalysisMode() == 1)
            processor.saveVocalResult();
        refreshRecommendation();
    };
    clearBeatButton.onClick = [this] { processor.clearBeatResult(); refreshRecommendation(); };
    clearVocalButton.onClick = [this] { processor.clearVocalResult(); refreshRecommendation(); };
    resetButton.onClick = [this] { processor.resetAllResults(); refreshRecommendation(); };
    copyBpmButton.onClick = [this] { copyDetectedBpm(); };
    copySettingsButton.onClick = [this] { copySettings(); };
    holdButton.onClick = [this] { processor.setAnalysisEnabled (false); modeStatus.setText ("ANALYSIS HELD", juce::dontSendNotification); };
    lockButton.onClick = [this] { processor.setAnalysisEnabled (false); modeStatus.setText ("RESULTS LOCKED", juce::dontSendNotification); };

    setCaption (profileCaption, "VOCAL PROFILE");
    setCaption (genreCaption, "GENRE");
    setCaption (deliveryCaption, "DELIVERY");
    setCaption (vibeCaption, "STYLE / VIBE");
    updateModeControls();
}

void KeyBridgeAudioProcessorEditor::setCaption (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
}

void KeyBridgeAudioProcessorEditor::parentHierarchyChanged()
{
    if (isShowing())
    {
        if (! isTimerRunning())
            startTimerHz (5);
    }
    else
    {
        stopTimer();
    }
}

void KeyBridgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d1117));
    g.setColour (juce::Colour (0xffc33257));
    g.fillRect (0, 0, getWidth(), 5);

    const int margin = 16;
    const int gap = 12;
    const int top = 84;
    const int cardsHeight = 226;
    const int usableWidth = getWidth() - margin * 2 - gap * 2;
    const int cardWidth = usableWidth / 3;

    for (int i = 0; i < 3; ++i)
    {
        g.setColour (cardColour (i));
        g.fillRoundedRectangle (static_cast<float> (margin + i * (cardWidth + gap)), static_cast<float> (top), static_cast<float> (cardWidth), static_cast<float> (cardsHeight), 12.0f);
    }

    g.setColour (juce::Colour (0xff161d26));
    g.fillRoundedRectangle (static_cast<float> (margin), 324.0f, static_cast<float> (getWidth() - margin * 2), 68.0f, 12.0f);
    g.setColour (juce::Colour (0xff121922));
    g.fillRoundedRectangle (static_cast<float> (margin), 404.0f, static_cast<float> (getWidth() - margin * 2), 84.0f, 12.0f);
    g.setColour (juce::Colour (0xff131a22));
    g.fillRoundedRectangle (static_cast<float> (margin), 500.0f, static_cast<float> (getWidth() - margin * 2), static_cast<float> (getHeight() - 516), 12.0f);
}

void KeyBridgeAudioProcessorEditor::resized()
{
    const int margin = 16;
    const int gap = 12;
    const int usableWidth = getWidth() - margin * 2 - gap * 2;
    const int cardWidth = usableWidth / 3;
    const int cardX1 = margin;
    const int cardX2 = cardX1 + cardWidth + gap;
    const int cardX3 = cardX2 + cardWidth + gap;

    title.setBounds (margin, 12, 160, 30);
    subtitle.setBounds (margin, 42, 540, 18);
    analysisModeBox.setBounds (getWidth() - 280, 18, 264, 30);
    modeStatus.setBounds (getWidth() - 450, 52, 434, 18);

    beatCardTitle.setBounds (cardX1 + 16, 98, cardWidth - 32, 20);
    beatStatus.setBounds (cardX1 + 16, 124, cardWidth - 32, 20);
    beatResultStatus.setBounds (cardX1 + 16, 148, cardWidth - 32, 18);
    keyLabel.setBounds (cardX1 + 16, 172, cardWidth - 32, 26);
    bpmLabel.setBounds (cardX1 + 16, 202, cardWidth - 32, 20);
    confidenceLabel.setBounds (cardX1 + 16, 226, cardWidth - 32, 20);
    notesLabel.setBounds (cardX1 + 16, 250, cardWidth - 32, 34);
    beatMetrics.setBounds (cardX1 + 16, 284, cardWidth - 32, 18);

    vocalCardTitle.setBounds (cardX2 + 16, 98, cardWidth - 32, 20);
    vocalStatus.setBounds (cardX2 + 16, 124, cardWidth - 32, 20);
    vocalResultStatus.setBounds (cardX2 + 16, 148, cardWidth - 32, 18);
    vocalMetrics.setBounds (cardX2 + 16, 176, cardWidth - 32, 112);

    recommendationCardTitle.setBounds (cardX3 + 16, 98, cardWidth - 32, 20);
    recommendationStatus.setBounds (cardX3 + 16, 124, cardWidth - 32, 20);
    guidanceLabel.setBounds (cardX3 + 16, 154, cardWidth - 32, 64);
    settingsLabel.setBounds (cardX3 + 16, 224, cardWidth - 32, 72);

    analyzeButton.setBounds (margin + 16, 342, 180, 32);
    saveButton.setBounds (margin + 208, 342, 170, 32);
    clearBeatButton.setBounds (margin + 390, 342, 112, 32);
    clearVocalButton.setBounds (margin + 514, 342, 122, 32);
    resetButton.setBounds (margin + 648, 342, 110, 32);
    holdButton.setBounds (margin + 770, 342, 78, 32);
    lockButton.setBounds (margin + 860, 342, 78, 32);

    const int rowY = 430;
    profileCaption.setBounds (margin + 16, rowY, 126, 16);
    profileBox.setBounds (margin + 16, rowY + 20, 170, 28);
    genreCaption.setBounds (margin + 205, rowY, 110, 16);
    genreBox.setBounds (margin + 205, rowY + 20, 180, 28);
    deliveryCaption.setBounds (margin + 404, rowY, 110, 16);
    deliveryBox.setBounds (margin + 404, rowY + 20, 170, 28);
    vibeCaption.setBounds (margin + 593, rowY, 120, 16);
    vibeBox.setBounds (margin + 593, rowY + 20, 190, 28);
    copyBpmButton.setBounds (getWidth() - 232, rowY + 20, 98, 28);
    copySettingsButton.setBounds (getWidth() - 122, rowY + 20, 106, 28);
}

void KeyBridgeAudioProcessorEditor::timerCallback()
{
    const auto selectedMode = processor.getAnalysisMode();
    const auto beatSaved = processor.hasSavedBeatResult();
    const auto vocalSaved = processor.hasSavedVocalResult();
    const auto progress = processor.getCaptureProgress();
    const auto liveKey = juce::jlimit (0, 11, processor.getDetectedKey());
    const auto key = selectedMode == 2 && beatSaved ? processor.getSavedBeatKey() : liveKey;
    const auto scaleMode = selectedMode == 2 && beatSaved ? processor.getSavedBeatMode() : processor.getDetectedMode();
    const auto bpm = selectedMode == 2 && beatSaved ? processor.getSavedBeatBpm() : processor.getDetectedBpm();
    const auto keyConfidence = selectedMode == 2 && beatSaved ? processor.getSavedBeatKeyConfidence() : processor.getKeyConfidence();
    const auto bpmConfidence = selectedMode == 2 && beatSaved ? processor.getSavedBeatBpmConfidence() : processor.getBpmConfidence();
    const auto alternativeBpm = processor.getAlternativeBpm();
    const auto& scale = scaleMode == 0 ? majorScale : minorScale;

    const auto modeText = selectedMode == 0 ? "BEAT ONLY — mute vocal, analyze beat"
                        : selectedMode == 1 ? "VOCAL ONLY — mute beat, analyze vocal"
                                            : "COMBINED — saved results only";
    modeStatus.setText (progress > 0.0f && progress < 1.0f
        ? juce::String ("CAPTURING… ") + juce::String (progress * 100.0f, 0) + "%"
        : modeText, juce::dontSendNotification);

    beatStatus.setText (selectedMode == 0
        ? (processor.getInputLevel() > 0.0001f ? "INPUT: ACTIVE" : "INPUT: NO SIGNAL")
        : "Run in Beat Only mode", juce::dontSendNotification);
    vocalStatus.setText (selectedMode == 1
        ? (processor.getVocalInputLevel() > 0.0001f ? "INPUT: ACTIVE" : "INPUT: NO SIGNAL")
        : "Run in Vocal Only mode", juce::dontSendNotification);
    beatResultStatus.setText (beatSaved ? "BEAT RESULT: SAVED" : "BEAT RESULT: NOT ANALYZED", juce::dontSendNotification);
    vocalResultStatus.setText (vocalSaved ? "VOCAL RESULT: SAVED" : "VOCAL RESULT: NOT ANALYZED", juce::dontSendNotification);
    recommendationStatus.setText (beatSaved && vocalSaved ? "RECOMMENDATION: READY" : "RECOMMENDATION: NOT READY", juce::dontSendNotification);

    if (selectedMode == 1)
    {
        keyLabel.setText ("Key: beat pass not running", juce::dontSendNotification);
        bpmLabel.setText ("Detected Audio BPM: —", juce::dontSendNotification);
        confidenceLabel.setText ("Beat confidence: —", juce::dontSendNotification);
        notesLabel.setText ("Scale notes: analyze and save a beat pass", juce::dontSendNotification);
        beatMetrics.setText ("Project BPM is reference metadata only.", juce::dontSendNotification);
    }
    else if ((selectedMode == 2 && beatSaved) || processor.hasStableDetection())
    {
        juce::String notes = "Scale: ";
        for (int i = 0; i < 7; ++i)
            notes += juce::String (noteNames[static_cast<size_t> ((key + scale[static_cast<size_t> (i)]) % 12)]) + (i == 6 ? "" : "  ");
        keyLabel.setText (juce::String ("Key: ") + noteNames[static_cast<size_t> (key)] + (scaleMode == 0 ? " Major" : " Minor"), juce::dontSendNotification);
        bpmLabel.setText ("Detected BPM: " + juce::String (bpm, 2) + "   |   Project: " + juce::String (processor.getHostBpm(), 2), juce::dontSendNotification);
        confidenceLabel.setText ("Key confidence " + juce::String (keyConfidence * 100.0f, 0) + "%   |   BPM confidence " + juce::String (bpmConfidence * 100.0f, 0) + "%", juce::dontSendNotification);
        notesLabel.setText (notes, juce::dontSendNotification);
        beatMetrics.setText ("RMS " + juce::String (processor.getBeatRms(), 3) + "   |   Frames " + juce::String (processor.getAnalysisFrames()) + "   |   Duration " + juce::String (processor.getAnalysisDuration(), 1) + "s", juce::dontSendNotification);
    }
    else if (selectedMode == 0 && bpm > 0.0)
    {
        keyLabel.setText ("Key: Uncertain — no scale is locked", juce::dontSendNotification);
        bpmLabel.setText ("Audio BPM candidate: " + juce::String (bpm, 2)
            + (alternativeBpm > 0.0 ? "  |  Alternative: " + juce::String (alternativeBpm, 2) : "")
            + "  |  Project: " + juce::String (processor.getHostBpm(), 2), juce::dontSendNotification);
        confidenceLabel.setText ("Key " + juce::String (keyConfidence * 100.0f, 0)
            + "%  |  BPM " + juce::String (bpmConfidence * 100.0f, 0)
            + "% — capture cleaner or longer audio before saving.", juce::dontSendNotification);
        notesLabel.setText ("No Auto-Tune key or scale is recommended while the beat result is uncertain.", juce::dontSendNotification);
        beatMetrics.setText ("Audio-derived candidate only; the displayed Project BPM is reference metadata, not detection.", juce::dontSendNotification);
    }
    else
    {
        keyLabel.setText ("Key: Uncertain", juce::dontSendNotification);
        bpmLabel.setText ("Detected Audio BPM: —", juce::dontSendNotification);
        confidenceLabel.setText ("Analyze a beat pass with more audio.", juce::dontSendNotification);
        notesLabel.setText ("No scale is recommended until beat confidence is adequate.", juce::dontSendNotification);
        beatMetrics.setText ("Waiting for a clean beat-only capture.", juce::dontSendNotification);
    }

    const auto vocalConfidence = selectedMode == 2 && vocalSaved ? processor.getSavedVocalConfidence() : processor.getVocalConfidence();
    const auto vocalLow = selectedMode == 2 && vocalSaved ? processor.getSavedVocalLowestMidi() : processor.getVocalLowestMidi();
    const auto vocalHigh = selectedMode == 2 && vocalSaved ? processor.getSavedVocalHighestMidi() : processor.getVocalHighestMidi();
    if ((selectedMode == 2 && vocalSaved) || (selectedMode == 1 && vocalConfidence > 0.0f))
    {
        vocalMetrics.setText ("Range: " + juce::String (vocalLow, 0) + "–" + juce::String (vocalHigh, 0) + " MIDI\n"
            + "Pitch confidence: " + juce::String (vocalConfidence * 100.0f, 0) + "%   |   Voiced: " + juce::String (processor.getVocalVoicedPercent() * 100.0f, 0) + "%\n"
            + "Sustained: " + juce::String ((selectedMode == 2 ? processor.getSavedVocalSustainedPercent() : processor.getVocalSustainedPercent()) * 100.0f, 0) + "%   |   Changes: "
            + juce::String (selectedMode == 2 ? processor.getSavedVocalNoteChangeSpeed() : processor.getVocalNoteChangeSpeed(), 1) + "/sec\n"
            + ((selectedMode == 2 ? processor.getSavedVocalMelodic() : processor.isVocalMelodic()) ? "Delivery: melodic" : "Delivery: spoken / rap"), juce::dontSendNotification);
    }
    else
    {
        vocalMetrics.setText ("Mute the beat, select Vocal Only, then capture the isolated lead vocal.\nNo vocal range or correction settings are invented before a reliable vocal pass.", juce::dontSendNotification);
    }

    refreshRecommendation();
}

void KeyBridgeAudioProcessorEditor::refreshRecommendation()
{
    const auto combined = processor.getAnalysisMode() == 2;
    if (! combined)
    {
        guidanceLabel.setText (processor.getAnalysisMode() == 0
            ? "Beat Only: mute vocals, press Analyze, then save the beat result."
            : "Vocal Only: mute the beat, press Analyze, then save the vocal result.", juce::dontSendNotification);
        settingsLabel.setText ("Combined recommendations are intentionally unavailable while a single pass is active.", juce::dontSendNotification);
        return;
    }

    if (! processor.hasSavedBeatResult() || ! processor.hasSavedVocalResult())
    {
        guidanceLabel.setText ("Analyze and save the vocal result and beat result separately first.", juce::dontSendNotification);
        settingsLabel.setText ("TuneRite will not invent a key, BPM, vocal range, or Auto-Tune setting without both saved results.", juce::dontSendNotification);
        return;
    }

    const auto key = juce::jlimit (0, 11, processor.getSavedBeatKey());
    const auto scaleMode = processor.getSavedBeatMode();
    const auto melodic = processor.getSavedVocalMelodic();
    const auto sustained = processor.getSavedVocalSustainedPercent();
    const auto changeRate = processor.getSavedVocalNoteChangeSpeed();
    const auto hardTune = vibeBox.getText() == "Hard Tune";
    const auto retune = hardTune ? 5 : (melodic ? 38 : 18);
    const auto humanize = juce::jlimit (5, 55, static_cast<int> (10.0f + sustained * 45.0f));
    const auto flexTune = hardTune ? 8 : (melodic ? 42 : 18);
    const auto processing = deliveryBox.getText() == "Sung" ? "HQ" : "Low Latency";
    const auto modeText = hardTune ? "Classic" : "Modern";

    guidanceLabel.setText ("Starting point based on saved beat key and saved vocal behavior. Genre and vibe adjust style only; they do not alter detected key or BPM.", juce::dontSendNotification);
    settingsLabel.setText (juce::String ("Auto-Tune: ") + noteNames[static_cast<size_t> (key)] + (scaleMode == 0 ? " Major" : " Minor")
        + "  |  Range " + juce::String (processor.getSavedVocalLowestMidi(), 0) + "–" + juce::String (processor.getSavedVocalHighestMidi(), 0) + " MIDI"
        + "  |  Retune " + juce::String (retune) + " ms"
        + "  |  Humanize " + juce::String (humanize)
        + "  |  Flex-Tune " + juce::String (flexTune)
        + "  |  " + modeText + " / " + processing
        + "  |  Note-change rate " + juce::String (changeRate, 1) + "/sec", juce::dontSendNotification);
}

void KeyBridgeAudioProcessorEditor::updateModeControls()
{
    const auto mode = processor.getAnalysisMode();
    analyzeButton.setButtonText (mode == 0 ? "ANALYZE BEAT" : mode == 1 ? "ANALYZE VOCAL" : "RESULTS READY MODE");
    saveButton.setButtonText (mode == 0 ? "SAVE BEAT RESULT" : mode == 1 ? "SAVE VOCAL RESULT" : "SAVED RESULTS ONLY");
    analyzeButton.setEnabled (mode != 2);
    saveButton.setEnabled (mode != 2);
}

void KeyBridgeAudioProcessorEditor::copySettings()
{
    if (! processor.hasSavedBeatResult() || ! processor.hasSavedVocalResult())
    {
        settingsLabel.setText ("Save both beat and vocal results before copying Auto-Tune recommendations.", juce::dontSendNotification);
        return;
    }

    const auto key = juce::jlimit (0, 11, processor.getSavedBeatKey());
    const auto settings = juce::String ("TuneRite Auto-Tune Recommendation\n")
        + juce::String ("Key: ") + juce::String (noteNames[static_cast<size_t> (key)]) + (processor.getSavedBeatMode() == 0 ? " Major\n" : " Minor\n")
        + "Detected BPM: " + juce::String (processor.getSavedBeatBpm(), 2) + "\n"
        + "Vocal Range: " + juce::String (processor.getSavedVocalLowestMidi(), 0) + "-" + juce::String (processor.getSavedVocalHighestMidi(), 0) + " MIDI\n"
        + "Beat Confidence: " + juce::String (processor.getSavedBeatKeyConfidence() * 100.0f, 0) + "% key / " + juce::String (processor.getSavedBeatBpmConfidence() * 100.0f, 0) + "% BPM\n"
        + "Vocal Confidence: " + juce::String (processor.getSavedVocalConfidence() * 100.0f, 0) + "%\n"
        + "Recommendation: starting point only; TuneRite does not control Auto-Tune directly.";
    juce::SystemClipboard::copyTextToClipboard (settings);
    settingsLabel.setText ("Recommendation copied to clipboard.", juce::dontSendNotification);
}

void KeyBridgeAudioProcessorEditor::copyDetectedBpm()
{
    const auto hasSavedBeat = processor.hasSavedBeatResult();
    const auto bpm = hasSavedBeat ? processor.getSavedBeatBpm() : processor.getDetectedBpm();
    if (bpm <= 0.0 || (! hasSavedBeat && ! processor.hasStableDetection()))
    {
        guidanceLabel.setText ("No stable BPM is available to copy. Run and save a confident Beat Only pass first.", juce::dontSendNotification);
        return;
    }

    juce::SystemClipboard::copyTextToClipboard (juce::String (bpm, 2));
    guidanceLabel.setText ("Detected BPM copied. Paste it into FL Studio’s tempo field if you choose to match the project.", juce::dontSendNotification);
}
