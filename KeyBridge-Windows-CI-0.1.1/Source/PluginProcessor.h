#pragma once
#include <string.h>
#include <cstring>
#include <cmath>
#include <JuceHeader.h>

class KeyBridgeAudioProcessor : public juce::AudioProcessor
{
public:
    KeyBridgeAudioProcessor();
    ~KeyBridgeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
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
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    double getHostBpm() const noexcept { return hostBpm.load(); }
    int getDetectedKey() const noexcept { return detectedKey.load(); }
    float getKeyConfidence() const noexcept { return keyConfidence.load(); }
    void setAnalysisEnabled (bool enabled) noexcept { analysisEnabled.store(enabled); }
    void requestReferenceTone (int midiNote) noexcept { toneFrequency.store (440.0 * std::pow (2.0, (midiNote - 69) / 12.0)); toneSamplesRemaining.store (static_cast<int> (sampleRate * 0.65)); }

private:
    void analyzeFrame();
    double sampleRate = 44100.0;
    int fftOrder = 11;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::array<float, 2048> fftData{};
    std::array<float, 12> chroma{};
    int fftFill = 0;
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<int> detectedKey { 0 };
    std::atomic<float> keyConfidence { 0.0f };
    std::atomic<bool> analysisEnabled { true };
    std::atomic<double> toneFrequency { 0.0 };
    std::atomic<int> toneSamplesRemaining { 0 };
    double tonePhase = 0.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBridgeAudioProcessor)
};
