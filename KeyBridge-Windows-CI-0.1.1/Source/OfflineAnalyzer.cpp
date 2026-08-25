#include <JuceHeader.h>
#include "AnalysisCore.h"

#include <fstream>
#include <iomanip>
#include <iostream>

namespace
{
    void printUsage()
    {
        std::cout << "Usage: TuneriteOfflineAnalyzer --input <audio.wav> --mode <beat|vocal> [--output <report-prefix>]\n";
    }

    juce::String jsonEscape (const std::string& text)
    {
        return juce::String (text).replace ("\\", "\\\\").replace ("\"", "\\\"").replace ("\n", "\\n");
    }

    void writeBeatReport (const juce::File& output, const tunerite::BeatAnalysisResult& result, double sampleRate, int channels)
    {
        juce::String json;
        json << "{\n";
        json << "  \"mode\": \"beat\",\n";
        json << "  \"sample_rate\": " << sampleRate << ",\n";
        json << "  \"channels\": " << channels << ",\n";
        json << "  \"duration_seconds\": " << result.durationSeconds << ",\n";
        json << "  \"rms\": " << result.rms << ",\n";
        json << "  \"peak\": " << result.peak << ",\n";
        json << "  \"detected_bpm\": " << result.bpm << ",\n";
        json << "  \"alternative_bpm\": " << result.alternativeBpm << ",\n";
        json << "  \"half_time_bpm\": " << result.halfTimeBpm << ",\n";
        json << "  \"double_time_bpm\": " << result.doubleTimeBpm << ",\n";
        json << "  \"bpm_confidence\": " << result.bpmConfidence << ",\n";
        json << "  \"key_root\": " << result.keyRoot << ",\n";
        json << "  \"key_mode\": " << result.keyMode << ",\n";
        json << "  \"key_confidence\": " << result.keyConfidence << ",\n";
        json << "  \"key_uncertain\": " << (result.keyUncertain ? "true" : "false") << ",\n";
        json << "  \"bpm_uncertain\": " << (result.bpmUncertain ? "true" : "false") << ",\n";
        json << "  \"warning\": \"" << jsonEscape (result.warning) << "\",\n";
        json << "  \"tempo_candidates\": [";
        for (size_t i = 0; i < result.tempoCandidates.size(); ++i)
        {
            const auto& candidate = result.tempoCandidates[i];
            json << "{\"bpm\":" << candidate.bpm << ",\"score\":" << candidate.score << "}";
            if (i + 1 < result.tempoCandidates.size()) json << ",";
        }
        json << "],\n  \"key_candidates\": [";
        for (size_t i = 0; i < result.keyCandidates.size(); ++i)
        {
            json << "\"" << jsonEscape (result.keyCandidates[i]) << "\"";
            if (i + 1 < result.keyCandidates.size()) json << ",";
        }
        json << "]\n}\n";
        output.replaceWithText (json);

        juce::String text;
        text << "TuneRite Offline Beat Analysis\n";
        text << "Sample rate: " << sampleRate << " Hz\nChannels: " << channels << "\n";
        text << "Duration: " << result.durationSeconds << " seconds\n";
        text << "BPM: " << result.bpm << "  Alternative: " << result.alternativeBpm << "  Half: " << result.halfTimeBpm << "  Double: " << result.doubleTimeBpm << "\n";
        text << "BPM confidence: " << result.bpmConfidence << "\n";
        text << "Key root: " << result.keyRoot << "  Mode: " << (result.keyMode == 0 ? "major" : "minor") << "  Key confidence: " << result.keyConfidence << "\n";
        text << "Warning: " << result.warning << "\n";
        output.getSiblingFile (output.getFileNameWithoutExtension() + ".txt").replaceWithText (text);
    }

