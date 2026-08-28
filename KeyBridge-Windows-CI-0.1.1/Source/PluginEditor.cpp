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

    juce::Colour colourFromArgb (std::uint32_t argb)
    {
        return juce::Colour (argb);
    }

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
}

KeyBridgeAudioProcessorEditor::KeyBridgeAudioProcessorEditor (KeyBridgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
    brandLogo = juce::ImageCache::getFromMemory (BinaryData::MurderMittenMediaLogo_png,
                                                 BinaryData::MurderMittenMediaLogo_pngSize);
    setResizable (true, true);
    setResizeLimits (1000, 650, 1800, 1200);
    setSize (1280, 780);

    addLabel (title, 24.0f, juce::Justification::centredLeft, juce::Colours::white);
    addLabel (subtitle, 11.0f, juce::Justification::centredLeft, juce::Colour (0xffaab7c6));
    addLabel (inputStatus, 11.0f, juce::Justification::centredRight, juce::Colour (0xffd9e4ee));
    addLabel (analysisStatus, 11.0f, juce::Justification::centredRight, juce::Colour (0xffd9e4ee));
    addLabel (projectBpmLabel, 11.0f, juce::Justification::centredRight, juce::Colour (0xff8c9aaa));
    addLabel (workflowLabel, 11.0f, juce::Justification::centredLeft, juce::Colour (0xff9bb3c9));
    addLabel (instructionLabel, 13.0f, juce::Justification::centredLeft, juce::Colours::white);
    addLabel (visualStatusLabel, 11.0f, juce::Justification::centredLeft, juce::Colour (0xffafbecd));
    addLabel (meterCaption, 10.0f, juce::Justification::centredRight, juce::Colour (0xff9baaba));

    for (auto* label : { &bpmTitle, &keyTitle, &vocalTitle, &recommendationTitle })
        addLabel (*label, 11.0f, juce::Justification::centredLeft, juce::Colour (0xffa7bacb));
    for (auto* label : { &bpmValue, &keyValue, &vocalValue, &recommendationValue })
        addLabel (*label, 29.0f, juce::Justification::centredLeft, juce::Colours::white);
    for (auto* label : { &bpmDetail, &keyDetail, &keyNotes, &vocalDetail, &recommendationDetail })
        addLabel (*label, 11.0f, juce::Justification::centredLeft, juce::Colour (0xffd7e1ea));
    for (auto* label : { &bpmStatus, &keyStatus, &vocalStatus, &recommendationStatus })
        addLabel (*label, 10.0f, juce::Justification::centredLeft, juce::Colour (0xff9eb0c1));

    title.setText ("TuneRite", juce::dontSendNotification);
    subtitle.setText ("by Murder Mitten Media | Audio Analysis Assistant", juce::dontSendNotification);
    bpmTitle.setText ("AUDIO BPM", juce::dontSendNotification);
    keyTitle.setText ("KEY / SCALE", juce::dontSendNotification);
    vocalTitle.setText ("VOCAL ANALYSIS", juce::dontSendNotification);
    recommendationTitle.setText ("AUTO-TUNE STARTING POINT", juce::dontSendNotification);

    for (auto* button : { &beatModeButton, &vocalModeButton, &reviewModeButton, &analyzeButton, &stopButton,
                          &saveButton, &clearButton, &copyBpmButton, &copyReportButton, &copySettingsButton,
                          &resetButton, &appearanceButton, &closeAppearanceButton, &saveThemeButton, &resetThemeButton })
        addButton (*button, button->getButtonText());

    beatModeButton.setClickingTogglesState (true);
    vocalModeButton.setClickingTogglesState (true);
    reviewModeButton.setClickingTogglesState (true);
    beatModeButton.onClick = [this] { setMode (0); };
    vocalModeButton.onClick = [this] { setMode (1); };
    reviewModeButton.onClick = [this] { setMode (2); };
    analyzeButton.onClick = [this]
    {
        if (processor.getAnalysisMode() != 2 && processor.getCaptureState() == 2)
            processor.finishCapture();
        else
            processor.startFreshAnalysis();
    };
    stopButton.onClick = [this] { processor.finishCapture(); };
    saveButton.onClick = [this]
    {
        if (processor.getAnalysisMode() == 0) processor.saveBeatResult();
        else if (processor.getAnalysisMode() == 1) processor.saveVocalResult();
        refreshView();
    };
    clearButton.onClick = [this]
    {
        if (processor.getAnalysisMode() == 0) processor.clearBeatResult();
        else if (processor.getAnalysisMode() == 1) processor.clearVocalResult();
        refreshView();
    };
    copyBpmButton.onClick = [this] { copyDetectedBpm(); };
    copyReportButton.onClick = [this] { copyEngineerReport(); };
    copySettingsButton.onClick = [this] { copySettings(); };
    resetButton.onClick = [this] { processor.resetAllResults(); refreshView(); };
    appearanceButton.onClick = [this] { appearanceOpen = true; resized(); repaint(); };
    closeAppearanceButton.onClick = [this] { appearanceOpen = false; resized(); repaint(); };
    saveThemeButton.onClick = [this] { saveAppearance(); };
    resetThemeButton.onClick = [this] { processor.resetAppearance(); refreshAppearance(); };

    addOptions (profileBox, { "Auto range", "Male hint", "Female hint", "Custom" });
    addOptions (genreBox, { "Rap", "Melodic rap", "Trap", "R&B", "Pop", "Gospel", "Soul", "Hip-hop" });
    addOptions (deliveryBox, { "Rap", "Melodic", "Sung", "Spoken", "Chant" });
    addOptions (vibeBox, { "Natural", "Hard tune", "Dark", "Emotional", "Energetic", "Romantic", "Aggressive", "Laid-back" });
    profileBox.setSelectedId (1, juce::dontSendNotification);
    genreBox.setSelectedId (3, juce::dontSendNotification);
    deliveryBox.setSelectedId (2, juce::dontSendNotification);
    vibeBox.setSelectedId (1, juce::dontSendNotification);
    for (auto* box : { &profileBox, &genreBox, &deliveryBox, &vibeBox })
    {
        box->setTooltip ("A user preference that affects recommendation style only. It never changes measured key or BPM.");
        box->onChange = [this] { refreshView(); };
        addAndMakeVisible (*box);
    }

    addOptions (themeBox, { "Cyan", "Violet", "Blue", "Green", "Amber", "Monochrome" });
    addOptions (colourTargetBox, { "Accent", "Panel", "Background" });
    themeBox.setSelectedId (1, juce::dontSendNotification);
    colourTargetBox.setSelectedId (1, juce::dontSendNotification);
    themeBox.onChange = [this] { applyThemePreset(); };
    colourTargetBox.onChange = [this] { colourSelector.setCurrentColour (colourTargetBox.getSelectedId() == 1 ? accentColour() : colourTargetBox.getSelectedId() == 2 ? panelColour() : backgroundColour()); };
    opacitySlider.setRange (0.72, 1.0, 0.01);
    glowSlider.setRange (0.0, 1.0, 0.01);
    opacitySlider.onValueChange = [this] { applyColourSelector(); };
    glowSlider.onValueChange = [this] { applyColourSelector(); };
    compactLayoutToggle.onClick = [this] { applyColourSelector(); resized(); };
    colourSelector.addChangeListener (this);
    for (auto* component : { static_cast<juce::Component*> (&themeBox), static_cast<juce::Component*> (&colourTargetBox), static_cast<juce::Component*> (&opacitySlider), static_cast<juce::Component*> (&glowSlider), static_cast<juce::Component*> (&compactLayoutToggle), static_cast<juce::Component*> (&colourSelector) })
        addAndMakeVisible (*component);
    addLabel (appearanceTitle, 16.0f, juce::Justification::centredLeft, juce::Colours::white);
    for (auto* label : { &themeCaption, &colourTargetCaption, &opacityCaption, &glowCaption, &layoutCaption })
        addLabel (*label, 10.0f, juce::Justification::centredLeft, juce::Colour (0xffaab7c6));
    appearanceTitle.setText ("APPEARANCE", juce::dontSendNotification);
    setCaption (themeCaption, "THEME");
    setCaption (colourTargetCaption, "CUSTOM COLOUR TARGET");
    setCaption (opacityCaption, "PANEL OPACITY");
    setCaption (glowCaption, "GLOW INTENSITY");
    setCaption (layoutCaption, "LAYOUT");

    bpmValue.setTooltip ("Audio-derived tempo. Project BPM is separate reference metadata.");
    keyValue.setTooltip ("TuneRite withholds a scale when key confidence is inadequate.");
    vocalValue.setTooltip ("Measured stable note when voiced pitch is valid. No vocal setting is saved from an invalid result.");
    recommendationValue.setTooltip ("Starting settings only. TuneRite does not control Auto-Tune directly.");

    processor.setAnalysisMode (0);
    refreshAppearance();
    setMode (0);
    juce::MessageManager::callAsync ([editor = juce::Component::SafePointer<KeyBridgeAudioProcessorEditor> (this)]
    {
        if (editor != nullptr)
            editor->ensureUiTimerRunning();
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
    button.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    button.setTooltip (text);
    addAndMakeVisible (button);
}

void KeyBridgeAudioProcessorEditor::setCaption (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
}

juce::Colour KeyBridgeAudioProcessorEditor::accentColour() const { return colourFromArgb (processor.getAppearanceAccent()); }
juce::Colour KeyBridgeAudioProcessorEditor::panelColour() const { return colourFromArgb (processor.getAppearancePanel()); }
juce::Colour KeyBridgeAudioProcessorEditor::backgroundColour() const { return colourFromArgb (processor.getAppearanceBackground()); }

juce::Colour KeyBridgeAudioProcessorEditor::statusColour (const juce::String& status) const
{
    if (status.containsIgnoreCase ("INVALID") || status.containsIgnoreCase ("NO SIGNAL")) return juce::Colour (0xffef6b73);
    if (status.containsIgnoreCase ("LOW") || status.containsIgnoreCase ("UNCERTAIN")) return juce::Colour (0xffffb454);
    if (status.containsIgnoreCase ("SAVED") || status.containsIgnoreCase ("VALID")) return juce::Colour (0xff5fd29c);
    if (status.containsIgnoreCase ("LISTENING") || status.containsIgnoreCase ("PROCESS")) return accentColour();
    return juce::Colour (0xffaebdca);
}

juce::String KeyBridgeAudioProcessorEditor::keyName (int root, int mode) const
{
    if (root < 0 || root >= 12 || (mode != 0 && mode != 1))
        return "UNKNOWN";
    return juce::String (noteNames[static_cast<size_t> (root)]) + (mode == 0 ? " major" : " minor");
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
    request.voiceProfile = profileBox.getText().toStdString();
    request.genre = genreBox.getText().toStdString();
    request.delivery = deliveryBox.getText().toStdString();
    request.mood = vibeBox.getText().toStdString();
    return tunerite::VocalFit::recommend (request);
}

juce::String KeyBridgeAudioProcessorEditor::midiName (float midi) const
{
    if (midi <= 0.0f) return "--";
    const auto rounded = static_cast<int> (std::round (midi));
    const auto octave = rounded / 12 - 1;
    return juce::String (noteNames[static_cast<size_t> ((rounded % 12 + 12) % 12)]) + juce::String (octave);
}

void KeyBridgeAudioProcessorEditor::ensureUiTimerRunning()
{
    if (! isTimerRunning())
        startTimerHz (10);
}

void KeyBridgeAudioProcessorEditor::parentHierarchyChanged()
{
    if (isShowing())
        ensureUiTimerRunning();
}

void KeyBridgeAudioProcessorEditor::visibilityChanged()
{
    if (isShowing())
        ensureUiTimerRunning();
    else
        stopTimer();
}

void KeyBridgeAudioProcessorEditor::setMode (int mode)
{
    processor.setAnalysisMode (mode);
    processor.setAnalysisEnabled (true);
    beatModeButton.setToggleState (mode == 0, juce::dontSendNotification);
    vocalModeButton.setToggleState (mode == 1, juce::dontSendNotification);
    reviewModeButton.setToggleState (mode == 2, juce::dontSendNotification);
    resized();
    refreshView();
}

void KeyBridgeAudioProcessorEditor::refreshAppearance()
{
    opacitySlider.setValue (processor.getAppearancePanelOpacity(), juce::dontSendNotification);
    glowSlider.setValue (processor.getAppearanceGlow(), juce::dontSendNotification);
    compactLayoutToggle.setToggleState (processor.isCompactAppearance(), juce::dontSendNotification);
    colourSelector.setCurrentColour (accentColour());
    repaint();
}

void KeyBridgeAudioProcessorEditor::applyThemePreset()
{
    juce::Colour accent (0xff55c7e8), panel (0xff17202c), background (0xff0b1017);
    switch (themeBox.getSelectedId())
    {
        case 2: accent = juce::Colour (0xffaa8cff); panel = juce::Colour (0xff1d1930); background = juce::Colour (0xff100e1a); break;
        case 3: accent = juce::Colour (0xff62a8ff); panel = juce::Colour (0xff152238); background = juce::Colour (0xff0a101c); break;
        case 4: accent = juce::Colour (0xff55d7a5); panel = juce::Colour (0xff15251f); background = juce::Colour (0xff0a1511); break;
        case 5: accent = juce::Colour (0xffffb454); panel = juce::Colour (0xff292016); background = juce::Colour (0xff17110a); break;
        case 6: accent = juce::Colour (0xffd8dee8); panel = juce::Colour (0xff242a31); background = juce::Colour (0xff12161a); break;
        default: break;
    }
    processor.setAppearance (accent.getARGB(), panel.getARGB(), background.getARGB(), static_cast<float> (opacitySlider.getValue()), static_cast<float> (glowSlider.getValue()), compactLayoutToggle.getToggleState());
    colourSelector.setCurrentColour (colourTargetBox.getSelectedId() == 1 ? accent : colourTargetBox.getSelectedId() == 2 ? panel : background);
    repaint();
}

void KeyBridgeAudioProcessorEditor::applyColourSelector()
{
    auto accent = accentColour();
    auto panel = panelColour();
    auto background = backgroundColour();
    if (colourTargetBox.getSelectedId() == 1) accent = colourSelector.getCurrentColour();
    else if (colourTargetBox.getSelectedId() == 2) panel = colourSelector.getCurrentColour();
    else background = colourSelector.getCurrentColour();
    processor.setAppearance (accent.getARGB(), panel.getARGB(), background.getARGB(), static_cast<float> (opacitySlider.getValue()), static_cast<float> (glowSlider.getValue()), compactLayoutToggle.getToggleState());
    repaint();
}

void KeyBridgeAudioProcessorEditor::saveAppearance()
{
    applyColourSelector();
    analysisStatus.setText ("APPEARANCE SAVED", juce::dontSendNotification);
}

void KeyBridgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto background = backgroundColour();
    const auto panel = panelColour().withAlpha (juce::jlimit (0.72f, 1.0f, processor.getAppearancePanelOpacity()));
    const auto accent = accentColour();
    g.fillAll (background);

    const auto w = getWidth();
    const auto h = getHeight();
    if (brandLogo.isValid())
        g.drawImageWithin (brandLogo, 18, 10, 54, 54, juce::RectanglePlacement::centred, false);

    g.setColour (accent.withAlpha (0.10f + processor.getAppearanceGlow() * 0.12f));
    g.fillRect (0, 0, w, 4);
    g.setColour (juce::Colours::black.withAlpha (0.24f));
    g.fillRect (0, 74, w, 1);

    if (processor.getAnalysisMode() == 0)
    {
        const auto margin = 18;
        const auto answerY = 126;
        const auto gap = 18;
        const auto answerWidth = (w - margin * 2 - gap) / 2;
        const auto answerHeight = std::max (210, h - 330);
        for (const auto& card : { juce::Rectangle<int> (margin, answerY, answerWidth, answerHeight), juce::Rectangle<int> (margin + answerWidth + gap, answerY, answerWidth, answerHeight) })
        {
            g.setColour (panel);
            g.fillRoundedRectangle (card.toFloat(), 10.0f);
            g.setColour (accent.withAlpha (0.55f));
            g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 10.0f, 1.0f);
        }
        return;
    }

    const auto margin = 18;
    const auto top = 92;
    const auto reviewWidth = static_cast<int> (w * 0.29f);
    const auto gap = 12;
    const auto leftWidth = w - margin * 2 - reviewWidth - gap;
    const auto half = (leftWidth - gap) / 2;
    const auto shortHeight = processor.isCompactAppearance() ? 128 : 148;
    const auto vocalY = top + shortHeight + gap;
    const auto vocalHeight = processor.isCompactAppearance() ? 132 : 154;

    auto drawModule = [&g, panel, accent] (juce::Rectangle<int> bounds, bool recommendation)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (bounds.toFloat(), 8.0f);
        g.setColour ((recommendation ? juce::Colour (0xff8267df) : accent).withAlpha (0.58f));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 8.0f, 1.0f);
    };
    drawModule ({ margin, top, half, shortHeight }, false);
    drawModule ({ margin + half + gap, top, half, shortHeight }, false);
    drawModule ({ margin, vocalY, leftWidth, vocalHeight }, false);
    drawModule ({ margin + leftWidth + gap, top, reviewWidth, vocalY + vocalHeight - top }, true);

    const auto workflowY = vocalY + vocalHeight + gap;
    g.setColour (panel);
    g.fillRoundedRectangle (juce::Rectangle<float> (static_cast<float> (margin), static_cast<float> (workflowY), static_cast<float> (w - margin * 2), 102.0f), 8.0f);
    g.setColour (accent.withAlpha (0.42f));
    g.drawRoundedRectangle (juce::Rectangle<float> (static_cast<float> (margin), static_cast<float> (workflowY), static_cast<float> (w - margin * 2), 102.0f).reduced (0.5f), 8.0f, 1.0f);

    const auto visualY = workflowY + 114;
    const auto visualHeight = std::max (92, h - visualY - 18);
    g.setColour (panel);
    g.fillRoundedRectangle (juce::Rectangle<float> (static_cast<float> (margin), static_cast<float> (visualY), static_cast<float> (w - margin * 2), static_cast<float> (visualHeight)), 8.0f);
    g.setColour (accent.withAlpha (0.28f));
    g.drawRoundedRectangle (juce::Rectangle<float> (static_cast<float> (margin), static_cast<float> (visualY), static_cast<float> (w - margin * 2), static_cast<float> (visualHeight)).reduced (0.5f), 8.0f, 1.0f);

    const auto trace = juce::Rectangle<float> (static_cast<float> (margin + 18), static_cast<float> (visualY + 30), static_cast<float> (w - margin * 2 - 36), static_cast<float> (visualHeight - 44));
    g.setColour (juce::Colour (0xff8ba0b5).withAlpha (0.16f));
    g.drawLine (trace.getX(), trace.getCentreY(), trace.getRight(), trace.getCentreY(), 1.0f);
    juce::Path waveform;
    for (size_t i = 0; i < levelHistory.size(); ++i)
    {
        const auto x = trace.getX() + trace.getWidth() * static_cast<float> (i) / static_cast<float> (levelHistory.size() - 1);
        const auto y = trace.getCentreY() - trace.getHeight() * 0.40f * juce::jlimit (0.0f, 1.0f, levelHistory[i]);
        if (i == 0) waveform.startNewSubPath (x, y); else waveform.lineTo (x, y);
    }
    g.setColour (accent.withAlpha (0.85f));
    g.strokePath (waveform, juce::PathStrokeType (1.6f));

    const auto meterBase = juce::Rectangle<float> (static_cast<float> (w - 86), 16.0f, 7.0f, 38.0f);
    for (int channel = 0; channel < 2; ++channel)
    {
        const auto level = channel == 0 ? leftMeter : rightMeter;
        const auto meter = meterBase.translated (static_cast<float> (channel * 13), 0.0f);
        g.setColour (juce::Colour (0xff1e2a35));
        g.fillRoundedRectangle (meter, 2.0f);
        const auto active = meter.withTop (meter.getBottom() - meter.getHeight() * juce::jlimit (0.0f, 1.0f, level));
        g.setColour (level > 0.88f ? juce::Colour (0xffef6b73) : accent);
        g.fillRoundedRectangle (active, 2.0f);
    }

    if (appearanceOpen)
    {
        const auto overlay = juce::Rectangle<int> (std::max (22, w - 404), 84, 382, std::min (h - 106, 566));
        g.setColour (background.brighter (0.08f).withAlpha (0.985f));
        g.fillRoundedRectangle (overlay.toFloat(), 10.0f);
        g.setColour (accent.withAlpha (0.72f));
        g.drawRoundedRectangle (overlay.toFloat().reduced (0.5f), 10.0f, 1.0f);
    }
}

