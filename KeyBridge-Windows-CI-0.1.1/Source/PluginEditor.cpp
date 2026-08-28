#include "PluginEditor.h"
#include "BinaryData.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    constexpr std::array<const char*, 12> noteNames { { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" } };
    constexpr std::array<int, 7> majorScale { { 0, 2, 4, 5, 7, 9, 11 } };
    constexpr std::array<int, 7> minorScale { { 0, 2, 3, 5, 7, 8, 10 } };
    const juce::Colour red { 0xffff3946 };
    const juce::Colour dimRed { 0xffa2101a };
    const juce::Colour charcoal { 0xff0b0b0d };
    const juce::Colour panel { 0xff101013 };
    const juce::Colour silver { 0xffe9e9ec };
    const juce::Colour muted { 0xff96969d };

    juce::String confidenceText (float confidence)
    {
        return juce::String (juce::jlimit (0.0f, 100.0f, confidence * 100.0f), 0) + "%";
    }

    void addOptions (juce::ComboBox& box, std::initializer_list<const char*> values)
    {
        int id = 1;
        for (const auto* value : values)
            box.addItem (value, id++);
    }

    void drawHardwarePanel (juce::Graphics& g, juce::Rectangle<float> bounds, bool active = false)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour ((active ? red : juce::Colour (0xff49494e)).withAlpha (active ? 0.88f : 0.78f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, active ? 1.35f : 0.8f);
        if (active)
        {
            g.setColour (red.withAlpha (0.13f));
            g.drawRoundedRectangle (bounds.expanded (3.0f), 12.0f, 3.5f);
        }
    }

    void drawMeter (juce::Graphics& g, juce::Rectangle<float> bounds, float percentage)
    {
        g.setColour (juce::Colour (0xff050506));
        g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
        g.setColour (juce::Colour (0xff48484d));
        g.drawRoundedRectangle (bounds, bounds.getHeight() * 0.5f, 0.7f);
        const auto filled = bounds.withWidth (bounds.getWidth() * juce::jlimit (0.0f, 1.0f, percentage));
        g.setColour (red);
        g.fillRoundedRectangle (filled, bounds.getHeight() * 0.5f);
    }
}

