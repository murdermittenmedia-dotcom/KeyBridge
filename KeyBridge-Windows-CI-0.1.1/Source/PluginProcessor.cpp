#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

KeyBridgeAudioProcessor::KeyBridgeAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    workerThread = std::thread ([this] { workerLoop(); });
}

KeyBridgeAudioProcessor::~KeyBridgeAudioProcessor()
{
    workerExit.store (true, std::memory_order_release);
    workerWake.notify_one();
    if (workerThread.joinable())
        workerThread.join();
}

void KeyBridgeAudioProcessor::prepareToPlay (double newSampleRate, int)
{
    analysisGeneration.fetch_add (1, std::memory_order_acq_rel);
    captureActive.store (false, std::memory_order_release);
    captureRequested.store (false, std::memory_order_release);
    workerWake.notify_one();
    while (workerBusy.load (std::memory_order_acquire) || workerJobReady.load (std::memory_order_acquire))
        juce::Thread::sleep (1);

    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    const auto capacity = static_cast<size_t> (std::ceil (sampleRate * captureSeconds));
    captureBuffers[0].assign (capacity, 0.0f);
    captureBuffers[1].assign (capacity, 0.0f);
    capturedSamples.store (0, std::memory_order_relaxed);
    completedSamples.store (0, std::memory_order_relaxed);
    completedBuffer.store (-1, std::memory_order_relaxed);
    captureRequested.store (false, std::memory_order_relaxed);
    finishCaptureRequested.store (false, std::memory_order_relaxed);
    captureActive.store (false, std::memory_order_relaxed);
    captureState.store (idle, std::memory_order_relaxed);
    capturedSignalSamples.store (0, std::memory_order_relaxed);
    capturedSignalSeconds.store (0.0f, std::memory_order_relaxed);
    captureGeneration.store (analysisGeneration.load (std::memory_order_relaxed), std::memory_order_relaxed);
    completedGeneration.store (0, std::memory_order_relaxed);
    captureProgress.store (0.0f, std::memory_order_relaxed);
    resetLiveResults();
}

void KeyBridgeAudioProcessor::releaseResources()
{
    captureActive.store (false, std::memory_order_relaxed);
}

bool KeyBridgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == juce::AudioChannelSet::stereo() && output == juce::AudioChannelSet::stereo();
}

void KeyBridgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;
    audioCallbackCount.fetch_add (1, std::memory_order_relaxed);
    lastAudioBlockSize.store (buffer.getNumSamples(), std::memory_order_relaxed);

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (auto position = getPlayHead() != nullptr ? getPlayHead()->getPosition() : juce::Optional<juce::AudioPlayHead::PositionInfo> {})
        if (position->getBpm().hasValue())
            hostBpm.store (juce::jlimit (1.0, 999.0, *position->getBpm()), std::memory_order_relaxed);

    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
        return;

    const auto* left = buffer.getReadPointer (0);
    const auto* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : left;
    float blockPeak = 0.0f;
    float leftPeak = 0.0f;
    float rightPeak = 0.0f;
    double energy = 0.0;
    double leftEnergy = 0.0;
    double rightEnergy = 0.0;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto mono = 0.5f * (left[i] + right[i]);
        leftPeak = juce::jmax (leftPeak, std::abs (left[i]));
        rightPeak = juce::jmax (rightPeak, std::abs (right[i]));
        blockPeak = juce::jmax (blockPeak, std::abs (mono));
        energy += static_cast<double> (mono) * static_cast<double> (mono);
        leftEnergy += static_cast<double> (left[i]) * left[i];
        rightEnergy += static_cast<double> (right[i]) * right[i];
    }
    const auto rms = static_cast<float> (std::sqrt (energy / static_cast<double> (buffer.getNumSamples())));
    const auto strongestStereoEnergy = std::max (leftEnergy, rightEnergy);
    const bool destructiveStereoCancellation = buffer.getNumChannels() > 1
        && strongestStereoEnergy > 1.0e-12
        && energy < strongestStereoEnergy * 0.18;
    const bool captureLeftOnly = destructiveStereoCancellation && leftEnergy >= rightEnergy;
    const auto captureEnergy = destructiveStereoCancellation ? strongestStereoEnergy : energy;
    const auto captureRms = static_cast<float> (std::sqrt (captureEnergy / static_cast<double> (buffer.getNumSamples())));
    inputPeak.store (0.85f * inputPeak.load (std::memory_order_relaxed) + 0.15f * blockPeak, std::memory_order_relaxed);
    leftInputPeak.store (0.85f * leftInputPeak.load (std::memory_order_relaxed) + 0.15f * leftPeak, std::memory_order_relaxed);
    rightInputPeak.store (0.85f * rightInputPeak.load (std::memory_order_relaxed) + 0.15f * rightPeak, std::memory_order_relaxed);

    if (captureRequested.load (std::memory_order_acquire)
        && ! captureActive.load (std::memory_order_relaxed)
        && ! workerBusy.load (std::memory_order_relaxed)
        && ! workerJobReady.load (std::memory_order_relaxed)
        && captureRequested.exchange (false, std::memory_order_acq_rel))
    {
        const auto nextBuffer = 1 - juce::jmax (0, completedBuffer.load (std::memory_order_relaxed));
        activeBuffer.store (nextBuffer, std::memory_order_relaxed);
        capturedSamples.store (0, std::memory_order_relaxed);
        captureProgress.store (0.0f, std::memory_order_relaxed);
        capturedSignalSamples.store (0, std::memory_order_relaxed);
        capturedSignalSeconds.store (0.0f, std::memory_order_relaxed);
        finishCaptureRequested.store (false, std::memory_order_relaxed);
        captureGeneration.store (analysisGeneration.fetch_add (1, std::memory_order_acq_rel) + 1, std::memory_order_release);
        resetLiveResults();
        captureState.store (capturing, std::memory_order_release);
        captureActive.store (true, std::memory_order_release);
    }

    if (! captureActive.load (std::memory_order_acquire) || ! analysisEnabled.load (std::memory_order_relaxed))
        return;

    const auto mode = analysisMode.load (std::memory_order_relaxed);
    if (mode == 2)
        return;

    const auto bufferIndex = activeBuffer.load (std::memory_order_relaxed);
    auto& destination = captureBuffers[bufferIndex];
    auto write = capturedSamples.load (std::memory_order_relaxed);
    const auto capacity = static_cast<int> (destination.size());
    const auto available = juce::jmax (0, capacity - write);
    const auto toCopy = juce::jmin (available, buffer.getNumSamples());
    for (int i = 0; i < toCopy; ++i)
        destination[static_cast<size_t> (write + i)] = destructiveStereoCancellation
            ? (captureLeftOnly ? left[i] : right[i])
            : 0.5f * (left[i] + right[i]);
    write += toCopy;
    capturedSamples.store (write, std::memory_order_relaxed);
    if (captureRms > 0.0005f)
        capturedSignalSamples.fetch_add (toCopy, std::memory_order_relaxed);
    capturedSignalSeconds.store (static_cast<float> (capturedSignalSamples.load (std::memory_order_relaxed) / sampleRate), std::memory_order_relaxed);
    analysisFrames.fetch_add (1, std::memory_order_relaxed);
    analysisDuration.store (static_cast<float> (write / sampleRate), std::memory_order_relaxed);
    captureProgress.store (capacity > 0 ? static_cast<float> (write) / static_cast<float> (capacity) : 0.0f, std::memory_order_relaxed);

    if (mode == 0)
        beatRms.store (0.85f * beatRms.load (std::memory_order_relaxed) + 0.15f * rms, std::memory_order_relaxed);
    else
    {
        vocalInputPeak.store (0.85f * vocalInputPeak.load (std::memory_order_relaxed) + 0.15f * blockPeak, std::memory_order_relaxed);
        vocalRms.store (0.85f * vocalRms.load (std::memory_order_relaxed) + 0.15f * rms, std::memory_order_relaxed);
    }

    const auto minimumSamples = static_cast<int> (std::ceil (sampleRate * minimumCaptureSeconds));
    const auto finishRequested = finishCaptureRequested.load (std::memory_order_acquire);
    if (finishRequested && write < minimumSamples)
    {
        finishCaptureRequested.store (false, std::memory_order_release);
        captureState.store (captureRms > 0.0005f ? insufficientAudio : noSignal, std::memory_order_release);
    }
    else if (write >= capacity || (finishRequested && write >= minimumSamples))
    {
        finishCaptureRequested.store (false, std::memory_order_release);
        captureActive.store (false, std::memory_order_release);
        captureState.store (processing, std::memory_order_release);
        completedBuffer.store (bufferIndex, std::memory_order_release);
        completedSamples.store (write, std::memory_order_release);
        completedMode.store (mode, std::memory_order_release);
        completedGeneration.store (captureGeneration.load (std::memory_order_acquire), std::memory_order_release);
        workerJobReady.store (true, std::memory_order_release);
        workerWake.notify_one();
    }
}

