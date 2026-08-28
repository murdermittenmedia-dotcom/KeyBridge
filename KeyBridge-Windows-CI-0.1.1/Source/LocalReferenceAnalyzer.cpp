#include "LocalReferenceAnalyzer.h"

#include <cmath>
#include <memory>

namespace
{
    constexpr double minBpm = 40.0;
    constexpr double maxBpm = 300.0;
    constexpr double minimumConfidence = 0.15;
    constexpr int analyzerTimeoutMs = 60000;

    struct PrivateArtifacts
    {
        juce::File wav;
        juce::File json;

        ~PrivateArtifacts()
        {
            wav.deleteFile();
            json.deleteFile();
        }
    };

    bool isFinite (double value)
    {
        return std::isfinite (value);
    }

    bool getObjectBool (const juce::DynamicObject* object, const juce::Identifier& name, bool& value)
    {
        if (object == nullptr || ! object->hasProperty (name)) return false;
        const auto raw = object->getProperty (name);
        if (! raw.isBool()) return false;
        value = static_cast<bool> (raw);
        return true;
    }

    bool getObjectFiniteNumber (const juce::DynamicObject* object, const juce::Identifier& name, double& value)
    {
        if (object == nullptr || ! object->hasProperty (name)) return false;
        const auto raw = object->getProperty (name);
        if (! raw.isDouble() && ! raw.isInt() && ! raw.isInt64()) return false;
        value = static_cast<double> (raw);
        return isFinite (value);
    }

    bool getObjectInt (const juce::DynamicObject* object, const juce::Identifier& name, int& value)
    {
        double number = 0.0;
        if (! getObjectFiniteNumber (object, name, number) || std::abs (std::floor (number) - number) > 1.0e-9) return false;
        value = static_cast<int> (number);
        return true;
    }

    void addTempoCandidates (const juce::var& source, tunerite::BeatAnalysisResult& result)
    {
        const auto* array = source.getArray();
        if (array == nullptr) return;
        for (const auto& item : *array)
        {
            const auto* candidate = item.getDynamicObject();
            double bpm = 0.0, score = 0.0;
            if (! getObjectFiniteNumber (candidate, "bpm", bpm)
                || ! getObjectFiniteNumber (candidate, "score", score)
                || bpm < minBpm || bpm > maxBpm || score < 0.0 || score > 1.0)
                continue;
            result.tempoCandidates.push_back ({ bpm, score });
            if (result.tempoCandidates.size() == 3) return;
        }
    }

    void addKeyCandidates (const juce::var& source, tunerite::BeatAnalysisResult& result)
    {
        const auto* array = source.getArray();
        if (array == nullptr) return;
        for (const auto& item : *array)
        {
            const auto* candidate = item.getDynamicObject();
            int root = -1;
            double score = 0.0;
            if (! getObjectInt (candidate, "root", root)
                || ! getObjectFiniteNumber (candidate, "score", score)
                || root < 0 || root > 11 || score < 0.0 || score > 1.0)
                continue;
            const auto modeText = candidate->getProperty ("mode").toString().toLowerCase();
            if (modeText != "major" && modeText != "minor") continue;
            constexpr const char* noteNames[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
            const auto mode = modeText == "major" ? 0 : 1;
            result.keyCandidateScores.push_back ({ root, mode, score });
            result.keyCandidates.push_back (std::string (noteNames[root]) + " " + modeText.toStdString());
            if (result.keyCandidateScores.size() == 3) return;
        }
    }

    bool writePrivateWav (const juce::File& destination, const std::vector<float>& monoSamples, double sampleRate)
    {
        if (monoSamples.empty() || ! isFinite (sampleRate) || sampleRate < 8000.0 || sampleRate > 192000.0)
            return false;
        juce::WavAudioFormat format;
        auto stream = destination.createOutputStream();
        if (stream == nullptr) return false;
        std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (stream.release(), sampleRate, 1, 24, {}, 0));
        if (writer == nullptr) return false;
        juce::AudioBuffer<float> buffer (1, static_cast<int> (monoSamples.size()));
        buffer.copyFrom (0, 0, monoSamples.data(), static_cast<int> (monoSamples.size()));
        return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }
}

namespace tunerite
{
    juce::File LocalReferenceAnalyzer::resolveAnalyzerExecutable()
    {
        const auto overridePath = juce::SystemStats::getEnvironmentVariable ("TUNERITE_REFERENCE_ANALYZER", {});
        if (overridePath.isNotEmpty()) return juce::File (overridePath);

        // This path is intentionally not bundled or packaged until runtime and license
        // approval. It is the future location of a reviewed local analyzer executable.
        return juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
            .getChildFile ("Murder Mitten Media")
            .getChildFile ("TuneRite")
            .getChildFile ("TuneRiteReferenceAnalyzer.exe");
    }