void KeyBridgeAudioProcessorEditor::resized()
{
    const auto w = getWidth();
    const auto h = getHeight();
    const auto margin = 18;

    if (processor.getAnalysisMode() == 0)
    {
        title.setBounds (82, 12, 190, 32);
        subtitle.setBounds (82, 44, 260, 17);
        beatModeButton.setBounds (350, 18, 104, 32);
        vocalModeButton.setBounds (458, 18, 110, 32);
        reviewModeButton.setBounds (572, 18, 90, 32);
        appearanceButton.setVisible (false);
        analysisStatus.setBounds (margin, 77, w - margin * 2, 20);
        analysisStatus.setJustificationType (juce::Justification::centredLeft);

        const auto answerY = 126;
        const auto gap = 18;
        const auto answerWidth = (w - margin * 2 - gap) / 2;
        const auto answerHeight = std::max (210, h - 330);
        bpmTitle.setBounds (margin + 28, answerY + 28, answerWidth - 56, 22);
        bpmValue.setBounds (margin + 28, answerY + 72, answerWidth - 56, 76);
        keyTitle.setBounds (margin + answerWidth + gap + 28, answerY + 28, answerWidth - 56, 22);
        keyValue.setBounds (margin + answerWidth + gap + 28, answerY + 72, answerWidth - 56, 76);
        bpmStatus.setBounds (margin, answerY + answerHeight + 22, w - margin * 2, 22);
        bpmStatus.setJustificationType (juce::Justification::centred);
        workflowLabel.setVisible (false);
        instructionLabel.setBounds (margin, answerY + answerHeight + 54, w - margin * 2, 20);
        instructionLabel.setJustificationType (juce::Justification::centred);
        analyzeButton.setBounds (w / 2 - 178, answerY + answerHeight + 92, 230, 38);
        clearButton.setBounds (w / 2 + 64, answerY + answerHeight + 92, 118, 38);
        resetButton.setVisible (false);
        return;
    }

    appearanceButton.setVisible (true);
    resetButton.setVisible (true);
    const auto top = 92;
    const auto gap = 12;
    const auto reviewWidth = static_cast<int> (w * 0.29f);
    const auto leftWidth = w - margin * 2 - reviewWidth - gap;
    const auto half = (leftWidth - gap) / 2;
    const auto shortHeight = processor.isCompactAppearance() ? 128 : 148;
    const auto vocalY = top + shortHeight + gap;
    const auto vocalHeight = processor.isCompactAppearance() ? 132 : 154;
    const auto reviewX = margin + leftWidth + gap;

    title.setBounds (82, 8, 190, 30);
    subtitle.setBounds (82, 37, 250, 17);
    beatModeButton.setBounds (340, 18, 104, 32);
    vocalModeButton.setBounds (448, 18, 110, 32);
    reviewModeButton.setBounds (562, 18, 90, 32);
    appearanceButton.setBounds (w - 238, 20, 104, 27);
    inputStatus.setBounds (w - 430, 10, 178, 18);
    analysisStatus.setBounds (w - 430, 29, 178, 18);
    projectBpmLabel.setBounds (w - 430, 48, 178, 16);
    meterCaption.setBounds (w - 92, 53, 70, 12);

    bpmTitle.setBounds (margin + 16, top + 14, half - 32, 16);
    bpmValue.setBounds (margin + 16, top + 33, half - 32, 42);
    bpmDetail.setBounds (margin + 16, top + 76, half - 32, 32);
    bpmStatus.setBounds (margin + 16, top + shortHeight - 24, half - 32, 14);

    const auto keyX = margin + half + gap;
    keyTitle.setBounds (keyX + 16, top + 14, half - 32, 16);
    keyValue.setBounds (keyX + 16, top + 33, half - 32, 42);
    keyDetail.setBounds (keyX + 16, top + 76, half - 32, 20);
    keyNotes.setBounds (keyX + 16, top + 96, half - 32, 22);
    keyStatus.setBounds (keyX + 16, top + shortHeight - 24, half - 32, 14);

    vocalTitle.setBounds (margin + 16, vocalY + 14, leftWidth - 32, 16);
    vocalValue.setBounds (margin + 16, vocalY + 34, 250, 40);
    vocalDetail.setBounds (margin + 16, vocalY + 77, leftWidth - 32, vocalHeight - 112);
    vocalStatus.setBounds (margin + 16, vocalY + vocalHeight - 24, leftWidth - 32, 14);

    recommendationTitle.setBounds (reviewX + 16, top + 14, reviewWidth - 32, 16);
    recommendationValue.setBounds (reviewX + 16, top + 36, reviewWidth - 32, 42);
    recommendationDetail.setBounds (reviewX + 16, top + 82, reviewWidth - 32, vocalY + vocalHeight - top - 122);
    recommendationStatus.setBounds (reviewX + 16, vocalY + vocalHeight - 24, reviewWidth - 32, 14);

    const auto workflowY = vocalY + vocalHeight + gap;
    workflowLabel.setBounds (margin + 16, workflowY + 12, 230, 16);
    instructionLabel.setBounds (margin + 16, workflowY + 34, w - margin * 2 - 32, 20);
    analyzeButton.setBounds (margin + 16, workflowY + 64, 154, 28);
    stopButton.setBounds (margin + 178, workflowY + 64, 76, 28);
    saveButton.setBounds (margin + 262, workflowY + 64, 164, 28);
    clearButton.setBounds (margin + 434, workflowY + 64, 118, 28);
    copyBpmButton.setBounds (margin + 560, workflowY + 64, 98, 28);
    copyReportButton.setBounds (w - margin - 316, workflowY + 64, 158, 28);
    copySettingsButton.setBounds (w - margin - 150, workflowY + 64, 150, 28);
    resetButton.setBounds (w - margin - 100, workflowY + 12, 100, 24);

    const auto visualY = workflowY + 114;
    visualStatusLabel.setBounds (margin + 16, visualY + 9, w - margin * 2 - 32, 16);

    const auto overlayX = std::max (22, w - 404);
    const auto appearanceVisible = appearanceOpen;
    for (auto* component : { static_cast<juce::Component*> (&appearanceTitle), static_cast<juce::Component*> (&themeCaption), static_cast<juce::Component*> (&colourTargetCaption), static_cast<juce::Component*> (&opacityCaption), static_cast<juce::Component*> (&glowCaption), static_cast<juce::Component*> (&layoutCaption), static_cast<juce::Component*> (&themeBox), static_cast<juce::Component*> (&colourTargetBox), static_cast<juce::Component*> (&opacitySlider), static_cast<juce::Component*> (&glowSlider), static_cast<juce::Component*> (&compactLayoutToggle), static_cast<juce::Component*> (&colourSelector), static_cast<juce::Component*> (&closeAppearanceButton), static_cast<juce::Component*> (&saveThemeButton), static_cast<juce::Component*> (&resetThemeButton) })
        component->setVisible (appearanceVisible);
    appearanceTitle.setBounds (overlayX + 18, 102, 190, 24);
    closeAppearanceButton.setBounds (overlayX + 280, 104, 84, 24);
    themeCaption.setBounds (overlayX + 18, 140, 100, 14);
    themeBox.setBounds (overlayX + 18, 156, 164, 26);
    colourTargetCaption.setBounds (overlayX + 198, 140, 150, 14);
    colourTargetBox.setBounds (overlayX + 198, 156, 166, 26);
    opacityCaption.setBounds (overlayX + 18, 198, 120, 14);
    opacitySlider.setBounds (overlayX + 18, 214, 164, 28);
    glowCaption.setBounds (overlayX + 198, 198, 120, 14);
    glowSlider.setBounds (overlayX + 198, 214, 166, 28);
    layoutCaption.setBounds (overlayX + 18, 254, 120, 14);
    compactLayoutToggle.setBounds (overlayX + 18, 270, 160, 24);
    colourSelector.setBounds (overlayX + 18, 306, 346, 184);
    saveThemeButton.setBounds (overlayX + 18, 508, 142, 28);
    resetThemeButton.setBounds (overlayX + 168, 508, 142, 28);
}

void KeyBridgeAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &colourSelector)
        applyColourSelector();
}

void KeyBridgeAudioProcessorEditor::timerCallback()
{
    leftMeter = 0.84f * leftMeter + 0.16f * processor.getLeftInputLevel();
    rightMeter = 0.84f * rightMeter + 0.16f * processor.getRightInputLevel();
    std::move (levelHistory.begin() + 1, levelHistory.end(), levelHistory.begin());
    levelHistory.back() = processor.getInputLevel();
    refreshView();
}

void KeyBridgeAudioProcessorEditor::refreshView()
{
    const auto mode = processor.getAnalysisMode();
    const auto beatTempoSaved = processor.hasSavedBeatTempo();
    const auto beatKeySaved = processor.hasSavedBeatKey();
    const auto vocalSaved = processor.hasValidVocalResult();
    const auto live = processor.isAnalysisActive();
    const auto signal = processor.getInputLevel() > 0.0001f;

    inputStatus.setText (signal ? "INPUT: ACTIVE" : "INPUT: NO SIGNAL", juce::dontSendNotification);
    inputStatus.setColour (juce::Label::textColourId, statusColour (inputStatus.getText()));
    juce::String status;
    switch (processor.getCaptureState())
    {
        case 1: status = "ARMED - WAITING FOR AUDIO"; break;
        case 2: status = processor.isCaptureFinishRequested() ? "FINISH REQUESTED" : "CAPTURING"; break;
        case 3: status = "PROCESSING CAPTURE"; break;
        case 4: status = "NO SIGNAL - KEEP PLAYING"; break;
        case 5: status = "NEED 12 SECONDS BEFORE FINISH"; break;
        case 6: status = "CAPTURE CANCELLED"; break;
        default: status = mode == 2 ? "REVIEW READY" : signal ? "READY" : "NO SIGNAL"; break;
    }
    analysisStatus.setText (juce::String ("STATUS: ") + status, juce::dontSendNotification);
    analysisStatus.setColour (juce::Label::textColourId, statusColour (status));
    projectBpmLabel.setText ("Project BPM: " + (processor.getHostBpm() > 0.0 ? juce::String (processor.getHostBpm(), 2) : "--"), juce::dontSendNotification);

    if (mode == 0)
    {
        for (auto* component : { static_cast<juce::Component*> (&inputStatus), static_cast<juce::Component*> (&projectBpmLabel), static_cast<juce::Component*> (&meterCaption), static_cast<juce::Component*> (&bpmDetail), static_cast<juce::Component*> (&keyDetail), static_cast<juce::Component*> (&keyNotes), static_cast<juce::Component*> (&keyStatus), static_cast<juce::Component*> (&vocalTitle), static_cast<juce::Component*> (&vocalValue), static_cast<juce::Component*> (&vocalDetail), static_cast<juce::Component*> (&vocalStatus), static_cast<juce::Component*> (&recommendationTitle), static_cast<juce::Component*> (&recommendationValue), static_cast<juce::Component*> (&recommendationDetail), static_cast<juce::Component*> (&recommendationStatus), static_cast<juce::Component*> (&stopButton), static_cast<juce::Component*> (&saveButton), static_cast<juce::Component*> (&copyBpmButton), static_cast<juce::Component*> (&copyReportButton), static_cast<juce::Component*> (&copySettingsButton), static_cast<juce::Component*> (&visualStatusLabel), static_cast<juce::Component*> (&profileBox), static_cast<juce::Component*> (&genreBox), static_cast<juce::Component*> (&deliveryBox), static_cast<juce::Component*> (&vibeBox), static_cast<juce::Component*> (&themeBox), static_cast<juce::Component*> (&colourTargetBox), static_cast<juce::Component*> (&opacitySlider), static_cast<juce::Component*> (&glowSlider), static_cast<juce::Component*> (&compactLayoutToggle), static_cast<juce::Component*> (&colourSelector), static_cast<juce::Component*> (&appearanceTitle), static_cast<juce::Component*> (&themeCaption), static_cast<juce::Component*> (&colourTargetCaption), static_cast<juce::Component*> (&opacityCaption), static_cast<juce::Component*> (&glowCaption), static_cast<juce::Component*> (&layoutCaption), static_cast<juce::Component*> (&closeAppearanceButton), static_cast<juce::Component*> (&saveThemeButton), static_cast<juce::Component*> (&resetThemeButton) })
            component->setVisible (false);

        const auto captureState = processor.getCaptureState();
        const auto analyzing = captureState == 1 || captureState == 2 || captureState == 3;
        const auto savedTempo = processor.hasSavedBeatTempo();
        const auto savedKey = processor.hasSavedBeatKey();
        bpmTitle.setText ("BPM", juce::dontSendNotification);
        keyTitle.setText ("KEY / SCALE", juce::dontSendNotification);

        if (analyzing)
        {
            bpmValue.setText ("ANALYZING...", juce::dontSendNotification);
            keyValue.setText ("ANALYZING...", juce::dontSendNotification);
            bpmStatus.setText (captureState == 3 ? "STATUS: ANALYZING BEAT" : "STATUS: LISTENING TO BEAT", juce::dontSendNotification);
        }
        else
        {
            bpmValue.setText (savedTempo ? juce::String (processor.getSavedBeatBpm(), 1) : "--", juce::dontSendNotification);
            keyValue.setText (savedKey ? keyName (processor.getSavedBeatKey(), processor.getSavedBeatMode()) : "UNCERTAIN", juce::dontSendNotification);
            const auto outcome = processor.getBeatOutcomeState();
            const auto analyzerWarning = processor.getLastBeatAnalysisWarning();
            bpmStatus.setText (outcome == 1 ? "STATUS: BEAT SAVED"
                : outcome == 2 ? (analyzerWarning.isNotEmpty() ? "STATUS: " + analyzerWarning : "STATUS: NOT ENOUGH USABLE AUDIO")
                : "STATUS: NOT ANALYZED", juce::dontSendNotification);
        }
        bpmStatus.setColour (juce::Label::textColourId, statusColour (bpmStatus.getText()));
        instructionLabel.setText ("Mute vocals and play the beat into this mixer track.", juce::dontSendNotification);
        analyzeButton.setButtonText (captureState == 2 ? "STOP AND SAVE" : "ANALYZE BEAT");
        analyzeButton.setEnabled (captureState != 3);
        clearButton.setButtonText ("CLEAR");
        clearButton.setEnabled (processor.hasSavedBeatResult() || processor.getBeatOutcomeState() != 0);
        bpmDetail.setVisible (false);
        return;
    }

    for (auto* component : { static_cast<juce::Component*> (&inputStatus), static_cast<juce::Component*> (&projectBpmLabel), static_cast<juce::Component*> (&meterCaption), static_cast<juce::Component*> (&bpmDetail), static_cast<juce::Component*> (&keyDetail), static_cast<juce::Component*> (&keyNotes), static_cast<juce::Component*> (&keyStatus), static_cast<juce::Component*> (&vocalTitle), static_cast<juce::Component*> (&vocalValue), static_cast<juce::Component*> (&vocalDetail), static_cast<juce::Component*> (&vocalStatus), static_cast<juce::Component*> (&recommendationTitle), static_cast<juce::Component*> (&recommendationValue), static_cast<juce::Component*> (&recommendationDetail), static_cast<juce::Component*> (&recommendationStatus), static_cast<juce::Component*> (&stopButton), static_cast<juce::Component*> (&saveButton), static_cast<juce::Component*> (&copyBpmButton), static_cast<juce::Component*> (&copyReportButton), static_cast<juce::Component*> (&copySettingsButton), static_cast<juce::Component*> (&visualStatusLabel) })
        component->setVisible (true);

    const auto bpm = beatTempoSaved ? processor.getSavedBeatBpm() : processor.getDetectedBpm();
    const auto alternative = beatTempoSaved ? processor.getSavedBeatAlternativeBpm() : processor.getAlternativeBpm();
    const auto bpmConfidence = beatTempoSaved ? processor.getSavedBeatBpmConfidence() : processor.getBpmConfidence();
    if (beatTempoSaved)
    {
        bpmValue.setText (juce::String (bpm, 1), juce::dontSendNotification);
        bpmDetail.setText ("Project BPM: " + juce::String (processor.getHostBpm(), 2)
            + "  |  Confidence: " + confidenceText (bpmConfidence)
            + "\nTempo candidates: " + juce::String (bpm, 1) + (alternative > 0.0 ? " / " + juce::String (alternative, 1) : ""), juce::dontSendNotification);
        bpmStatus.setText ("MEASURED  |  SAVED", juce::dontSendNotification);
    }
    else if (mode == 0 && bpm > 0.0)
    {
        bpmValue.setText (juce::String (bpm, 1), juce::dontSendNotification);
        bpmDetail.setText ("Confidence: " + confidenceText (bpmConfidence)
            + (alternative > 0.0 ? "  |  Alternative: " + juce::String (alternative, 1) : "")
            + "\nProject BPM is reference metadata only.", juce::dontSendNotification);
        bpmStatus.setText ("LIVE PREVIEW - NOT SAVED", juce::dontSendNotification);
    }
    else
    {
        bpmValue.setText ("--", juce::dontSendNotification);
        bpmDetail.setText ("No valid beat analysis saved.", juce::dontSendNotification);
        bpmStatus.setText ("WAITING FOR BEAT CAPTURE", juce::dontSendNotification);
    }
    bpmStatus.setColour (juce::Label::textColourId, statusColour (bpmStatus.getText()));

    const auto root = beatKeySaved ? processor.getSavedBeatKey() : processor.getDetectedKey();
    const auto scaleMode = beatKeySaved ? processor.getSavedBeatMode() : processor.getDetectedMode();
    const auto keyConfidence = beatKeySaved ? processor.getSavedBeatKeyConfidence() : processor.getKeyConfidence();
    if (beatKeySaved)
    {
        const auto& scale = scaleMode == 0 ? majorScale : minorScale;
        juce::String notes;
        for (int index = 0; index < 7; ++index)
            notes += juce::String (noteNames[static_cast<size_t> ((root + scale[static_cast<size_t> (index)]) % 12)]) + (index == 6 ? "" : "  ");
        keyValue.setText (keyName (root, scaleMode), juce::dontSendNotification);
        keyDetail.setText ("Scale: " + juce::String (scaleMode == 0 ? "Major" : "Minor") + "  |  Confidence: " + confidenceText (keyConfidence) + "  |  A4 = 440 Hz", juce::dontSendNotification);
        keyNotes.setText ("Enabled notes: " + notes, juce::dontSendNotification);
        keyStatus.setText ("MEASURED  |  SAVED", juce::dontSendNotification);
    }
    else if (mode == 0 && processor.hasStableDetection())
    {
        keyValue.setText (keyName (root, scaleMode), juce::dontSendNotification);
        keyDetail.setText ("Confidence: " + confidenceText (keyConfidence) + "  |  Awaiting explicit save", juce::dontSendNotification);
        keyNotes.setText ("Scale preview available only after valid analysis.", juce::dontSendNotification);
        keyStatus.setText ("LIVE PREVIEW - NOT SAVED", juce::dontSendNotification);
    }
    else
    {
        keyValue.setText ("UNCERTAIN", juce::dontSendNotification);
        keyDetail.setText ("No scale is recommended until a valid beat result is saved.", juce::dontSendNotification);
        keyNotes.setText ("No fallback key is used.", juce::dontSendNotification);
        keyStatus.setText (mode == 0 && bpm > 0.0 ? "LOW CONFIDENCE" : "WAITING FOR BEAT CAPTURE", juce::dontSendNotification);
    }
    keyStatus.setColour (juce::Label::textColourId, statusColour (keyStatus.getText()));

    const auto vocalConfidence = vocalSaved ? processor.getSavedVocalConfidence() : processor.getVocalConfidence();
    const auto vocalLow = vocalSaved ? processor.getSavedVocalLowestMidi() : processor.getVocalLowestMidi();
    const auto vocalHigh = vocalSaved ? processor.getSavedVocalHighestMidi() : processor.getVocalHighestMidi();
    const auto vocalAverage = vocalSaved ? processor.getSavedVocalAverageMidi() : processor.getVocalAverageMidi();
    const auto voiced = vocalSaved ? processor.getSavedVocalVoicedPercent() : processor.getVocalVoicedPercent();
    if (vocalSaved)
    {
        vocalValue.setText (midiName (vocalAverage), juce::dontSendNotification);
        vocalDetail.setText ("Detected range: " + midiName (vocalLow) + " - " + midiName (vocalHigh)
            + "  |  Median pitch: " + midiName (vocalAverage)
            + "\nVoiced audio: " + confidenceText (voiced) + "  |  Pitch confidence: " + confidenceText (vocalConfidence)
            + "  |  Sustained: " + confidenceText (processor.getSavedVocalSustainedPercent()), juce::dontSendNotification);
        vocalStatus.setText ("MEASURED  |  SAVED", juce::dontSendNotification);
    }
    else if (mode == 1 && vocalConfidence >= 0.55f && voiced >= 0.20f)
    {
        vocalValue.setText (midiName (vocalAverage), juce::dontSendNotification);
        vocalDetail.setText ("Range preview: " + midiName (vocalLow) + " - " + midiName (vocalHigh)
            + "  |  Voiced: " + confidenceText (voiced)
            + "\nPitch confidence: " + confidenceText (vocalConfidence) + ". Save to use in review.", juce::dontSendNotification);
        vocalStatus.setText ("LIVE PREVIEW - NOT SAVED", juce::dontSendNotification);
    }
    else
    {
        vocalValue.setText ("INVALID", juce::dontSendNotification);
        vocalDetail.setText ("No stable voiced pitch detected. No vocal range or Auto-Tune settings are saved from an invalid result.", juce::dontSendNotification);
        vocalStatus.setText (mode == 1 && live ? "LISTENING" : "NO VALID VOCAL RESULT", juce::dontSendNotification);
    }
    vocalStatus.setColour (juce::Label::textColourId, statusColour (vocalStatus.getText()));

    const auto vocalFit = currentVocalFit();
    if (vocalFit.ready)
    {
        recommendationValue.setText (juce::String (vocalFit.keyScale), juce::dontSendNotification);
        recommendationDetail.setText ("MEASURED: " + juce::String (vocalFit.keyScale) + " | " + juce::String (vocalFit.vocalRange)
            + "\nRECOMMENDED: " + (vocalFit.classicMode ? "Classic" : "Modern")
            + "  |  Retune " + juce::String (vocalFit.retuneSpeedMs) + " ms\nHumanize " + juce::String (vocalFit.humanize)
            + "  |  Flex-Tune " + juce::String (vocalFit.flexTune)
            + "  |  " + juce::String (vocalFit.processingMode)
            + "\nWhy: " + juce::String (vocalFit.rationale) + " Starting point only; TuneRite does not control Auto-Tune directly.", juce::dontSendNotification);
        recommendationStatus.setText (juce::String (vocalFit.status), juce::dontSendNotification);
    }
    else
    {
        recommendationValue.setText (vocalFit.keyScale.empty() ? "LOCKED" : juce::String (vocalFit.keyScale), juce::dontSendNotification);
        recommendationDetail.setText (juce::String (vocalFit.rationale), juce::dontSendNotification);
        recommendationStatus.setText (juce::String (vocalFit.status), juce::dontSendNotification);
    }
    recommendationStatus.setColour (juce::Label::textColourId, statusColour (vocalFit.ready ? "VALID" : "LOW"));

    if (mode == 0)
    {
        workflowLabel.setText ("1. CAPTURE BEAT   ->   2. CAPTURE VOCAL   ->   3. REVIEW SETTINGS", juce::dontSendNotification);
        instructionLabel.setText ("Mute vocals. Press Analyze Beat, play at least 12 seconds, then press Stop and Save; it auto-finishes at 16 seconds.", juce::dontSendNotification);
        analyzeButton.setButtonText ("ANALYZE BEAT"); stopButton.setButtonText ("FINISH CAPTURE"); saveButton.setButtonText ("SAVE BEAT RESULT"); clearButton.setButtonText ("CLEAR BEAT");
    }
    else if (mode == 1)
    {
        workflowLabel.setText ("1. CAPTURE BEAT   ->   2. CAPTURE VOCAL   ->   3. REVIEW SETTINGS", juce::dontSendNotification);
        instructionLabel.setText ("Mute the beat. Press Analyze Vocal, play isolated voice for at least 12 seconds, then press Stop and Save; it auto-finishes at 16 seconds.", juce::dontSendNotification);
        analyzeButton.setButtonText ("ANALYZE VOCAL"); stopButton.setButtonText ("FINISH CAPTURE"); saveButton.setButtonText ("SAVE VOCAL RESULT"); clearButton.setButtonText ("CLEAR VOCAL");
    }
    else
    {
        workflowLabel.setText ("1. CAPTURE BEAT   ->   2. CAPTURE VOCAL   ->   3. REVIEW SETTINGS", juce::dontSendNotification);
        instructionLabel.setText (beatKeySaved && vocalSaved ? "Both valid results are saved. Review and copy starting settings." : "Review needs a saved beat key and saved vocal result. BPM may be saved separately.", juce::dontSendNotification);
        analyzeButton.setButtonText ("REVIEW COMBINED"); stopButton.setButtonText ("FINISH CAPTURE"); saveButton.setButtonText ("SAVED RESULTS ONLY"); clearButton.setButtonText ("CLEAR RESULTS");
    }

    const auto callbackText = "callbacks " + juce::String (static_cast<juce::int64> (processor.getAudioCallbackCount()))
        + " | block " + juce::String (processor.getLastAudioBlockSize());
    visualStatusLabel.setText (live ? "CAPTURE DIAGNOSTIC: " + callbackText + " | buffer " + juce::String (processor.getAnalysisDuration(), 1) + "s | signal " + juce::String (processor.getCapturedSignalSeconds(), 1) + "s | minimum finish 12.0s" : "INPUT HISTORY: " + callbackText + " | real meter samples only. No decorative animation.", juce::dontSendNotification);
    updateActionStates();
}