void KeyBridgeAudioProcessor::workerLoop()
{
    while (! workerExit.load (std::memory_order_acquire))
    {
        std::unique_lock<std::mutex> lock (workerMutex);
        workerWake.wait (lock, [this]
        {
            return workerExit.load (std::memory_order_acquire) || workerJobReady.load (std::memory_order_acquire);
        });
        if (workerExit.load (std::memory_order_acquire))
            break;

        const auto bufferIndex = completedBuffer.load (std::memory_order_acquire);
        const auto sampleCount = completedSamples.load (std::memory_order_acquire);
        const auto mode = completedMode.load (std::memory_order_acquire);
        const auto generation = completedGeneration.load (std::memory_order_acquire);
        workerJobReady.store (false, std::memory_order_release);
        workerBusy.store (true, std::memory_order_release);
        lock.unlock();

        if (bufferIndex >= 0 && bufferIndex < 2 && sampleCount > 0)
        {
            const auto& buffer = captureBuffers[bufferIndex];
            std::vector<float> view (buffer.begin(), buffer.begin() + juce::jmin (sampleCount, static_cast<int> (buffer.size())));
            if (mode == 0)
                publishBeatResult (tunerite::AnalysisCore::analyzeBeat (view, sampleRate), generation);
            else if (mode == 1)
                publishVocalResult (tunerite::AnalysisCore::analyzeVocal (view, sampleRate), generation);
        }
        workerBusy.store (false, std::memory_order_release);
        if (generation == analysisGeneration.load (std::memory_order_acquire))
            captureState.store (idle, std::memory_order_release);
    }
}

void KeyBridgeAudioProcessor::publishBeatResult (const tunerite::BeatAnalysisResult& result, std::uint64_t generation)
{
    if (generation != analysisGeneration.load (std::memory_order_acquire))
        return;
    detectedBpm.store (result.bpm, std::memory_order_relaxed);
    detectedAlternativeBpm.store (result.alternativeBpm, std::memory_order_relaxed);
    detectedKey.store (result.keyRoot, std::memory_order_relaxed);
    detectedMode.store (result.keyMode, std::memory_order_relaxed);
    bpmConfidence.store (static_cast<float> (result.bpmConfidence), std::memory_order_relaxed);
    keyConfidence.store (static_cast<float> (result.keyConfidence), std::memory_order_relaxed);
    detectedTempoValid.store (result.tempoValid, std::memory_order_relaxed);
    detectedKeyValid.store (result.keyValid, std::memory_order_relaxed);
    hasStableDetectionFlag.store (result.usableAudio && result.tempoValid && result.keyValid, std::memory_order_release);

    // Beat Only is an answer workflow: retain each independently valid audio-derived result.
    if (result.usableAudio && result.tempoValid)
    {
        savedBeatBpm.store (result.bpm, std::memory_order_relaxed);
        savedBeatAlternativeBpm.store (result.alternativeBpm, std::memory_order_relaxed);
        savedBeatBpmConfidence.store (static_cast<float> (result.bpmConfidence), std::memory_order_relaxed);
        savedBeatTempoValid.store (true, std::memory_order_release);
    }
    if (result.usableAudio && result.keyValid)
    {
        savedBeatKey.store (result.keyRoot, std::memory_order_relaxed);
        savedBeatMode.store (result.keyMode, std::memory_order_relaxed);
        savedBeatKeyConfidence.store (static_cast<float> (result.keyConfidence), std::memory_order_relaxed);
        savedBeatKeyValid.store (true, std::memory_order_release);
    }
    const auto savedAnyBeatAnswer = savedBeatTempoValid.load (std::memory_order_acquire)
                                 || savedBeatKeyValid.load (std::memory_order_acquire);
    savedBeatResult.store (savedAnyBeatAnswer, std::memory_order_release);
    beatOutcomeState.store (savedAnyBeatAnswer ? 1 : 2, std::memory_order_release);
}

