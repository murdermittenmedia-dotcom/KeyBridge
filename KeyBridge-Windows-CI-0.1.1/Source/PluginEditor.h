#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class KeyBridgeAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit KeyBridgeAudioProcessorEditor (KeyBridgeAudioProcessor&);
    ~KeyBridgeAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

private:
    void timerCallback() override;
    void refreshRecommendation();
    void copyDetectedBpm();
    void copySettings();
    void updateModeControls();
    void setCaption (juce::Label&, const juce::String&);

    KeyBridgeAudioProcessor& processor;

    juce::Label title, subtitle, modeStatus;
    juce::Label beatCardTitle, vocalCardTitle, recommendationCardTitle;
    juce::Label beatStatus, vocalStatus, beatResultStatus, vocalResultStatus, recommendationStatus;
    juce::Label keyLabel, bpmLabel, confidenceLabel, notesLabel, beatMetrics;
    juce::Label vocalMetrics, guidanceLabel, settingsLabel;
    juce::Label profileCaption, genreCaption, deliveryCaption, vibeCaption;

    juce::ComboBox analysisModeBox, profileBox, genreBox, deliveryBox, vibeBox;

    juce::TextButton analyzeButton { "ANALYZE" };
    juce::TextButton saveButton { "SAVE RESULT" };
    juce::TextButton clearBeatButton { "CLEAR BEAT" };
    juce::TextButton clearVocalButton { "CLEAR VOCAL" };
    juce::TextButton resetButton { "RESET ALL" };
    juce::TextButton copyBpmButton { "COPY BPM" };
    juce::TextButton copySettingsButton { "COPY SETTINGS" };
    juce::TextButton holdButton { "HOLD" };
    juce::TextButton lockButton { "LOCK" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBridgeAudioProcessorEditor)
};