KeyBridgeAudioProcessorEditor::KeyBridgeAudioProcessorEditor (KeyBridgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
    brandLogo = juce::ImageCache::getFromMemory (BinaryData::MurderMittenMediaLogo_png,
                                                  BinaryData::MurderMittenMediaLogo_pngSize);
    setResizable (true, true);
    setResizeLimits (1100, 720, 1800, 1120);
    setSize (1420, 820);

    addLabel (title, 31.0f, juce::Justification::centredLeft, silver);
    addLabel (subtitle, 12.0f, juce::Justification::centredLeft, muted);
    addLabel (analysisStatus, 11.0f, juce::Justification::centredRight, muted);
    addLabel (footerStatus, 11.0f, juce::Justification::centredLeft, muted);

    for (auto* label : { &bpmCaption, &keyCaption, &confidenceCaption, &targetGenderCaption,
                         &targetGenreCaption, &targetMoodCaption, &summaryKeyCaption,
                         &summaryScaleCaption, &similarNotesCaption, &inputTypeCaption,
                         &summaryConfidenceCaption, &settingsHeading, &inKeyHeading,
                         &lowestCaption, &comfortCaption, &guidanceHeading })
        addLabel (*label, 11.0f, juce::Justification::centredLeft, red);

    for (auto* label : { &bpmValue, &keyValue, &scaleValue, &confidenceValue,
                         &summaryKeyValue, &summaryScaleValue, &similarNotesValue,
                         &inputTypeValue, &summaryConfidenceValue, &lowestValue, &comfortValue })
        addLabel (*label, 25.0f, juce::Justification::centredLeft, silver);

    addLabel (inKeyNotes, 17.0f, juce::Justification::centredLeft, silver);
    addLabel (guidanceText, 13.0f, juce::Justification::topLeft, juce::Colour (0xffcfcfd3));
    guidanceText.setJustificationType (juce::Justification::topLeft);

    for (auto& label : settingCaptions)
        addLabel (label, 10.0f, juce::Justification::centred, muted);
    for (auto& label : settingValues)
        addLabel (label, 27.0f, juce::Justification::centred, silver);

    bpmCaption.setText ("BPM  (i)", juce::dontSendNotification);
    keyCaption.setText ("KEY / SCALE  (i)", juce::dontSendNotification);
    confidenceCaption.setText ("CONFIDENCE  (i)", juce::dontSendNotification);
    targetGenderCaption.setText ("GENDER  (i)", juce::dontSendNotification);
    targetGenreCaption.setText ("GENRE  (i)", juce::dontSendNotification);
    targetMoodCaption.setText ("MOOD  (i)", juce::dontSendNotification);
    summaryKeyCaption.setText ("DETECTED KEY  (i)", juce::dontSendNotification);
    summaryScaleCaption.setText ("SCALE  (i)", juce::dontSendNotification);
    similarNotesCaption.setText ("SIMILAR NOTES  (i)", juce::dontSendNotification);
    inputTypeCaption.setText ("INPUT TYPE  (i)", juce::dontSendNotification);
    summaryConfidenceCaption.setText ("CONFIDENCE  (i)", juce::dontSendNotification);
    settingsHeading.setText ("RECOMMENDED AUTO-TUNE SETTINGS", juce::dontSendNotification);
    inKeyHeading.setText ("IN KEY NOTES", juce::dontSendNotification);
    lowestCaption.setText ("LOWEST NOTE  (i)", juce::dontSendNotification);
    comfortCaption.setText ("COMFORT ZONE  (i)", juce::dontSendNotification);
    guidanceHeading.setText ("QUICK GUIDANCE", juce::dontSendNotification);
    const std::array<const char*, 6> settingNames { { "KEY  (i)", "SCALE  (i)", "RETUNE SPEED  (i)", "HUMANIZE  (i)", "FLEX TUNE  (i)", "FORMANT  (i)" } };
    for (size_t index = 0; index < settingNames.size(); ++index)
        settingCaptions[index].setText (settingNames[index], juce::dontSendNotification);

    for (auto* button : { &analyzePageButton, &recommendationsPageButton, &helpButton,
                          &beatInputButton, &vocalInputButton, &combinedInputButton,
                          &analyzeButton, &clearButton, &copySettingsButton })
        addButton (*button, button->getButtonText());

    analyzePageButton.onClick = [this] { setPage (0); };
    recommendationsPageButton.onClick = [this] { setPage (1); };
    helpButton.setTooltip ("TuneRite reads routed audio only. Analyze a Beat or Vocal capture, then copy recommendation-only Auto-Tune starting settings.");
    beatInputButton.setTooltip ("Capture routed beat audio. Key and BPM are measured from audio, not host tempo.");
    vocalInputButton.setTooltip ("Capture isolated vocal audio. Vocal evidence informs recommendation settings only.");
    combinedInputButton.setTooltip ("Open recommendations from independently saved beat and vocal results. It does not analyze a mixed signal for vocal range.");
    beatInputButton.onClick = [this] { selectInput (0); };
    vocalInputButton.onClick = [this] { selectInput (1); };
    combinedInputButton.onClick = [this] { selectInput (2); };
    analyzeButton.onClick = [this]
    {
        if (selectedInput == 2) { setPage (1); return; }
        if (processor.getCaptureState() == 2) processor.finishCapture();
        else processor.startFreshAnalysis();
    };
    clearButton.onClick = [this]
    {
        if (selectedInput == 0) processor.clearBeatResult();
        else if (selectedInput == 1) processor.clearVocalResult();
        else processor.resetAllResults();
        refreshView();
    };
    copySettingsButton.onClick = [this] { copySettings(); };

    addOptions (profileBox, { "Male", "Female" });
    addOptions (genreBox, { "Rap", "Melodic Rap", "R&B", "Pop", "Trap", "Drill", "Rock", "Alternative" });
    addOptions (vibeBox, { "Melodic", "Natural", "Aggressive", "Emotional", "Smooth", "Energetic" });
    profileBox.setSelectedId (1, juce::dontSendNotification);
    genreBox.setSelectedId (1, juce::dontSendNotification);
    vibeBox.setSelectedId (1, juce::dontSendNotification);
    for (auto* box : { &profileBox, &genreBox, &vibeBox })
    {
        box->setColour (juce::ComboBox::backgroundColourId, charcoal);
        box->setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff4a4a50));
        box->setColour (juce::ComboBox::textColourId, silver);
        box->setColour (juce::ComboBox::arrowColourId, red);
        box->onChange = [this] { refreshView(); };
        addAndMakeVisible (*box);
    }
    profileBox.setTooltip ("Helps TuneRite tailor recommendations. It never changes measured musical key or BPM.");
    genreBox.setTooltip ("Changes recommendation style only. It never changes measured musical key or BPM.");
    vibeBox.setTooltip ("Changes recommendation style only. It never changes measured musical key or BPM.");
    similarNotesValue.setTooltip ("Scale notes near the detected tonal center that may work well melodically.");
    settingCaptions[2].setTooltip ("Suggested Auto-Tune correction speed. Lower values create faster, harder tuning.");
    settingCaptions[3].setTooltip ("Helps sustained notes retain more natural pitch movement.");
    settingCaptions[4].setTooltip ("Allows notes closer to the correct pitch to remain less corrected.");
    settingCaptions[5].setTooltip ("Preserves vocal character when pitch correction is applied.");
    lowestValue.setTooltip ("Lowest stable vocal note detected during vocal analysis.");
    comfortValue.setTooltip ("Approximate pitch area where the vocal spent the most usable time.");

    processor.setAnalysisMode (0);
    selectInput (0);
    juce::MessageManager::callAsync ([editor = juce::Component::SafePointer<KeyBridgeAudioProcessorEditor> (this)]
    {
        if (editor != nullptr) editor->ensureUiTimerRunning();
    });
}

