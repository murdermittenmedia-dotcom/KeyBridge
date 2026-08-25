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
    const juce::String getName() const override { return "Tunerite"; }
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
    bool hasStableDetection() const noexcept { return hasStableDetectionFlag.load (std::memory_order_relaxed); }
    int getDetectedMode() const noexcept { return detectedMode.load (std::memory_order_relaxed); }
    float getKeyConfidence() const noexcept { return keyConfidence.load (std::memory_order_relaxed); }
    float getBpmConfidence() const noexcept { return bpmConfidence.load (std::memory_order_relaxed); }
    float getInputLevel() const noexcept { return inputLevel.load (std::memory_order_relaxed); }
    int getAnalysisFrames() const noexcept { return analysisFrames.load (std::memory_order_relaxed); }
    bool isAnalysisActive() const noexcept { return analysisEnabled.load (std::memory_order_relaxed); }
    float getCaptureProgress() const noexcept { return captureProgress.load (std::memory_order_relaxed); }
    void requestReferenceTone (int midiNote) noexcept;
    void startFreshAnalysis() noexcept;
    void setAnalysisEnabled (bool enabled) noexcept { analysisEnabled.store (enabled, std::memory_order_relaxed); }

private:
    void analyzeFrame();
    void estimateAudioBpm();

    double sampleRate = 44100.0;
    static constexpr int fftOrder = 13;
    static constexpr int fftSize = 1 << fftOrder;
    std::unique_ptr<juce::dsp::FFT> fft;
    juce::HeapBlock<float, true> fftData;
    std::array<float, 12> chroma{};
    int fftFill = 0;
    int analysisFrameCount = 0;
    float previousEnergy = 0.0f;
    int samplesSinceOnset = 0;
    float adaptiveEnergy = 0.0001f;
    std::array<float, 256> energyHistory{};
    int energyHistoryWrite = 0;
    int energyHistoryCount = 0;
    int captureSamples = 0;
    bool oneShotMode = false;
    int candidateKey = 0;
    int candidateMode = 0;
    int candidateWins = 0;
    int stableKey = 0;
    int stableMode = 0;
    bool hasStableKey = false;

    std::atomic<bool> analysisEnabled { true };
    std::atomic<bool> resetRequested { false };
    std::atomic<bool> oneShotRequested { false };
    std::atomic<float> captureProgress { 0.0f };
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<double> detectedBpm { 0.0 };
    std::atomic<int> detectedKey { 0 };
    std::atomic<int> detectedMode { 0 };
    std::atomic<bool> hasStableDetectionFlag { false };
    std::atomic<float> keyConfidence { 0.0f };
    std::atomic<float> bpmConfidence { 0.0f };
    std::atomic<float> inputLevel { 0.0f };
    std::atomic<int> analysisFrames { 0 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBridgeAudioProcessor)
};
