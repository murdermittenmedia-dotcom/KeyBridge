#pragma once

#include <JuceHeader.h>
#include "AnalysisCore.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

class KeyBridgeAudioProcessor final : public juce::AudioProcessor
{
public:
    KeyBridgeAudioProcessor();
    ~KeyBridgeAudioProcessor() override;

    void prepareToPlay (double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "TuneRite"; }
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

    double getHostBpm() const noexcept { return hostBpm.load (std::memory_order_relaxed); }
    double getDetectedBpm() const noexcept { return detectedBpm.load (std::memory_order_relaxed); }
    double getAlternativeBpm() const noexcept { return detectedAlternativeBpm.load (std::memory_order_relaxed); }
    int getDetectedKey() const noexcept { return detectedKey.load (std::memory_order_relaxed); }
    bool hasStableDetection() const noexcept { return hasStableDetectionFlag.load (std::memory_order_relaxed); }
    bool hasValidDetectedTempo() const noexcept { return detectedTempoValid.load (std::memory_order_relaxed); }
    bool hasValidDetectedKey() const noexcept { return detectedKeyValid.load (std::memory_order_relaxed); }
    int getDetectedMode() const noexcept { return detectedMode.load (std::memory_order_relaxed); }
    float getKeyConfidence() const noexcept { return keyConfidence.load (std::memory_order_relaxed); }
    float getBpmConfidence() const noexcept { return bpmConfidence.load (std::memory_order_relaxed); }
    float getInputLevel() const noexcept { return inputPeak.load (std::memory_order_relaxed); }
    float getLeftInputLevel() const noexcept { return leftInputPeak.load (std::memory_order_relaxed); }
    float getRightInputLevel() const noexcept { return rightInputPeak.load (std::memory_order_relaxed); }
    int getAnalysisFrames() const noexcept { return analysisFrames.load (std::memory_order_relaxed); }
    int getVocalFrames() const noexcept { return vocalFrames.load (std::memory_order_relaxed); }
    float getBeatRms() const noexcept { return beatRms.load (std::memory_order_relaxed); }
    float getVocalRms() const noexcept { return vocalRms.load (std::memory_order_relaxed); }
    float getAnalysisDuration() const noexcept { return analysisDuration.load (std::memory_order_relaxed); }
    float getVocalVoicedPercent() const noexcept { return vocalVoicedPercent.load (std::memory_order_relaxed); }
    bool isAnalysisActive() const noexcept { return captureActive.load (std::memory_order_relaxed) || workerBusy.load (std::memory_order_relaxed); }

    void setAnalysisMode (int mode) noexcept { analysisMode.store (juce::jlimit (0, 2, mode), std::memory_order_relaxed); }
    int getAnalysisMode() const noexcept { return analysisMode.load (std::memory_order_relaxed); }
    void startFreshAnalysis() noexcept;
    void stopAnalysis() noexcept;
    void setAnalysisEnabled (bool enabled) noexcept { analysisEnabled.store (enabled, std::memory_order_relaxed); }

    void saveBeatResult() noexcept;
    void saveVocalResult() noexcept;
    void clearBeatResult() noexcept;
    void clearVocalResult() noexcept;
    void resetAllResults() noexcept;

