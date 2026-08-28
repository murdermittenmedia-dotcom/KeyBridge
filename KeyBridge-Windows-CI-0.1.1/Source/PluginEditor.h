#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "VocalFit.h"

#include <array>

class KeyBridgeAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
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
    void ensureUiTimerRunning();
    void setPage (int page);
    void selectInput (int mode);
    void refreshView();
    void copySettings();
    void addLabel (juce::Label&, float size, juce::Justification, juce::Colour);
    void addButton (juce::TextButton&, const juce::String&);
    juce::Colour accentColour() const;
    juce::Colour panelColour() const;
    juce::Colour statusColour (const juce::String&) const;
    juce::String midiName (float midi) const;
    juce::String keyName (int root, int mode) const;
    juce::String rootName (int root) const;
    juce::String scaleNotes (int root, int mode, bool similarOnly) const;
    tunerite::VocalFitRecommendation currentVocalFit() const;

    KeyBridgeAudioProcessor& processor;
    juce::Image brandLogo;
    int activePage = 0;       // 0 = Analyze, 1 = Auto-Tune Recommendations
    int selectedInput = 0;    // 0 = beat, 1 = vocal, 2 = combined review

    juce::Label title, subtitle, analysisStatus, footerStatus;
    juce::Label bpmCaption, bpmValue, keyCaption, keyValue, scaleValue, confidenceCaption, confidenceValue;
    juce::Label targetGenderCaption, targetGenreCaption, targetMoodCaption;
    juce::Label summaryKeyCaption, summaryKeyValue, summaryScaleCaption, summaryScaleValue;
    juce::Label similarNotesCaption, similarNotesValue, inputTypeCaption, inputTypeValue;
    juce::Label summaryConfidenceCaption, summaryConfidenceValue;
    juce::Label settingsHeading, inKeyHeading, inKeyNotes, lowestCaption, lowestValue, comfortCaption, comfortValue;
    juce::Label guidanceHeading, guidanceText;
    std::array<juce::Label, 6> settingCaptions;
    std::array<juce::Label, 6> settingValues;

    juce::TextButton analyzePageButton { "ANALYZE" };
    juce::TextButton recommendationsPageButton { "AUTOTUNE RECOMMENDATIONS" };
    juce::TextButton helpButton { "?" };
    juce::TextButton beatInputButton { "BEAT ONLY" };
    juce::TextButton vocalInputButton { "VOCALS ONLY" };
    juce::TextButton combinedInputButton { "COMBINED" };
    juce::TextButton analyzeButton { "ANALYZE" };
    juce::TextButton clearButton { "CLEAR" };
    juce::TextButton copySettingsButton { "COPY SETTINGS" };

    juce::ComboBox profileBox, genreBox, vibeBox;
    std::array<float, 120> levelHistory {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBridgeAudioProcessorEditor)
};