void KeyBridgeAudioProcessorEditor::addLabel (juce::Label& label, float size, juce::Justification justification, juce::Colour colour)
{
    label.setFont (juce::Font (size, juce::Font::bold));
    label.setJustificationType (justification);
    label.setColour (juce::Label::textColourId, colour);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

void KeyBridgeAudioProcessorEditor::addButton (juce::TextButton& button, const juce::String& text)
{
    button.setButtonText (text);
    button.setColour (juce::TextButton::buttonColourId, charcoal);
    button.setColour (juce::TextButton::buttonOnColourId, dimRed);
    button.setColour (juce::TextButton::textColourOffId, silver);
    button.setColour (juce::TextButton::textColourOnId, silver);
    addAndMakeVisible (button);
}

juce::Colour KeyBridgeAudioProcessorEditor::accentColour() const { return red; }
juce::Colour KeyBridgeAudioProcessorEditor::panelColour() const { return panel; }

juce::Colour KeyBridgeAudioProcessorEditor::statusColour (const juce::String& status) const
{
    if (status.containsIgnoreCase ("ERROR") || status.containsIgnoreCase ("FAILED") || status.containsIgnoreCase ("NO SIGNAL")) return red;
    if (status.containsIgnoreCase ("UNCERTAIN") || status.containsIgnoreCase ("NEED") || status.containsIgnoreCase ("UNAVAILABLE")) return juce::Colour (0xffffb454);
    if (status.containsIgnoreCase ("SAVED") || status.containsIgnoreCase ("READY")) return juce::Colour (0xfff2f2f5);
    return muted;
}

juce::String KeyBridgeAudioProcessorEditor::rootName (int root) const
{
    return root >= 0 && root < 12 ? juce::String (noteNames[static_cast<size_t> (root)]) : "--";
}

juce::String KeyBridgeAudioProcessorEditor::keyName (int root, int mode) const
{
    if (root < 0 || root >= 12 || (mode != 0 && mode != 1)) return "UNKNOWN";
    return rootName (root) + (mode == 0 ? " Major" : " Minor");
}

juce::String KeyBridgeAudioProcessorEditor::midiName (float midi) const
{
    if (midi <= 0.0f) return "--";
    const auto rounded = static_cast<int> (std::round (midi));
    return rootName ((rounded % 12 + 12) % 12) + juce::String (rounded / 12 - 1);
}

juce::String KeyBridgeAudioProcessorEditor::scaleNotes (int root, int mode, bool similarOnly) const
{
    if (root < 0 || root >= 12 || (mode != 0 && mode != 1)) return "--";
    const auto& scale = mode == 0 ? majorScale : minorScale;
    const std::array<int, 4> indices { { 0, 2, 4, 5 } };
    juce::String text;
    const auto count = similarOnly ? 4 : 7;
    for (int index = 0; index < count; ++index)
    {
        const auto degree = similarOnly ? indices[static_cast<size_t> (index)] : index;
        text += rootName ((root + scale[static_cast<size_t> (degree)]) % 12);
        if (index + 1 < count) text += similarOnly ? "     " : "   ";
    }
    return text;
}

tunerite::VocalFitRecommendation KeyBridgeAudioProcessorEditor::currentVocalFit() const
{
    tunerite::VocalFitRequest request;
    request.beatKeyValid = processor.hasSavedBeatKey();
    request.beatKeyRoot = processor.getSavedBeatKey();
    request.beatKeyMode = processor.getSavedBeatMode();
    request.beatTempoValid = processor.hasSavedBeatTempo();
    request.beatBpm = processor.getSavedBeatBpm();
    request.vocalValid = processor.hasValidVocalResult();
    request.vocalMelodic = processor.getSavedVocalMelodic();
    request.vocalLowMidi = processor.getSavedVocalLowestMidi();
    request.vocalHighMidi = processor.getSavedVocalHighestMidi();
    request.vocalAverageMidi = processor.getSavedVocalAverageMidi();
    request.vocalConfidence = processor.getSavedVocalConfidence();
    request.vocalSustainedPercent = processor.getSavedVocalSustainedPercent();
    request.vocalNoteChangeRate = processor.getSavedVocalNoteChangeSpeed();
    request.voiceProfile = profileBox.getText() == "Female" ? "Female hint" : "Male hint";
    request.genre = genreBox.getText().toStdString();
    request.delivery = (genreBox.getText().containsIgnoreCase ("rap") || genreBox.getText() == "Trap" || genreBox.getText() == "Drill") ? "Rap" : "Sung";
    request.mood = vibeBox.getText() == "Aggressive" ? "Hard tune" : vibeBox.getText().toStdString();
    return tunerite::VocalFit::recommend (request);
}

void KeyBridgeAudioProcessorEditor::ensureUiTimerRunning()
{
    if (! isTimerRunning()) startTimerHz (10);
}

void KeyBridgeAudioProcessorEditor::parentHierarchyChanged()
{
    if (isShowing()) ensureUiTimerRunning();
}

void KeyBridgeAudioProcessorEditor::visibilityChanged()
{
    if (isShowing()) ensureUiTimerRunning(); else stopTimer();
}

void KeyBridgeAudioProcessorEditor::setPage (int page)
{
    activePage = juce::jlimit (0, 1, page);
    resized();
    refreshView();
}

void KeyBridgeAudioProcessorEditor::selectInput (int mode)
{
    selectedInput = juce::jlimit (0, 2, mode);
    processor.setAnalysisMode (selectedInput == 2 ? 2 : selectedInput);
    processor.setAnalysisEnabled (true);
    activePage = selectedInput == 2 ? 1 : 0;
    resized();
    refreshView();
}

void KeyBridgeAudioProcessorEditor::timerCallback()
{
    std::move (levelHistory.begin() + 1, levelHistory.end(), levelHistory.begin());
    levelHistory.back() = processor.getInputLevel();
    refreshView();
}

void KeyBridgeAudioProcessorEditor::refreshView()
{
    const auto captureState = processor.getCaptureState();
    const auto capturing = captureState == 1 || captureState == 2 || captureState == 3;
    const auto savedTempo = processor.hasSavedBeatTempo();
    const auto savedKey = processor.hasSavedBeatKey();
    const auto savedVocal = processor.hasValidVocalResult();
    const auto root = savedKey ? processor.getSavedBeatKey() : -1;
    const auto mode = savedKey ? processor.getSavedBeatMode() : -1;
    const auto keyConfidence = savedKey ? processor.getSavedBeatKeyConfidence() : 0.0f;
    const auto bpmConfidence = savedTempo ? processor.getSavedBeatBpmConfidence() : 0.0f;
    const auto displayConfidence = savedKey ? keyConfidence : bpmConfidence;
    const auto vocalFit = currentVocalFit();
    const auto analyzerWarning = processor.getLastBeatAnalysisWarning();

    juce::String status;
    switch (captureState)
    {
        case 1: status = "ARMED — WAITING FOR AUDIO"; break;
        case 2: status = processor.isCaptureFinishRequested() ? "FINISH REQUESTED" : "CAPTURING ROUTED AUDIO"; break;
        case 3: status = "PROCESSING LOCAL ANALYZER"; break;
        case 4: status = "NO SIGNAL — KEEP PLAYING"; break;
        case 5: status = "NEED 12 SECONDS BEFORE FINISH"; break;
        case 6: status = "CAPTURE CANCELLED"; break;
        default: status = analyzerWarning.isNotEmpty() && ! processor.hasSavedBeatResult() ? analyzerWarning : "READY"; break;
    }
    analysisStatus.setText ("STATUS: " + status, juce::dontSendNotification);
    analysisStatus.setColour (juce::Label::textColourId, statusColour (status));
    footerStatus.setText ("STATUS: " + status, juce::dontSendNotification);
    footerStatus.setColour (juce::Label::textColourId, statusColour (status));

    bpmValue.setText (savedTempo ? juce::String (processor.getSavedBeatBpm(), 1) : "--", juce::dontSendNotification);
    keyValue.setText (rootName (root), juce::dontSendNotification);
    scaleValue.setText (mode == 0 ? "MAJOR" : mode == 1 ? "MINOR" : "--", juce::dontSendNotification);
    confidenceValue.setText ((savedKey || savedTempo) ? confidenceText (displayConfidence) : "--", juce::dontSendNotification);

    summaryKeyValue.setText (rootName (root), juce::dontSendNotification);
    summaryScaleValue.setText (mode == 0 ? "MAJOR" : mode == 1 ? "MINOR" : "--", juce::dontSendNotification);
    similarNotesValue.setText (scaleNotes (root, mode, true), juce::dontSendNotification);
    inputTypeValue.setText (savedVocal ? midiName (processor.getSavedVocalAverageMidi()) + " TARGET" : "--", juce::dontSendNotification);
    summaryConfidenceValue.setText ((savedKey || savedTempo) ? confidenceText (displayConfidence) : "--", juce::dontSendNotification);
    inKeyNotes.setText (scaleNotes (root, mode, false), juce::dontSendNotification);

    settingValues[0].setText (vocalFit.ready ? rootName (vocalFit.keyRoot) : "--", juce::dontSendNotification);
    settingValues[1].setText (vocalFit.ready ? (vocalFit.keyMode == 0 ? "MAJOR" : "MINOR") : "--", juce::dontSendNotification);
    settingValues[2].setText (vocalFit.ready ? juce::String (vocalFit.retuneSpeedMs) : "--", juce::dontSendNotification);
    settingValues[3].setText (vocalFit.ready ? juce::String (vocalFit.humanize) : "--", juce::dontSendNotification);
    settingValues[4].setText (vocalFit.ready ? juce::String (vocalFit.flexTune) : "--", juce::dontSendNotification);
    // Formant is a general starting recommendation, not a detected property or external plugin control.
    settingValues[5].setText (vocalFit.ready ? "ON" : "--", juce::dontSendNotification);

    lowestValue.setText (savedVocal ? midiName (processor.getSavedVocalLowestMidi()) : "--", juce::dontSendNotification);
    comfortValue.setText (savedVocal ? midiName (processor.getSavedVocalAverageMidi()) : "--", juce::dontSendNotification);
    if (vocalFit.ready)
    {
        guidanceText.setText ("• Start with Retune Speed " + juce::String (vocalFit.retuneSpeedMs) + ".\n"
                              "• Use Humanize " + juce::String (vocalFit.humanize) + " and Flex Tune " + juce::String (vocalFit.flexTune) + ".\n"
                              "• " + juce::String (vocalFit.keyScale) + " is the measured tuning key.", juce::dontSendNotification);
    }
    else
    {
        guidanceText.setText ("• Save a valid beat key before selecting a tuning key.\n"
                              "• Save isolated vocal evidence for range-sensitive settings.\n"
                              "• Target controls adjust recommendations only.", juce::dontSendNotification);
    }

    analyzePageButton.setToggleState (activePage == 0, juce::dontSendNotification);
    recommendationsPageButton.setToggleState (activePage == 1, juce::dontSendNotification);
    beatInputButton.setToggleState (selectedInput == 0, juce::dontSendNotification);
    vocalInputButton.setToggleState (selectedInput == 1, juce::dontSendNotification);
    combinedInputButton.setToggleState (selectedInput == 2, juce::dontSendNotification);
    analyzeButton.setButtonText (selectedInput == 2 ? "OPEN RECOMMENDATIONS" : captureState == 2 ? "STOP AND SAVE" : "ANALYZE");
    analyzeButton.setEnabled (selectedInput == 2 || captureState != 3);
    clearButton.setEnabled (selectedInput == 0 ? processor.hasSavedBeatResult() : selectedInput == 1 ? processor.hasSavedVocalResult() : (savedTempo || savedKey || savedVocal));
    copySettingsButton.setEnabled (vocalFit.ready);

    const bool showAnalyze = activePage == 0;
    for (auto* component : { static_cast<juce::Component*> (&bpmCaption), static_cast<juce::Component*> (&bpmValue),
                             static_cast<juce::Component*> (&keyCaption), static_cast<juce::Component*> (&keyValue),
                             static_cast<juce::Component*> (&scaleValue), static_cast<juce::Component*> (&confidenceCaption),
                             static_cast<juce::Component*> (&confidenceValue), static_cast<juce::Component*> (&beatInputButton),
                             static_cast<juce::Component*> (&vocalInputButton), static_cast<juce::Component*> (&combinedInputButton),
                             static_cast<juce::Component*> (&analyzeButton), static_cast<juce::Component*> (&clearButton) })
        component->setVisible (showAnalyze);

    for (auto* component : { static_cast<juce::Component*> (&targetGenderCaption), static_cast<juce::Component*> (&targetGenreCaption),
                             static_cast<juce::Component*> (&targetMoodCaption), static_cast<juce::Component*> (&profileBox),
                             static_cast<juce::Component*> (&genreBox), static_cast<juce::Component*> (&vibeBox),
                             static_cast<juce::Component*> (&summaryKeyCaption), static_cast<juce::Component*> (&summaryKeyValue),
                             static_cast<juce::Component*> (&summaryScaleCaption), static_cast<juce::Component*> (&summaryScaleValue),
                             static_cast<juce::Component*> (&similarNotesCaption), static_cast<juce::Component*> (&similarNotesValue),
                             static_cast<juce::Component*> (&inputTypeCaption), static_cast<juce::Component*> (&inputTypeValue),
                             static_cast<juce::Component*> (&summaryConfidenceCaption), static_cast<juce::Component*> (&summaryConfidenceValue),
                             static_cast<juce::Component*> (&settingsHeading), static_cast<juce::Component*> (&inKeyHeading),
                             static_cast<juce::Component*> (&inKeyNotes), static_cast<juce::Component*> (&lowestCaption),
                             static_cast<juce::Component*> (&lowestValue), static_cast<juce::Component*> (&comfortCaption),
                             static_cast<juce::Component*> (&comfortValue), static_cast<juce::Component*> (&guidanceHeading),
                             static_cast<juce::Component*> (&guidanceText), static_cast<juce::Component*> (&copySettingsButton) })
        component->setVisible (! showAnalyze);
    for (auto& label : settingCaptions) label.setVisible (! showAnalyze);
    for (auto& label : settingValues) label.setVisible (! showAnalyze);

    juce::ignoreUnused (capturing);
    repaint();
}

void KeyBridgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto w = getWidth();
    const auto h = getHeight();
    const auto margin = 34;
    g.fillAll (juce::Colour (0xff040405));
    juce::ColourGradient backdrop (juce::Colour (0xff121015), 0.0f, 0.0f, juce::Colour (0xff030304), 0.0f, static_cast<float> (h), false);
    g.setGradientFill (backdrop);
    g.fillRect (getLocalBounds());
    g.setColour (juce::Colour (0xff2d2d33));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (10.0f), 14.0f, 0.8f);

    if (brandLogo.isValid())
        g.drawImageWithin (brandLogo, margin, 20, 88, 88, juce::RectanglePlacement::centred, false);
    g.setColour (red.withAlpha (0.35f));
    g.drawEllipse (juce::Rectangle<float> (static_cast<float> (margin - 6), 14.0f, 100.0f, 100.0f), 1.2f);

    const auto nav = juce::Rectangle<float> (static_cast<float> (w / 2 - 330), 26.0f, 660.0f, 62.0f);
    g.setColour (juce::Colour (0xff0a0a0c));
    g.fillRoundedRectangle (nav, 10.0f);
    g.setColour (juce::Colour (0xff4b3034));
    g.drawRoundedRectangle (nav, 10.0f, 0.9f);
    const auto navSplit = nav.getX() + nav.getWidth() * 0.39f;
    g.setColour (juce::Colour (0xff64646a));
    g.drawLine (navSplit, nav.getY() + 16.0f, navSplit, nav.getBottom() - 16.0f, 0.7f);
    g.setColour (red);
    const auto activeStart = activePage == 0 ? nav.getX() + 24.0f : navSplit + 22.0f;
    const auto activeEnd = activePage == 0 ? navSplit - 34.0f : nav.getRight() - 24.0f;
    g.drawLine (activeStart, nav.getBottom() - 2.0f, activeEnd, nav.getBottom() - 2.0f, 2.5f);

    if (activePage == 0)
    {
        const auto summary = juce::Rectangle<float> (static_cast<float> (margin + 40), 132.0f, static_cast<float> (w - (margin + 40) * 2), 170.0f);
        drawHardwarePanel (g, summary, true);
        const auto section = summary.getWidth() / 3.0f;
        g.setColour (juce::Colour (0xff4c4c52));
        g.drawLine (summary.getX() + section, summary.getY() + 28.0f, summary.getX() + section, summary.getBottom() - 28.0f, 0.75f);
        g.drawLine (summary.getX() + section * 2.0f, summary.getY() + 28.0f, summary.getX() + section * 2.0f, summary.getBottom() - 28.0f, 0.75f);

        const auto selector = juce::Rectangle<float> (static_cast<float> (margin + 40), 332.0f, static_cast<float> (w - (margin + 40) * 2), 268.0f);
        drawHardwarePanel (g, selector);
        g.setColour (red);
        g.setFont (juce::Font (14.0f, juce::Font::bold));
        g.drawFittedText ("SELECT INPUT", selector.getCentreX() - 90.0f, selector.getY() + 10.0f, 180.0f, 22.0f, juce::Justification::centred, 1);
        const auto gap = 36.0f;
        const auto cardW = (selector.getWidth() - 88.0f - gap * 2.0f) / 3.0f;
        for (int card = 0; card < 3; ++card)
        {
            const auto bounds = juce::Rectangle<float> (selector.getX() + 44.0f + card * (cardW + gap), selector.getY() + 50.0f, cardW, 204.0f);
            drawHardwarePanel (g, bounds, selectedInput == card);
        }
        g.setColour (red.withAlpha (0.80f));
        for (int i = 0; i < 5; ++i)
        {
            const auto x = selector.getX() + 44.0f + cardW * 0.5f - 24.0f + i * 12.0f;
            const auto height = 20.0f + static_cast<float> ((i % 3) * 12);
            g.drawLine (x, selector.getY() + 88.0f - height * 0.5f, x, selector.getY() + 88.0f + height * 0.5f, 2.5f);
        }
    }
    else
    {
        const auto controlsY = 112.0f;
        const auto controlW = 250.0f;
        const auto controlsStart = (static_cast<float> (w) - controlW * 3.0f - 28.0f) * 0.5f;
        for (int i = 0; i < 3; ++i)
            drawHardwarePanel (g, { controlsStart + i * (controlW + 14.0f), controlsY + 18.0f, controlW, 48.0f });

        const auto summary = juce::Rectangle<float> (static_cast<float> (margin + 40), 194.0f, static_cast<float> (w - (margin + 40) * 2), 112.0f);
        drawHardwarePanel (g, summary, true);
        const auto segment = summary.getWidth() / 5.0f;
        g.setColour (juce::Colour (0xff4c4c52));
        for (int i = 1; i < 5; ++i)
            g.drawLine (summary.getX() + segment * i, summary.getY() + 18.0f, summary.getX() + segment * i, summary.getBottom() - 18.0f, 0.7f);
        drawMeter (g, { summary.getX() + segment * 4.0f + 98.0f, summary.getBottom() - 28.0f, segment - 138.0f, 8.0f }, processor.hasSavedBeatKey() ? processor.getSavedBeatKeyConfidence() : processor.getSavedBeatBpmConfidence());

        const auto settings = juce::Rectangle<float> (static_cast<float> (margin + 40), 334.0f, static_cast<float> (w - (margin + 40) * 2), 160.0f);
        drawHardwarePanel (g, settings);
        g.setColour (red);
        g.setFont (juce::Font (14.0f, juce::Font::bold));
        g.drawFittedText ("RECOMMENDED AUTO-TUNE SETTINGS", settings.getCentreX() - 180.0f, settings.getY() - 11.0f, 360.0f, 22.0f, juce::Justification::centred, 1);
        const auto cardGap = 12.0f;
        const auto cardW = (settings.getWidth() - 32.0f - cardGap * 5.0f) / 6.0f;
        for (int card = 0; card < 6; ++card)
            drawHardwarePanel (g, { settings.getX() + 16.0f + card * (cardW + cardGap), settings.getY() + 30.0f, cardW, 112.0f });

        const auto bottomY = 520.0f;
        const auto bottomH = std::max (118.0f, static_cast<float> (h) - bottomY - 114.0f);
        const auto bottomW = static_cast<float> (w - (margin + 40) * 2);
        const auto leftW = bottomW * 0.34f;
        const auto middleW = bottomW * 0.30f;
        drawHardwarePanel (g, { static_cast<float> (margin + 40), bottomY, leftW, bottomH });
        drawHardwarePanel (g, { static_cast<float> (margin + 40) + leftW + 14.0f, bottomY, middleW, bottomH });
        drawHardwarePanel (g, { static_cast<float> (margin + 40) + leftW + middleW + 28.0f, bottomY, bottomW - leftW - middleW - 28.0f, bottomH });
    }

    g.setColour (juce::Colour (0xff29292e));
    g.drawLine (static_cast<float> (margin), static_cast<float> (h - 44), static_cast<float> (w - margin), static_cast<float> (h - 44), 0.8f);
    g.setColour (red.withAlpha (0.78f));
    g.drawLine (static_cast<float> (w / 2 - 112), static_cast<float> (h - 35), static_cast<float> (w / 2 + 112), static_cast<float> (h - 35), 1.4f);
}