void KeyBridgeAudioProcessor::publishVocalResult (const tunerite::VocalAnalysisResult& result, std::uint64_t generation)
{
    if (generation != analysisGeneration.load (std::memory_order_acquire))
        return;
    vocalConfidence.store (static_cast<float> (result.confidence), std::memory_order_relaxed);
    vocalLowestMidi.store (static_cast<float> (result.lowMidi), std::memory_order_relaxed);
    vocalHighestMidi.store (static_cast<float> (result.highMidi), std::memory_order_relaxed);
    vocalAverageMidi.store (static_cast<float> (result.averageMidi), std::memory_order_relaxed);
    vocalSustainedPercent.store (static_cast<float> (result.sustainedPercent), std::memory_order_relaxed);
    vocalNoteChangeSpeed.store (static_cast<float> (result.noteChangeRate), std::memory_order_relaxed);
    vocalVoicedPercent.store (static_cast<float> (result.voicedPercent), std::memory_order_relaxed);
    vocalVibrato.store (static_cast<float> (result.vibratoDepthCents), std::memory_order_relaxed);
    vocalFrames.store (static_cast<int> (result.midiContour.size()), std::memory_order_relaxed);
    vocalMelodic.store (result.melodic, std::memory_order_relaxed);
}

void KeyBridgeAudioProcessor::resetLiveResults() noexcept
{
    detectedBpm.store (0.0, std::memory_order_relaxed);
    detectedAlternativeBpm.store (0.0, std::memory_order_relaxed);
    detectedKey.store (-1, std::memory_order_relaxed);
    detectedMode.store (-1, std::memory_order_relaxed);
    hasStableDetectionFlag.store (false, std::memory_order_relaxed);
    detectedTempoValid.store (false, std::memory_order_relaxed);
    detectedKeyValid.store (false, std::memory_order_relaxed);
    keyConfidence.store (0.0f, std::memory_order_relaxed);
    bpmConfidence.store (0.0f, std::memory_order_relaxed);
    inputPeak.store (0.0f, std::memory_order_relaxed);
    audioCallbackCount.store (0, std::memory_order_relaxed);
    lastAudioBlockSize.store (0, std::memory_order_relaxed);
    leftInputPeak.store (0.0f, std::memory_order_relaxed);
    rightInputPeak.store (0.0f, std::memory_order_relaxed);
    beatRms.store (0.0f, std::memory_order_relaxed);
    vocalRms.store (0.0f, std::memory_order_relaxed);
    vocalInputPeak.store (0.0f, std::memory_order_relaxed);
    analysisDuration.store (0.0f, std::memory_order_relaxed);
    analysisFrames.store (0, std::memory_order_relaxed);
    vocalFrames.store (0, std::memory_order_relaxed);
    vocalConfidence.store (0.0f, std::memory_order_relaxed);
    vocalLowestMidi.store (0.0f, std::memory_order_relaxed);
    vocalHighestMidi.store (0.0f, std::memory_order_relaxed);
    vocalAverageMidi.store (0.0f, std::memory_order_relaxed);
    vocalPitchAccuracy.store (0.0f, std::memory_order_relaxed);
    vocalVibrato.store (0.0f, std::memory_order_relaxed);
    vocalSustainedPercent.store (0.0f, std::memory_order_relaxed);
    vocalNoteChangeSpeed.store (0.0f, std::memory_order_relaxed);
    vocalVoicedPercent.store (0.0f, std::memory_order_relaxed);
    vocalMelodic.store (false, std::memory_order_relaxed);
}

