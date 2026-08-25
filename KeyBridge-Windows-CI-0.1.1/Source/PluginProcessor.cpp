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
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    const auto capacity = static_cast<size_t> (std::ceil (sampleRate * captureSeconds));
    captureBuffers[0].assign (capacity, 0.0f);
    captureBuffers[1].assign (capacity, 0.0f);
    capturedSamples.store (0, std::memory_order_relaxed);
    completedSamples.store (0, std::memory_order_relaxed);
    completedBuffer.store (-1, std::memory_order_relaxed);
    captureRequested.store (false, std::memory_order_relaxed);
    captureActive.store (false, std::memory_order_relaxed);
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
    double energy = 0.0;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto mono = 0.5f * (left[i] + right[i]);
        blockPeak = juce::jmax (blockPeak, std::abs (mono));
        energy += static_cast<double> (mono) * static_cast<double> (mono);
    }
    const auto rms = static_cast<float> (std::sqrt (energy / static_cast<double> (buffer.getNumSamples())));
    inputPeak.store (0.85f * inputPeak.load (std::memory_order_relaxed) + 0.15f * blockPeak, std::memory_order_relaxed);

    if (captureRequested.exchange (false, std::memory_order_acq_rel)
        && ! captureActive.load (std::memory_order_relaxed)
        && ! workerBusy.load (std::memory_order_relaxed)
        && ! workerJobReady.load (std::memory_order_relaxed))
    {
        const auto nextBuffer = 1 - juce::jmax (0, completedBuffer.load (std::memory_order_relaxed));
        activeBuffer.store (nextBuffer, std::memory_order_relaxed);
        capturedSamples.store (0, std::memory_order_relaxed);
        captureProgress.store (0.0f, std::memory_order_relaxed);
        resetLiveResults();
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
        destination[static_cast<size_t> (write + i)] = 0.5f * (left[i] + right[i]);
    write += toCopy;
    capturedSamples.store (write, std::memory_order_relaxed);
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

    if (write >= capacity)
    {
        captureActive.store (false, std::memory_order_release);
        completedBuffer.store (bufferIndex, std::memory_order_release);
        completedSamples.store (write, std::memory_order_release);
        completedMode.store (mode, std::memory_order_release);
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
        workerJobReady.store (false, std::memory_order_release);
        workerBusy.store (true, std::memory_order_release);
        lock.unlock();

        if (bufferIndex >= 0 && bufferIndex < 2 && sampleCount > 0)
        {
            const auto& buffer = captureBuffers[bufferIndex];
            std::vector<float> view (buffer.begin(), buffer.begin() + juce::jmin (sampleCount, static_cast<int> (buffer.size())));
            if (mode == 0)
                publishBeatResult (tunerite::AnalysisCore::analyzeBeat (view, sampleRate));
            else if (mode == 1)
                publishVocalResult (tunerite::AnalysisCore::analyzeVocal (view, sampleRate));
        }
        workerBusy.store (false, std::memory_order_release);
    }
}

void KeyBridgeAudioProcessor::publishBeatResult (const tunerite::BeatAnalysisResult& result)
{
    detectedBpm.store (result.bpm, std::memory_order_relaxed);
    detectedKey.store (result.keyRoot, std::memory_order_relaxed);
    detectedMode.store (result.keyMode, std::memory_order_relaxed);
    bpmConfidence.store (static_cast<float> (result.bpmConfidence), std::memory_order_relaxed);
    keyConfidence.store (static_cast<float> (result.keyConfidence), std::memory_order_relaxed);
    hasStableDetectionFlag.store (result.usableAudio && ! result.keyUncertain && ! result.bpmUncertain, std::memory_order_release);
}

void KeyBridgeAudioProcessor::publishVocalResult (const tunerite::VocalAnalysisResult& result)
{
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
    detectedKey.store (0, std::memory_order_relaxed);
    detectedMode.store (0, std::memory_order_relaxed);
    hasStableDetectionFlag.store (false, std::memory_order_relaxed);
    keyConfidence.store (0.0f, std::memory_order_relaxed);
    bpmConfidence.store (0.0f, std::memory_order_relaxed);
    inputPeak.store (0.0f, std::memory_order_relaxed);
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
        captureRequested.store (true, std::memory_order_release);
    }
}

void KeyBridgeAudioProcessor::saveBeatResult() noexcept
{
    if (hasStableDetectionFlag.load (std::memory_order_acquire))
    {
        savedBeatKey.store (detectedKey.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedBeatMode.store (detectedMode.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedBeatBpm.store (detectedBpm.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedBeatKeyConfidence.store (keyConfidence.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedBeatBpmConfidence.store (bpmConfidence.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedBeatResult.store (true, std::memory_order_release);
    }
}

void KeyBridgeAudioProcessor::saveVocalResult() noexcept
{
    if (vocalConfidence.load (std::memory_order_relaxed) >= 0.55f)
    {
        savedVocalLowestMidi.store (vocalLowestMidi.load (std::memory_order_relaxed), std::memory_order_relaxed);
        savedVocalHighestMidi.store (vocalHighestMidi.load (std::memory_order_relaxed), std::memory_order_relaxed);
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
}

void KeyBridgeAudioProcessor::clearVocalResult() noexcept
{
    savedVocalResult.store (false, std::memory_order_release);
}

void KeyBridgeAudioProcessor::resetAllResults() noexcept
{
    savedBeatResult.store (false, std::memory_order_release);
    savedVocalResult.store (false, std::memory_order_release);
    captureActive.store (false, std::memory_order_release);
    captureRequested.store (false, std::memory_order_release);
    resetLiveResults();
}

juce::AudioProcessorEditor* KeyBridgeAudioProcessor::createEditor()
{
    return new KeyBridgeAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KeyBridgeAudioProcessor();
}