void KeyBridgeAudioProcessorEditor::resized()
{
    const auto w = getWidth();
    const auto h = getHeight();
    const auto margin = 34;
    title.setBounds (margin + 98, 29, 290, 40);
    subtitle.setBounds (margin + 100, 70, 290, 20);
    analysisStatus.setBounds (w - 340, 38, 250, 20);
    helpButton.setBounds (w - 76, 42, 34, 34);
    analyzePageButton.setBounds (w / 2 - 318, 33, 238, 48);
    recommendationsPageButton.setBounds (w / 2 - 64, 33, 370, 48);
    footerStatus.setBounds (margin + 10, h - 34, w - margin * 2 - 20, 20);

    if (activePage == 0)
    {
        const auto summaryX = margin + 40;
        const auto summaryW = w - (margin + 40) * 2;
        const auto section = summaryW / 3;
        bpmCaption.setBounds (summaryX + 40, 160, section - 80, 20);
        bpmValue.setBounds (summaryX + 40, 195, section - 80, 72);
        keyCaption.setBounds (summaryX + section + 38, 160, section - 76, 20);
        keyValue.setBounds (summaryX + section + 38, 190, (section - 76) * 0.50f, 72);
        scaleValue.setBounds (summaryX + section + (section - 76) * 0.50f + 38, 202, (section - 76) * 0.50f, 48);
        confidenceCaption.setBounds (summaryX + section * 2 + 38, 160, section - 76, 20);
        confidenceValue.setBounds (summaryX + section * 2 + 38, 195, section - 76, 70);

        const auto selectorX = margin + 40;
        const auto selectorW = w - (margin + 40) * 2;
        const auto gap = 36;
        const auto cardW = (selectorW - 88 - gap * 2) / 3;
        for (int card = 0; card < 3; ++card)
        {
            const auto x = selectorX + 44 + card * (cardW + gap);
            auto* button = card == 0 ? &beatInputButton : card == 1 ? &vocalInputButton : &combinedInputButton;
            button->setBounds (x + 20, 450, cardW - 40, 52);
        }
        analyzeButton.setBounds (w / 2 - 180, std::min (h - 96, 626), 278, 44);
        clearButton.setBounds (w / 2 + 112, std::min (h - 96, 626), 108, 44);
        return;
    }

    const auto controlW = 250;
    const auto controlsStart = (w - controlW * 3 - 28) / 2;
    targetGenderCaption.setBounds (controlsStart + 16, 130, 104, 18);
    profileBox.setBounds (controlsStart + 122, 126, 112, 28);
    targetGenreCaption.setBounds (controlsStart + controlW + 30, 130, 94, 18);
    genreBox.setBounds (controlsStart + controlW + 122, 126, 112, 28);
    targetMoodCaption.setBounds (controlsStart + (controlW + 14) * 2 + 16, 130, 92, 18);
    vibeBox.setBounds (controlsStart + (controlW + 14) * 2 + 108, 126, 126, 28);

    const auto summaryX = margin + 40;
    const auto summaryW = w - (margin + 40) * 2;
    const auto segment = summaryW / 5;
    summaryKeyCaption.setBounds (summaryX + 18, 211, segment - 36, 18);
    summaryKeyValue.setBounds (summaryX + 18, 236, segment - 36, 48);
    summaryScaleCaption.setBounds (summaryX + segment + 18, 211, segment - 36, 18);
    summaryScaleValue.setBounds (summaryX + segment + 18, 238, segment - 36, 42);
    similarNotesCaption.setBounds (summaryX + segment * 2 + 18, 211, segment - 36, 18);
    similarNotesValue.setBounds (summaryX + segment * 2 + 18, 240, segment - 36, 36);
    inputTypeCaption.setBounds (summaryX + segment * 3 + 18, 211, segment - 36, 18);
    inputTypeValue.setBounds (summaryX + segment * 3 + 18, 240, segment - 36, 36);
    summaryConfidenceCaption.setBounds (summaryX + segment * 4 + 18, 211, segment - 36, 18);
    summaryConfidenceValue.setBounds (summaryX + segment * 4 + 18, 236, 86, 44);

    const auto settingsX = margin + 40;
    const auto settingsW = w - (margin + 40) * 2;
    const auto cardGap = 12;
    const auto cardW = (settingsW - 32 - cardGap * 5) / 6;
    settingsHeading.setBounds (w / 2 - 210, 316, 420, 22);
    for (int index = 0; index < 6; ++index)
    {
        const auto x = settingsX + 16 + index * (cardW + cardGap);
        settingCaptions[static_cast<size_t> (index)].setBounds (x + 6, 375, cardW - 12, 18);
        settingValues[static_cast<size_t> (index)].setBounds (x + 6, 404, cardW - 12, 52);
    }

    const auto bottomY = 520;
    const auto bottomH = std::max (118, h - bottomY - 114);
    const auto bottomW = w - (margin + 40) * 2;
    const auto leftW = static_cast<int> (bottomW * 0.34f);
    const auto middleW = static_cast<int> (bottomW * 0.30f);
    const auto leftX = margin + 40;
    const auto middleX = leftX + leftW + 14;
    const auto guideX = middleX + middleW + 14;
    inKeyHeading.setBounds (leftX + 20, bottomY + 16, leftW - 40, 18);
    inKeyNotes.setBounds (leftX + 20, bottomY + 53, leftW - 40, 42);
    lowestCaption.setBounds (middleX + 20, bottomY + 16, middleW / 2 - 30, 18);
    lowestValue.setBounds (middleX + 20, bottomY + 49, middleW / 2 - 30, 44);
    comfortCaption.setBounds (middleX + middleW / 2 + 4, bottomY + 16, middleW / 2 - 24, 18);
    comfortValue.setBounds (middleX + middleW / 2 + 4, bottomY + 49, middleW / 2 - 24, 44);
    guidanceHeading.setBounds (guideX + 20, bottomY + 16, bottomW - leftW - middleW - 68, 18);
    guidanceText.setBounds (guideX + 20, bottomY + 46, bottomW - leftW - middleW - 68, bottomH - 56);
    copySettingsButton.setBounds (w / 2 - 170, std::min (h - 96, bottomY + bottomH + 20), 340, 44);
}

void KeyBridgeAudioProcessorEditor::copySettings()
{
    const auto recommendation = currentVocalFit();
    if (! recommendation.ready)
    {
        footerStatus.setText ("STATUS: " + juce::String (recommendation.status), juce::dontSendNotification);
        return;
    }
    const auto text = juce::String ("TuneRite Auto-Tune Recommendation\n")
        + "Key: " + rootName (recommendation.keyRoot) + "\n"
        + "Scale: " + (recommendation.keyMode == 0 ? "Major" : "Minor") + "\n"
        + "Retune Speed: " + juce::String (recommendation.retuneSpeedMs) + "\n"
        + "Humanize: " + juce::String (recommendation.humanize) + "\n"
        + "Flex Tune: " + juce::String (recommendation.flexTune) + "\n"
        + "Formant: On\n"
        + "TuneRite provides starting recommendations only and does not modify Auto-Tune.";
    juce::SystemClipboard::copyTextToClipboard (text);
    footerStatus.setText ("STATUS: SETTINGS COPIED", juce::dontSendNotification);
}
