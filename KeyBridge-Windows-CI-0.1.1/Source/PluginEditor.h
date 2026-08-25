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

    KeyBridgeAudioProcessor& processor;
    juce::Label title, sectionLabel, keyLabel, bpmLabel, confidenceLabel, notesLabel, recommendationLabel, guidanceLabel, bpmActionLabel;
    juce::Label profileLabel, rangeLabel, genreLabel, deliveryLabel, vibeLabel, displayLabel, themeLabel;
    juce::ComboBox profileBox, genreBox, deliveryBox, vibeBox, displayModeBox, themeBox;
    juce::Slider lowNoteSlider, highNoteSlider;
    juce::TextButton analyzeButton { "ANALYZE" }, holdButton { "HOLD" }, lockButton { "LOCK" }, setBpmButton { "COPY DETECTED BPM" };
    std::array<juce::TextButton, 12> noteButtons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBridgeAudioProcessorEditor)
};