    bool hasSavedBeatResult() const noexcept { return savedBeatResult.load (std::memory_order_relaxed); }
    bool hasSavedVocalResult() const noexcept { return savedVocalResult.load (std::memory_order_relaxed); }
    bool hasValidBeatResult() const noexcept { return hasSavedBeatResult(); }
    bool hasValidVocalResult() const noexcept { return hasSavedVocalResult(); }
    int getSavedBeatKey() const noexcept { return savedBeatKey.load (std::memory_order_relaxed); }
    int getSavedBeatMode() const noexcept { return savedBeatMode.load (std::memory_order_relaxed); }
    double getSavedBeatBpm() const noexcept { return savedBeatBpm.load (std::memory_order_relaxed); }
    double getSavedBeatAlternativeBpm() const noexcept { return savedBeatAlternativeBpm.load (std::memory_order_relaxed); }
    float getSavedBeatKeyConfidence() const noexcept { return savedBeatKeyConfidence.load (std::memory_order_relaxed); }
    float getSavedBeatBpmConfidence() const noexcept { return savedBeatBpmConfidence.load (std::memory_order_relaxed); }
    float getSavedVocalLowestMidi() const noexcept { return savedVocalLowestMidi.load (std::memory_order_relaxed); }
    float getSavedVocalHighestMidi() const noexcept { return savedVocalHighestMidi.load (std::memory_order_relaxed); }
    float getSavedVocalAverageMidi() const noexcept { return savedVocalAverageMidi.load (std::memory_order_relaxed); }
    float getSavedVocalVoicedPercent() const noexcept { return savedVocalVoicedPercent.load (std::memory_order_relaxed); }
    float getSavedVocalConfidence() const noexcept { return savedVocalConfidence.load (std::memory_order_relaxed); }
    float getSavedVocalSustainedPercent() const noexcept { return savedVocalSustainedPercent.load (std::memory_order_relaxed); }
    float getSavedVocalNoteChangeSpeed() const noexcept { return savedVocalNoteChangeSpeed.load (std::memory_order_relaxed); }
    bool getSavedVocalMelodic() const noexcept { return savedVocalMelodic.load (std::memory_order_relaxed); }

    float getCaptureProgress() const noexcept { return captureProgress.load (std::memory_order_relaxed); }
    float getVocalInputLevel() const noexcept { return vocalInputPeak.load (std::memory_order_relaxed); }
    float getVocalConfidence() const noexcept { return vocalConfidence.load (std::memory_order_relaxed); }
    float getVocalLowestMidi() const noexcept { return vocalLowestMidi.load (std::memory_order_relaxed); }
    float getVocalHighestMidi() const noexcept { return vocalHighestMidi.load (std::memory_order_relaxed); }
    float getVocalAverageMidi() const noexcept { return vocalAverageMidi.load (std::memory_order_relaxed); }
    float getVocalPitchAccuracy() const noexcept { return vocalPitchAccuracy.load (std::memory_order_relaxed); }
    float getVocalVibrato() const noexcept { return vocalVibrato.load (std::memory_order_relaxed); }
    float getVocalSustainedPercent() const noexcept { return vocalSustainedPercent.load (std::memory_order_relaxed); }
    float getVocalNoteChangeSpeed() const noexcept { return vocalNoteChangeSpeed.load (std::memory_order_relaxed); }
    bool isVocalMelodic() const noexcept { return vocalMelodic.load (std::memory_order_relaxed); }

    std::uint32_t getAppearanceAccent() const noexcept { return appearanceAccent.load (std::memory_order_relaxed); }
    std::uint32_t getAppearancePanel() const noexcept { return appearancePanel.load (std::memory_order_relaxed); }
    std::uint32_t getAppearanceBackground() const noexcept { return appearanceBackground.load (std::memory_order_relaxed); }
    float getAppearancePanelOpacity() const noexcept { return appearancePanelOpacity.load (std::memory_order_relaxed); }
    float getAppearanceGlow() const noexcept { return appearanceGlow.load (std::memory_order_relaxed); }
    bool isCompactAppearance() const noexcept { return appearanceCompact.load (std::memory_order_relaxed); }
    void setAppearance (std::uint32_t accent, std::uint32_t panel, std::uint32_t background, float opacity, float glow, bool compact) noexcept;
    void resetAppearance() noexcept;

private:
    void workerLoop();
    void resetLiveResults() noexcept;
    void publishBeatResult (const tunerite::BeatAnalysisResult&, std::uint64_t generation);
    void publishVocalResult (const tunerite::VocalAnalysisResult&, std::uint64_t generation);