void KeyBridgeAudioProcessorEditor::updateActionStates()
{
    const auto mode = processor.getAnalysisMode();
    const auto beatReady = processor.hasStableDetection() && ! processor.isAnalysisActive();
    const auto vocalReady = processor.getVocalConfidence() >= 0.55f && processor.getVocalVoicedPercent() >= 0.20f && ! processor.isAnalysisActive();
    const auto bothSaved = processor.hasSavedBeatKey() && processor.hasValidVocalResult();
    juce::ignoreUnused (beatReady, vocalReady);
    analyzeButton.setEnabled (mode != 2 && ! processor.isAnalysisActive());
    stopButton.setEnabled (processor.isAnalysisActive() && processor.getCaptureState() == 2);
    // Results are saved by the worker at publication time; no manual save action is required.
    saveButton.setVisible (false);
    saveButton.setEnabled (false);
    clearButton.setEnabled (mode == 0 ? processor.hasSavedBeatResult() : mode == 1 ? processor.hasSavedVocalResult() : bothSaved);
    copyBpmButton.setEnabled (processor.hasSavedBeatTempo());
    copySettingsButton.setEnabled (bothSaved);
    copyReportButton.setEnabled (processor.hasSavedBeatResult() || processor.hasSavedVocalResult());
}

void KeyBridgeAudioProcessorEditor::copyDetectedBpm()
{
    if (! processor.hasSavedBeatTempo())
    {
        bpmStatus.setText ("CAPTURE A VALID BPM FIRST", juce::dontSendNotification);
        return;
    }
    juce::SystemClipboard::copyTextToClipboard (juce::String (processor.getSavedBeatBpm(), 2));
    bpmStatus.setText ("SAVED BPM COPIED", juce::dontSendNotification);
}