void KeyBridgeAudioProcessor::startFreshAnalysis() noexcept
{
    if (analysisMode.load (std::memory_order_relaxed) != 2)
    {
        analysisEnabled.store (true, std::memory_order_relaxed);
        analysisGeneration.fetch_add (1, std::memory_order_acq_rel);
        finishCaptureRequested.store (false, std::memory_order_release);
        if (analysisMode.load (std::memory_order_relaxed) == 0)
        {
            savedBeatResult.store (false, std::memory_order_release);
            savedBeatTempoValid.store (false, std::memory_order_release);
            savedBeatKeyValid.store (false, std::memory_order_release);
            savedBeatBpm.store (0.0, std::memory_order_relaxed);
            savedBeatKey.store (-1, std::memory_order_relaxed);
            savedBeatMode.store (-1, std::memory_order_relaxed);
        }
        beatOutcomeState.store (0, std::memory_order_release);
        captureState.store (armed, std::memory_order_release);
        captureRequested.store (true, std::memory_order_release);
    }
}

void KeyBridgeAudioProcessor::finishCapture() noexcept
{
    if (captureActive.load (std::memory_order_acquire))
        finishCaptureRequested.store (true, std::memory_order_release);
}

void KeyBridgeAudioProcessor::stopAnalysis() noexcept
{
    analysisEnabled.store (false, std::memory_order_release);
    analysisGeneration.fetch_add (1, std::memory_order_acq_rel);
    captureActive.store (false, std::memory_order_release);
    captureRequested.store (false, std::memory_order_release);
    finishCaptureRequested.store (false, std::memory_order_release);
    captureState.store (cancelled, std::memory_order_release);
    captureProgress.store (0.0f, std::memory_order_relaxed);
}