    static constexpr double captureSeconds = 16.0;
    double sampleRate = 44100.0;
    std::vector<float> captureBuffers[2];
    std::atomic<int> activeBuffer { 0 };
    std::atomic<int> capturedSamples { 0 };
    std::atomic<int> completedBuffer { -1 };
    std::atomic<int> completedSamples { 0 };
    std::atomic<int> completedMode { 0 };
    std::atomic<std::uint64_t> analysisGeneration { 0 };
    std::atomic<std::uint64_t> captureGeneration { 0 };
    std::atomic<std::uint64_t> completedGeneration { 0 };
    std::atomic<bool> captureRequested { false };
    std::atomic<bool> captureActive { false };
    std::atomic<bool> analysisEnabled { true };
    std::atomic<int> analysisMode { 0 };
    std::atomic<float> captureProgress { 0.0f };

    std::thread workerThread;
    std::mutex workerMutex;
    std::condition_variable workerWake;
    std::atomic<bool> workerExit { false };
    std::atomic<bool> workerJobReady { false };
    std::atomic<bool> workerBusy { false };

    std::atomic<double> hostBpm { 0.0 };
    std::atomic<double> detectedBpm { 0.0 };
    std::atomic<double> detectedAlternativeBpm { 0.0 };
    std::atomic<int> detectedKey { -1 };
    std::atomic<int> detectedMode { -1 };
    std::atomic<bool> hasStableDetectionFlag { false };
    std::atomic<bool> detectedTempoValid { false };
    std::atomic<bool> detectedKeyValid { false };
    std::atomic<float> keyConfidence { 0.0f };
    std::atomic<float> bpmConfidence { 0.0f };
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> leftInputPeak { 0.0f };
    std::atomic<float> rightInputPeak { 0.0f };
    std::atomic<float> beatRms { 0.0f };
    std::atomic<float> vocalRms { 0.0f };
    std::atomic<float> analysisDuration { 0.0f };
    std::atomic<int> analysisFrames { 0 };

    std::atomic<float> vocalInputPeak { 0.0f };
    std::atomic<float> vocalConfidence { 0.0f };
    std::atomic<float> vocalLowestMidi { 0.0f };
    std::atomic<float> vocalHighestMidi { 0.0f };
    std::atomic<float> vocalAverageMidi { 0.0f };
    std::atomic<float> vocalPitchAccuracy { 0.0f };
    std::atomic<float> vocalVibrato { 0.0f };
    std::atomic<float> vocalSustainedPercent { 0.0f };
    std::atomic<float> vocalNoteChangeSpeed { 0.0f };
    std::atomic<float> vocalVoicedPercent { 0.0f };
    std::atomic<int> vocalFrames { 0 };
    std::atomic<bool> vocalMelodic { false };

    std::atomic<bool> savedBeatResult { false };
    std::atomic<bool> savedVocalResult { false };
    std::atomic<int> savedBeatKey { -1 };
    std::atomic<int> savedBeatMode { -1 };
    std::atomic<double> savedBeatBpm { 0.0 };
    std::atomic<double> savedBeatAlternativeBpm { 0.0 };
    std::atomic<float> savedBeatKeyConfidence { 0.0f };
    std::atomic<float> savedBeatBpmConfidence { 0.0f };
    std::atomic<float> savedVocalLowestMidi { 0.0f };
    std::atomic<float> savedVocalHighestMidi { 0.0f };
    std::atomic<float> savedVocalAverageMidi { 0.0f };
    std::atomic<float> savedVocalVoicedPercent { 0.0f };
    std::atomic<float> savedVocalConfidence { 0.0f };
    std::atomic<float> savedVocalSustainedPercent { 0.0f };
    std::atomic<float> savedVocalNoteChangeSpeed { 0.0f };
    std::atomic<bool> savedVocalMelodic { false };

    std::atomic<std::uint32_t> appearanceAccent { 0xff55c7e8 };
    std::atomic<std::uint32_t> appearancePanel { 0xff17202c };
    std::atomic<std::uint32_t> appearanceBackground { 0xff0b1017 };
    std::atomic<float> appearancePanelOpacity { 0.94f };
    std::atomic<float> appearanceGlow { 0.35f };
    std::atomic<bool> appearanceCompact { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyBridgeAudioProcessor)
};