void KeyBridgeAudioProcessorEditor::copySettings()
{
    const auto vocalFit = currentVocalFit();
    if (! vocalFit.ready)
    {
        recommendationStatus.setText (juce::String (vocalFit.status), juce::dontSendNotification);
        return;
    }
    const auto text = juce::String ("TuneRite Auto-Tune Starting Point\n")
        + "Measured key/scale: " + juce::String (vocalFit.keyScale) + "\n"
        + (processor.hasSavedBeatTempo() ? "Measured BPM: " + juce::String (processor.getSavedBeatBpm(), 2) + "\n" : "Measured BPM: unavailable\n")
        + "Measured vocal range: " + juce::String (vocalFit.vocalRange) + "\n"
        + "Recommended mode: " + (vocalFit.classicMode ? "Classic" : "Modern") + "\n"
        + "Recommended retune speed: " + juce::String (vocalFit.retuneSpeedMs) + " ms\n"
        + "Recommended humanize: " + juce::String (vocalFit.humanize) + "\n"
        + "Recommended Flex-Tune: " + juce::String (vocalFit.flexTune) + "\n"
        + "Processing: " + juce::String (vocalFit.processingMode) + "\n"
        + "Recommendation: starting point only; TuneRite does not control Auto-Tune directly.";
    juce::SystemClipboard::copyTextToClipboard (text);
    recommendationStatus.setText ("AUTO-TUNE STARTING POINT COPIED", juce::dontSendNotification);
}

