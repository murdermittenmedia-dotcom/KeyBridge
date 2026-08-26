#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

#include <array>

class KeyBridgeAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer,
                                             private juce::ChangeListener
{
public:
    explicit KeyBridgeAudioProcessorEditor (KeyBridgeAudioProcessor&);
    ~KeyBridgeAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void ensureUiTimerRunning();
    void setMode (int mode);
    void refreshView();
    void refreshAppearance();
    void updateActionStates();
    void applyThemePreset();
    void applyColourSelector();
    void saveAppearance();
    void copyDetectedBpm();
    void copySettings();
    void copyEngineerReport();
    void setCaption (juce::Label&, const juce::String&);
    void addLabel (juce::Label&, float size, juce::Justification, juce::Colour);
    void addButton (juce::TextButton&, const juce::String&);
    juce::Colour accentColour() const;
    juce::Colour panelColour() const;
    juce::Colour backgroundColour() const;
    juce::Colour statusColour (const juce::String&) const;
    juce::String midiName (float midi) const;
    juce::String keyName (int root, int mode) const;

    KeyBridgeAudioProcessor& processor;

    juce::Label title, subtitle, inputStatus, analysisStatus, projectBpmLabel;
    juce::Label workflowLabel, instructionLabel, visualStatusLabel, meterCaption;
    juce::Label bpmTitle, bpmValue, bpmDetail, bpmStatus;
    juce::Label keyTitle, keyValue, keyDetail, keyNotes, keyStatus;
    juce::Label vocalTitle, vocalValue, vocalDetail, vocalStatus;
    juce::Label recommendationTitle, recommendationValue, recommendationDetail, recommendationStatus;
    juce::Label appearanceTitle, themeCaption, colourTargetCaption, opacityCaption, glowCaption, layoutCaption;

    juce::TextButton beatModeButton { "BEAT ONLY" };
    juce::TextButton vocalModeButton { "VOCAL ONLY" };
    juce::TextButton reviewModeButton { "REVIEW" };
    juce::TextButton analyzeButton { "ANALYZE BEAT" };
    juce::TextButton stopButton { "STOP" };
    juce::TextButton saveButton { "SAVE BEAT RESULT" };
    juce::TextButton clearButton { "CLEAR BEAT" };
    juce::TextButton copyBpmButton { "COPY BPM" };
    juce::TextButton copyReportButton { "COPY ENGINEER REPORT" };
    juce::TextButton copySettingsButton { "COPY AUTO-TUNE SETTINGS" };
    juce::TextButton resetButton { "RESET ALL" };
    juce::TextButton appearanceButton { "APPEARANCE" };
    juce::TextButton closeAppearanceButton { "CLOSE" };
    juce::TextButton saveThemeButton { "SAVE THEME" };
    juce::TextButton resetThemeButton { "RESET DEFAULT" };

    juce::ComboBox profileBox, genreBox, deliveryBox, vibeBox;
    juce::ComboBox themeBox, colourTargetBox;
    juce::Slider opacitySlider, glowSlider;
    juce::ToggleButton compactLayoutToggle { "Compact layout" };
    juce::ColourSelector colourSelector { juce::ColourSelector::showColourspace | juce::ColourSelector::showSliders | juce::ColourSelector::showAlphaChannel };

    bool appearanceOpen = false;
    std::array<float, 120> levelHistory {};
    float leftMeter = 0.0f;
    float rightMeter = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBridgeAudioProcessorEditor)
};