    BeatAnalysisResult LocalReferenceAnalyzer::parseJsonResult (const juce::String& jsonText)
    {
        BeatAnalysisResult result;
        const auto parsed = juce::JSON::parse (jsonText);
        const auto* root = parsed.getDynamicObject();
        int schemaVersion = 0;
        if (! getObjectInt (root, "schema_version", schemaVersion) || schemaVersion != 1)
        {
            result.warning = "Local analyzer returned an invalid JSON schema.";
            return result;
        }

        const auto tempoValue = root->getProperty ("tempo");
        const auto keyValue = root->getProperty ("key");
        const auto* tempo = tempoValue.getDynamicObject();
        const auto* key = keyValue.getDynamicObject();
        bool tempoValid = false, keyValid = false;
        double tempoConfidence = 0.0, keyConfidence = 0.0;
        if (! getObjectBool (tempo, "valid", tempoValid)
            || ! getObjectBool (key, "valid", keyValid)
            || ! getObjectFiniteNumber (tempo, "confidence", tempoConfidence)
            || ! getObjectFiniteNumber (key, "confidence", keyConfidence)
            || tempoConfidence < 0.0 || tempoConfidence > 1.0
            || keyConfidence < 0.0 || keyConfidence > 1.0)
        {
            result.warning = "Local analyzer returned invalid BPM/key confidence fields.";
            return result;
        }

        result.usableAudio = true;
        result.bpmConfidence = tempoConfidence;
        result.keyConfidence = keyConfidence;
        result.bpmUncertain = true;
        result.keyUncertain = true;
        addTempoCandidates (tempo->getProperty ("candidates"), result);
        addKeyCandidates (key->getProperty ("candidates"), result);

        if (tempoValid)
        {
            double bpm = 0.0;
            if (! getObjectFiniteNumber (tempo, "bpm", bpm) || bpm < minBpm || bpm > maxBpm || tempoConfidence < minimumConfidence)
            {
                result.warning = "Local analyzer returned an invalid BPM result.";
                return result;
            }
            result.bpm = bpm;
            result.tempoValid = true;
            result.bpmUncertain = false;
            if (result.tempoCandidates.empty()) result.tempoCandidates.push_back ({ bpm, tempoConfidence });
            if (result.tempoCandidates.size() > 1) result.alternativeBpm = result.tempoCandidates[1].bpm;
            result.halfTimeBpm = bpm * 0.5;
            result.doubleTimeBpm = bpm * 2.0;
        }

        if (keyValid)
        {
            int rootIndex = -1;
            const auto modeText = key->getProperty ("mode").toString().toLowerCase();
            if (! getObjectInt (key, "root", rootIndex) || rootIndex < 0 || rootIndex > 11
                || (modeText != "major" && modeText != "minor") || keyConfidence < minimumConfidence)
            {
                result.warning = "Local analyzer returned an invalid key result.";
                result.keyValid = false;
                result.keyUncertain = true;
                return result;
            }
            result.keyRoot = rootIndex;
            result.keyMode = modeText == "major" ? 0 : 1;
            result.keyValid = true;
            result.keyUncertain = false;
            result.harmonicContentSufficient = true;
            if (result.keyCandidateScores.empty())
            {
                constexpr const char* noteNames[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
                result.keyCandidateScores.push_back ({ result.keyRoot, result.keyMode, keyConfidence });
                result.keyCandidates.push_back (std::string (noteNames[result.keyRoot]) + " " + modeText.toStdString());
            }
        }

        if (! result.tempoValid && ! result.keyValid)
            result.warning = "Local analyzer found insufficient BPM and harmonic evidence.";
        else if (! result.tempoValid)
            result.warning = "Local analyzer found no reliable BPM candidate.";
        else if (! result.keyValid)
            result.warning = "Local analyzer found insufficient harmonic evidence for key detection.";
        return result;
    }

    BeatAnalysisResult LocalReferenceAnalyzer::analyzeFinalizedCapture (const std::vector<float>& monoSamples,
                                                                         double sampleRate,
                                                                         std::uint64_t generation)
    {
        const auto executable = resolveAnalyzerExecutable();
        if (! executable.existsAsFile())
        {
            BeatAnalysisResult result;
            result.warning = "Local analyzer is unavailable; no BPM/key result was saved.";
            return result;
        }

        auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("TuneRitePrivateAnalysis");
        if (! directory.createDirectory())
        {
            BeatAnalysisResult result;
            result.warning = "Could not create private analysis workspace.";
            return result;
        }
        const auto token = juce::String::toHexString (juce::Time::getMillisecondCounterHiRes())
            + "_" + juce::String (static_cast<juce::int64> (generation));
        PrivateArtifacts artifacts {
            directory.getNonexistentChildFile ("capture_" + token, ".wav", false),
            directory.getNonexistentChildFile ("result_" + token, ".json", false)
        };
        if (! writePrivateWav (artifacts.wav, monoSamples, sampleRate))
        {
            BeatAnalysisResult result;
            result.warning = "Could not write private capture for local analysis.";
            return result;
        }

        juce::ChildProcess process;
        juce::StringArray arguments { executable.getFullPathName(), "--input", artifacts.wav.getFullPathName(), "--output", artifacts.json.getFullPathName() };
        if (! process.start (arguments, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        {
            BeatAnalysisResult result;
            result.warning = "Local analyzer process could not start.";
            return result;
        }
        if (! process.waitForProcessToFinish (analyzerTimeoutMs))
        {
            process.kill();
            BeatAnalysisResult result;
            result.warning = "Local analyzer timed out; no BPM/key result was saved.";
            return result;
        }
        const auto output = process.readAllProcessOutput().trim();
        if (process.getExitCode() != 0)
        {
            BeatAnalysisResult result;
            result.warning = output.isNotEmpty()
                ? (juce::String ("Local analyzer failed: ") + output.substring (0, 240)).toStdString()
                : "Local analyzer failed.";
            return result;
        }
        if (! artifacts.json.existsAsFile())
        {
            BeatAnalysisResult result;
            result.warning = "Local analyzer returned no JSON result.";
            return result;
        }
        return parseJsonResult (artifacts.json.loadFileAsString());
    }
}
