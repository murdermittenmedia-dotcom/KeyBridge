#pragma once
#include <JuceHeader.h>
#include <array>
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
    void playReferenceTone (int midiNote);
    void copyDetectedBpm();
    void copySettings();

    KeyBridgeAudioProcessor& processor;
    juce::Label title, sectionLabel, keyLabel, bpmLabel, confidenceLabel, notesLabel, recommendationLabel, guidanceLabel, bpmActionLabel, beatStatusLabel, vocalStatusLabel, vocalMetricsLabel, settingsLabel;
    juce::Label profileLabel, rangeLabel, genreLabel, deliveryLabel, vibeLabel, displayLabel, themeLabel, analysisModeLabel, resultStatusLabel;
    juce::ComboBox profileBox, genreBox, deliveryBox, vibeBox, displayModeBox, themeBox, analysisModeBox;
    juce::Slider lowNoteSlider, highNoteSlider;
    juce::TextButton analyzeButton { "ANALYZE" }, holdButton { "HOLD" }, lockButton { "LOCK" }, setBpmButton { "COPY DETECTED BPM" }, copySettingsButton { "COPY SETTINGS" }, saveBeatButton { "SAVE BEAT RESULT" }, saveVocalButton { "SAVE VOCAL RESULT" }, clearBeatButton { "CLEAR BEAT" }, clearVocalButton { "CLEAR VOCAL" }, resetButton { "RESET ALL" };
    std::array<juce::TextButton, 12> noteButtons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBridgeAudioProcessorEditor)
};