void KeyBridgeAudioProcessor::saveBeatResult() noexcept
{
    if (hasStableDetectionFlag.load (std::memory_order_acquire))
    {
        savedBeatKey.store (detectedKey.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedBeatMode.store (detectedMode.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedBeatBpm.store (detectedBpm.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedBeatAlternativeBpm.store (detectedAlternativeBpm.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedBeatKeyConfidence.store (keyConfidence.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedBeatBpmConfidence.store (bpmConfidence.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedBeatTempoValid.store (true, std::memory_order_release);
        savedBeatKeyValid.store (true, std::memory_order_release);
        savedBeatResult.store (true, std::memory_order_release);
    }
}

void KeyBridgeAudioProcessor::saveVocalResult() noexcept
{
    if (vocalConfidence.load (std::memory_order_relaxed) >= 0.55f
        && vocalVoicedPercent.load (std::memory_order_relaxed) >= 0.20f)
    {
        savedVocalLowestMidi.store (vocalLowestMidi.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedVocalHighestMidi.store (vocalHighestMidi.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedVocalAverageMidi.store (vocalAverageMidi.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedVocalVoicedPercent.store (vocalVoicedPercent.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedVocalConfidence.store (vocalConfidence.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedVocalSustainedPercent.store (vocalSustainedPercent.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedVocalNoteChangeSpeed.store (vocalNoteChangeSpeed.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedVocalMelodic.store (vocalMelodic.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedVocalResult.store (true, std::memory_order_release);
    }
}

void KeyBridgeAudioProcessor::clearBeatResult() noexcept
{
    savedBeatResult.store (false, std::memory_order_release);
    savedBeatTempoValid.store (false, std::memory_order_release);
    savedBeatKeyValid.store (false, std::memory_order_release);
    savedBeatBpm.store (0.0, std::memory_order_relaxed);
    beatOutcomeState.store (0, std::memory_order_release);
    savedBeatKey.store (-1, std::memory_order_relaxed);
    savedBeatMode.store (-1, std::memory_order_relaxed);
}

void KeyBridgeAudioProcessor::clearVocalResult() noexcept
{
    savedVocalResult.store (false, std::memory_order_release);
}

void KeyBridgeAudioProcessor::resetAllResults() noexcept
{
    savedBeatResult.store (false, std::memory_order_release);
    savedBeatTempoValid.store (false, std::memory_order_release);
    savedBeatKeyValid.store (false, std::memory_order_release);
    beatOutcomeState.store (0, std::memory_order_release);
    savedVocalResult.store (false, std::memory_order_release);
    analysisGeneration.fetch_add (1, std::memory_order_acq_rel);
    captureActive.store (false, std::memory_order_release);
    captureRequested.store (false, std::memory_order_release);
    finishCaptureRequested.store (false, std::memory_order_release);
    captureState.store (idle, std::memory_order_release);
    capturedSignalSamples.store (0, std::memory_order_relaxed);
    capturedSignalSeconds.store (0.0f, std::memory_order_relaxed);
    resetLiveResults();
}

void KeyBridgeAudioProcessor::setAppearance (std::uint32_t accent, std::uint32_t panel, std::uint32_t background, float opacity, float glow, bool compact) noexcept
{
    appearanceAccent.store (accent, std::memory_order_relaxed);
    appearancePanel.store (panel, std::memory_order_relaxed);
    appearanceBackground.store (background, std::memory_order_relaxed);
    appearancePanelOpacity.store (juce::jlimit (0.72f, 1.0f, opacity), std::memory_order_relaxed);
    appearanceGlow.store (juce::jlimit (0.0f, 1.0f, glow), std::memory_order_relaxed);
    appearanceCompact.store (compact, std::memory_order_relaxed);
}

void KeyBridgeAudioProcessor::resetAppearance() noexcept
{
    setAppearance (0xff55c7e8, 0xff17202c, 0xff0b1017, 0.94f, 0.35f, false);
}

void KeyBridgeAudioProcessor::getStateInformation (juce::MemoryBlock& destinationData)
{
    juce::ValueTree state ("TuneRiteState");
    state.setProperty ("accent", static_cast<int> (appearanceAccent.load (std::memory_order_relaxed)), nullptr);
    state.setProperty ("panel", static_cast<int> (appearancePanel.load (std::memory_order_relaxed)), nullptr);
    state.setProperty ("background", static_cast<int> (appearanceBackground.load (std::memory_order_relaxed)), nullptr);
    state.setProperty ("panelOpacity", appearancePanelOpacity.load (std::memory_order_relaxed), nullptr);
    state.setProperty ("glow", appearanceGlow.load (std::memory_order_relaxed), nullptr);
    state.setProperty ("compact", appearanceCompact.load (std::memory_order_relaxed), nullptr);
    copyXmlToBinary (*state.createXml(), destinationData);
}

void KeyBridgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        const auto state = juce::ValueTree::fromXml (*xml);
        if (state.hasType ("TuneRiteState"))
        {
            setAppearance (static_cast<std::uint32_t> (static_cast<int> (state.getProperty ("accent", static_cast<int> (0xff55c7e8)))),
                           static_cast<std::uint32_t> (static_cast<int> (state.getProperty ("panel", static_cast<int> (0xff17202c)))),
                           static_cast<std::uint32_t> (static_cast<int> (state.getProperty ("background", static_cast<int> (0xff0b1017)))),
                           static_cast<float> (state.getProperty ("panelOpacity", 0.94f)),
                           static_cast<float> (state.getProperty ("glow", 0.35f)),
                           static_cast<bool> (state.getProperty ("compact", false)));
        }
    }
}

juce::AudioProcessorEditor* KeyBridgeAudioProcessor::createEditor()
{
    return new KeyBridgeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KeyBridgeAudioProcessor();
}
