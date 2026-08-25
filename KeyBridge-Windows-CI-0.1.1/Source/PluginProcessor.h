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
    int getVocalFrames() const noexcept { return vocalFrames.load (std::memory_order_relaxed); }
    float getBeatRms() const noexcept { return beatRms.load (std::memory_order_relaxed); }
    float getVocalRms() const noexcept { return vocalRms.load (std::memory_order_relaxed); }
    float getAnalysisDuration() const noexcept { return analysisDuration.load (std::memory_order_relaxed); }
    float getVocalVoicedPercent() const noexcept { return vocalVoicedPercent.load (std::memory_order_relaxed); }
    bool isAnalysisActive() const noexcept { return analysisEnabled.load (std::memory_order_relaxed); }
    void setAnalysisMode (int mode) noexcept { analysisMode.store (juce::jlimit (0, 2, mode), std::memory_order_relaxed); }
    int getAnalysisMode() const noexcept { return analysisMode.load (std::memory_order_relaxed); }
    void saveBeatResult() noexcept;
    void saveVocalResult() noexcept;
    void clearBeatResult() noexcept;
    void clearVocalResult() noexcept;
    void resetAllResults() noexcept;
    bool hasSavedBeatResult() const noexcept { return savedBeatResult.load (std::memory_order_relaxed); }
    bool hasSavedVocalResult() const noexcept { return savedVocalResult.load (std::memory_order_relaxed); }
    int getSavedBeatKey() const noexcept { return savedBeatKey.load (std::memory_order_relaxed); }
    int getSavedBeatMode() const noexcept { return savedBeatMode.load (std::memory_order_relaxed); }
    double getSavedBeatBpm() const noexcept { return savedBeatBpm.load (std::memory_order_relaxed); }
    float getSavedBeatKeyConfidence() const noexcept { return savedBeatKeyConfidence.load (std::memory_order_relaxed); }
    float getSavedBeatBpmConfidence() const noexcept { return savedBeatBpmConfidence.load (std::memory_order_relaxed); }
    float getSavedVocalLowestMidi() const noexcept { return savedVocalLowestMidi.load (std::memory_order_relaxed); }
    float getSavedVocalHighestMidi() const noexcept { return savedVocalHighestMidi.load (std::memory_order_relaxed); }
    float getSavedVocalConfidence() const noexcept { return savedVocalConfidence.load (std::memory_order_relaxed); }
    float getSavedVocalSustainedPercent() const noexcept { return savedVocalSustainedPercent.load (std::memory_order_relaxed); }
    float getSavedVocalNoteChangeSpeed() const noexcept { return savedVocalNoteChangeSpeed.load (std::memory_order_relaxed); }
    bool getSavedVocalMelodic() const noexcept { return savedVocalMelodic.load (std::memory_order_relaxed); }
    float getCaptureProgress() const noexcept { return captureProgress.load (std::memory_order_relaxed); }
    float getVocalInputLevel() const noexcept { return vocalInputLevel.load (std::memory_order_relaxed); }
    float getVocalConfidence() const noexcept { return vocalConfidence.load (std::memory_order_relaxed); }
    float getVocalLowestMidi() const noexcept { return vocalLowestMidi.load (std::memory_order_relaxed); }
    float getVocalHighestMidi() const noexcept { return vocalHighestMidi.load (std::memory_order_relaxed); }
    float getVocalAverageMidi() const noexcept { return vocalAverageMidi.load (std::memory_order_relaxed); }
    float getVocalPitchAccuracy() const noexcept { return vocalPitchAccuracy.load (std::memory_order_relaxed); }
    float getVocalVibrato() const noexcept { return vocalVibrato.load (std::memory_order_relaxed); }
    float getVocalSustainedPercent() const noexcept { return vocalSustainedPercent.load (std::memory_order_relaxed); }
    float getVocalNoteChangeSpeed() const noexcept { return vocalNoteChangeSpeed.load (std::memory_order_relaxed); }
    bool isVocalMelodic() const noexcept { return vocalMelodic.load (std::memory_order_relaxed); }
    void requestReferenceTone (int midiNote) noexcept;
    void startFreshAnalysis() noexcept;
    void setAnalysisEnabled (bool enabled) noexcept { analysisEnabled.store (enabled, std::memory_order_relaxed); }

private:
    void analyzeFrame();
    void estimateAudioBpm();
    void analyzeVocalBlock (const juce::AudioBuffer<float>& vocalBuffer);

    double sampleRate = 44100.0;
    static constexpr int fftOrder = 13;
    static constexpr int fftSize = 1 << fftOrder;
    std::unique_ptr<juce::dsp::FFT> fft;
    juce::HeapBlock<float, true> fftData;
    std::array<float, 12> chroma{};
    std::array<float, 2048> vocalPitchBuffer{};
    int vocalPitchFill = 0;
    float vocalMinMidi = 127.0f;
    float vocalMaxMidi = 0.0f;
    float vocalMidiSum = 0.0f;
    int vocalPitchCount = 0;
    int vocalSustainedBlocks = 0;
    int vocalTotalBlocks = 0;
    int vocalNoteChanges = 0;
    float vocalPreviousMidi = 0.0f;
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
    std::atomic<int> analysisMode { 0 }; // 0 Beat Only, 1 Vocal Only, 2 Combined Recommendation
    std::atomic<bool> savedBeatResult { false };
    std::atomic<bool> savedVocalResult { false };
    std::atomic<int> savedBeatKey { 0 };
    std::atomic<int> savedBeatMode { 0 };
    std::atomic<double> savedBeatBpm { 0.0 };
    std::atomic<float> savedBeatKeyConfidence { 0.0f };
    std::atomic<float> savedBeatBpmConfidence { 0.0f };
    std::atomic<float> savedVocalLowestMidi { 0.0f };
    std::atomic<float> savedVocalHighestMidi { 0.0f };
    std::atomic<float> savedVocalConfidence { 0.0f };
    std::atomic<float> savedVocalSustainedPercent { 0.0f };
    std::atomic<float> savedVocalNoteChangeSpeed { 0.0f };
    std::atomic<bool> savedVocalMelodic { false };
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
    std::atomic<float> beatRms { 0.0f };
    std::atomic<float> vocalRms { 0.0f };
    std::atomic<float> analysisDuration { 0.0f };
    std::atomic<float> vocalVoicedPercent { 0.0f };
    std::atomic<int> vocalFrames { 0 };
    std::atomic<float> vocalInputLevel { 0.0f };
    std::atomic<float> vocalConfidence { 0.0f };
    std::atomic<float> vocalLowestMidi { 0.0f };
    std::atomic<float> vocalHighestMidi { 0.0f };
    std::atomic<float> vocalAverageMidi { 0.0f };
    std::atomic<float> vocalPitchAccuracy { 0.0f };
    std::atomic<float> vocalVibrato { 0.0f };
    std::atomic<float> vocalSustainedPercent { 0.0f };
    std::atomic<float> vocalNoteChangeSpeed { 0.0f };
    std::atomic<bool> vocalMelodic { false };
    std::atomic<int> analysisFrames { 0 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBridgeAudioProcessor)
};
