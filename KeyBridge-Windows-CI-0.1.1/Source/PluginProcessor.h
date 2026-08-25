#pragma once
#include <JuceHeader.h>

class KeyBridgeAudioProcessor final : public juce::AudioProcessor
{
public:
    KeyBridgeAudioProcessor();
    ~KeyBridgeAudioProcessor() override = default;

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "KeyBridge"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    double getHostBpm() const noexcept { return hostBpm.load (std::memory_order_relaxed); }
    int getDetectedKey() const noexcept { return detectedKey.load (std::memory_order_relaxed); }
    float getKeyConfidence() const noexcept { return keyConfidence.load (std::memory_order_relaxed); }

private:
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<int> detectedKey { 0 };
    std::atomic<float> keyConfidence { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBridgeAudioProcessor)
};
