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

private:
    void timerCallback() override;
    void refreshRecommendation();
    void playReferenceTone (int midiNote);

    KeyBridgeAudioProcessor& processor;
    juce::Label title, keyLabel, bpmLabel, confidenceLabel, notesLabel, recommendationLabel;
    juce::Label profileLabel, rangeLabel, genreLabel, vibeLabel;
    juce::ComboBox profileBox, genreBox, vibeBox;
    juce::Slider lowNoteSlider, highNoteSlider;
    juce::TextButton analyzeButton { "ANALYZE" }, holdButton { "HOLD" };
    std::array<std::unique_ptr<juce::TextButton>, 12> noteButtons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBridgeAudioProcessorEditor)
};