    void writeVocalReport (const juce::File& output, const tunerite::VocalAnalysisResult& result, double sampleRate, int channels)
    {
        juce::String json;
        json << "{\n";
        json << "  \"mode\": \"vocal\",\n";
        json << "  \"sample_rate\": " << sampleRate << ",\n";
        json << "  \"channels\": " << channels << ",\n";
        json << "  \"rms\": " << result.rms << ",\n";
        json << "  \"peak\": " << result.peak << ",\n";
        json << "  \"voiced_percent\": " << result.voicedPercent << ",\n";
        json << "  \"confidence\": " << result.confidence << ",\n";
        json << "  \"low_midi\": " << result.lowMidi << ",\n";
        json << "  \"high_midi\": " << result.highMidi << ",\n";
        json << "  \"p05_midi\": " << result.p05Midi << ",\n";
        json << "  \"p95_midi\": " << result.p95Midi << ",\n";
        json << "  \"average_midi\": " << result.averageMidi << ",\n";
        json << "  \"pitch_stability_cents\": " << result.pitchStabilityCents << ",\n";
        json << "  \"sustained_percent\": " << result.sustainedPercent << ",\n";
        json << "  \"note_change_rate\": " << result.noteChangeRate << ",\n";
        json << "  \"melodic\": " << (result.melodic ? "true" : "false") << ",\n";
        json << "  \"warning\": \"" << jsonEscape (result.warning) << "\"\n";
        json << "}\n";
        output.replaceWithText (json);

        juce::String text;
        text << "TuneRite Offline Vocal Analysis\n";
        text << "Sample rate: " << sampleRate << " Hz\nChannels: " << channels << "\n";
        text << "Range: " << result.lowMidi << " to " << result.highMidi << " MIDI\n";
        text << "Voiced: " << result.voicedPercent << "  Confidence: " << result.confidence << "\n";
        text << "Warning: " << result.warning << "\n";
        output.getSiblingFile (output.getFileNameWithoutExtension() + ".txt").replaceWithText (text);
    }
}

int main (int argc, char* argv[])
{
    juce::String inputPath;
    juce::String mode = "beat";
    juce::String outputPrefix;
    for (int i = 1; i < argc; ++i)
    {
        const juce::String argument (argv[i]);
        if (argument == "--input" && i + 1 < argc) inputPath = argv[++i];
        else if (argument == "--mode" && i + 1 < argc) mode = argv[++i].toLowerCase();
        else if (argument == "--output" && i + 1 < argc) outputPrefix = argv[++i];
    }
    if (inputPath.isEmpty() || (mode != "beat" && mode != "vocal"))
    {
        printUsage();
        return 2;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    const juce::File input (inputPath);
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (input));
    if (reader == nullptr)
    {
        std::cerr << "Unable to decode input file: " << inputPath << "\n";
        return 3;
    }

    const auto sampleCount = static_cast<int> (reader->lengthInSamples);
    juce::AudioBuffer<float> decoded (static_cast<int> (reader->numChannels), sampleCount);
    if (! reader->read (&decoded, 0, sampleCount, 0, true, true))
    {
        std::cerr << "Unable to read input samples.\n";
        return 4;
    }

    std::vector<float> mono (static_cast<size_t> (sampleCount), 0.0f);
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        float value = 0.0f;
        for (int channel = 0; channel < decoded.getNumChannels(); ++channel)
            value += decoded.getSample (channel, sample);
        mono[static_cast<size_t> (sample)] = value / static_cast<float> (juce::jmax (1, decoded.getNumChannels()));
    }

    const juce::File output = outputPrefix.isEmpty()
        ? input.getSiblingFile (input.getFileNameWithoutExtension() + "_tunerite.json")
        : juce::File (outputPrefix).withFileExtension ("json");

    if (mode == "beat")
        writeBeatReport (output, tunerite::AnalysisCore::analyzeBeat (mono, reader->sampleRate), reader->sampleRate, decoded.getNumChannels());
    else
        writeVocalReport (output, tunerite::AnalysisCore::analyzeVocal (mono, reader->sampleRate), reader->sampleRate, decoded.getNumChannels());

    std::cout << "Wrote report: " << output.getFullPathName() << "\n";
    return 0;
}
