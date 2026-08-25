#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>

class KeyBridgeAudioProcessor final : public juce::AudioProcessor
{
public:
    KeyBridgeAudioProcessor();
    ~KeyBridgeAudioProcessor() override = default;

    void prepareToPlay (double, int) override;
    void releaseResources() override;
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
    double getDetectedBpm() const noexcept { return detectedBpm.load (std::memory_order_relaxed); }
    int getDetectedKey() const noexcept { return detectedKey.load (std::memory_order_relaxed); }
    int getDetectedMode() const noexcept { return detectedMode.load (std::memory_order_relaxed); }
    float getKeyConfidence() const noexcept { return keyConfidence.load (std::memory_order_relaxed); }
    float getBpmConfidence() const noexcept { return bpmConfidence.load (std::memory_order_relaxed); }
    void requestReferenceTone (int midiNote) noexcept;
    void setAnalysisEnabled (bool enabled) noexcept { analysisEnabled.store (enabled, std::memory_order_relaxed); }

private:
    void analyzeFrame();

    double sampleRate = 44100.0;
    static constexpr int fftOrder = 11;
    std::unique_ptr<juce::dsp::FFT> fft;
    juce::HeapBlock<float, true> fftData;
    std::array<float, 12> chroma{};
    int fftFill = 0;
    int analysisFrameCount = 0;
    float previousEnergy = 0.0f;
    int samplesSinceOnset = 0;
    float adaptiveEnergy = 0.0001f;

    std::atomic<bool> analysisEnabled { true };
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<double> detectedBpm { 0.0 };
    std::atomic<int> detectedKey { 0 };
    std::atomic<int> detectedMode { 0 };
    std::atomic<float> keyConfidence { 0.0f };
    std::atomic<float> bpmConfidence { 0.0f };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBridgeAudioProcessor)
};