void KeyBridgeAudioProcessorEditor::copyEngineerReport()
{
    juce::String text ("TuneRite Engineer Report\n");
    text += "Beat BPM saved: " + juce::String (processor.hasSavedBeatTempo() ? "yes" : "no") + "\n";
    if (processor.hasSavedBeatTempo())
    {
        text += "BPM: " + juce::String (processor.getSavedBeatBpm(), 2) + "\n";
        text += "BPM confidence: " + confidenceText (processor.getSavedBeatBpmConfidence()) + "\n";
    }
    text += "Beat key saved: " + juce::String (processor.hasSavedBeatKey() ? "yes" : "no") + "\n";
    if (processor.hasSavedBeatKey())
    {
        text += "Key: " + keyName (processor.getSavedBeatKey(), processor.getSavedBeatMode()) + "\n";
        text += "Key confidence: " + confidenceText (processor.getSavedBeatKeyConfidence()) + "\n";
    }
    else
    {
        text += "Key: UNKNOWN / UNCERTAIN\n";
    }
    text += "Vocal saved: " + juce::String (processor.hasSavedVocalResult() ? "yes" : "no") + "\n";
    if (processor.hasSavedVocalResult())
        text += "Vocal range: " + midiName (processor.getSavedVocalLowestMidi()) + " - " + midiName (processor.getSavedVocalHighestMidi()) + "\n";
    text += "TuneRite provides analysis and starting recommendations only; it does not alter audio or control Auto-Tune.";
    juce::SystemClipboard::copyTextToClipboard (text);
    visualStatusLabel.setText ("ENGINEER REPORT COPIED", juce::dontSendNotification);
}
