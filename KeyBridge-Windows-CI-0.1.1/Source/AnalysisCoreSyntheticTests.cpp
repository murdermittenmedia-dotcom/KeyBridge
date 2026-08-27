#include "AnalysisCore.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
    constexpr double pi = 3.14159265358979323846;
    constexpr const char* names[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

    struct TestRecord
    {
        std::string name;
        bool passed = false;
        double expectedBpm = 0.0;
        double detectedBpm = 0.0;
        int expectedRoot = -1;
        int expectedMode = -1;
        int detectedRoot = -1;
        int detectedMode = -1;
        double bpmConfidence = 0.0;
        double keyConfidence = 0.0;
        std::string warning;
    };

    int mod12 (int value)
    {
        const auto result = value % 12;
        return result < 0 ? result + 12 : result;
    }

    void addTone (std::vector<float>& output, double sampleRate, int begin, int end, double hz, double amplitude)
    {
        for (int sample = begin; sample < end; ++sample)
        {
            const auto t = static_cast<double> (sample - begin) / sampleRate;
            const auto envelope = std::min (1.0, t * 16.0) * std::min (1.0, (static_cast<double> (end - sample) / sampleRate) * 16.0);
            const auto fundamental = std::sin (2.0 * pi * hz * t);
            const auto harmonic2 = 0.28 * std::sin (2.0 * pi * hz * 2.0 * t);
            const auto harmonic3 = 0.11 * std::sin (2.0 * pi * hz * 3.0 * t);
            output[static_cast<size_t> (sample)] += static_cast<float> (amplitude * envelope * (fundamental + harmonic2 + harmonic3));
        }
    }

    double frequencyForMidi (int midi)
    {
        return 440.0 * std::pow (2.0, (midi - 69) / 12.0);
    }

    std::vector<float> makeClickBeat (double bpm, double seconds, double sampleRate)
    {
        const auto count = static_cast<size_t> (std::round (seconds * sampleRate));
        std::vector<float> output (count, 0.0f);
        const auto interval = std::max (1, static_cast<int> (std::round ((60.0 / bpm) * sampleRate)));
        for (int start = 0; start < static_cast<int> (count); start += interval)
            for (int n = 0; n < 1200 && start + n < static_cast<int> (count); ++n)
                output[static_cast<size_t> (start + n)] += static_cast<float> (0.72 * std::exp (-static_cast<double> (n) / 155.0));
        return output;
    }

    std::vector<float> makeProgression (int root, int mode, double sampleRate, double detuneCents = 0.0)
    {
        constexpr double seconds = 16.0;
        const auto detuneRatio = std::pow (2.0, detuneCents / 1200.0);
        const auto count = static_cast<size_t> (seconds * sampleRate);
        std::vector<float> output (count, 0.0f);
        // Five sustained functions with characteristic scale degrees and a deliberately longer final tonic.
        const std::array<int, 5> majorRoots { { 0, 5, 7, 0, 0 } };
        const std::array<bool, 5> majorMinor { { false, false, false, false, false } };
        const std::array<int, 5> minorRoots { { 0, 8, 10, 7, 0 } };
        const std::array<bool, 5> minorMinor { { true, false, false, false, true } };
        const auto& chordRoots = mode == 0 ? majorRoots : minorRoots;
        const auto& chordMinor = mode == 0 ? majorMinor : minorMinor;
        const auto chordSeconds = seconds / chordRoots.size();
        for (size_t chord = 0; chord < chordRoots.size(); ++chord)
        {
            const auto begin = static_cast<int> (chord * chordSeconds * sampleRate);
            const auto end = static_cast<int> ((chord + 1) * chordSeconds * sampleRate);
            const auto chordRoot = mod12 (root + chordRoots[chord]);
            const auto third = chordMinor[chord] ? 3 : 4;
            addTone (output, sampleRate, begin, end, frequencyForMidi (36 + chordRoot) * detuneRatio, 0.25);
            addTone (output, sampleRate, begin, end, frequencyForMidi (48 + chordRoot) * detuneRatio, 0.18);
            addTone (output, sampleRate, begin, end, frequencyForMidi (48 + chordRoot + third) * detuneRatio, 0.14);
            addTone (output, sampleRate, begin, end, frequencyForMidi (48 + chordRoot + 7) * detuneRatio, 0.14);
            // A sustained characteristic degree makes major/minor differentiation explicit.
            const auto characteristic = mode == 0 ? root + 11 : root + 8;
            addTone (output, sampleRate, begin, end, frequencyForMidi (60 + mod12 (characteristic)) * detuneRatio, 0.055);
        }
        const auto kickInterval = static_cast<int> (0.5 * sampleRate);
        for (int start = 0; start < static_cast<int> (count); start += kickInterval)
            for (int n = 0; n < 550 && start + n < static_cast<int> (count); ++n)
                output[static_cast<size_t> (start + n)] += static_cast<float> (0.12 * std::exp (-static_cast<double> (n) / 90.0));
        return output;
    }

    std::vector<float> makeAmbiguousTriad (double sampleRate)
    {
        std::vector<float> output (static_cast<size_t> (16.0 * sampleRate), 0.0f);
        for (const auto midi : { 48, 52, 55 })
            addTone (output, sampleRate, 0, static_cast<int> (output.size()), frequencyForMidi (midi), 0.15);
        return output;
    }

    std::vector<float> makePercussionOnly (double sampleRate)
    {
        auto output = makeClickBeat (120.0, 16.0, sampleRate);
        unsigned state = 0x13579bdu;
        for (auto& sample : output)
        {
            state = state * 1664525u + 1013904223u;
            sample += static_cast<float> ((((state >> 8) & 0xffff) / 65535.0 - 0.5) * 0.025);
        }
        return output;
    }

    std::string jsonEscape (const std::string& value)
    {
        std::string output;
        for (const auto character : value)
        {
            if (character == '"' || character == '\\') output += '\\';
            output += character;
        }
        return output;
    }

    void writeReport (const std::vector<TestRecord>& records)
    {
        std::ofstream output ("analysis-core-synthetic-results.json", std::ios::trunc);
        output << "{\n  \"tests\": [\n";
        for (size_t index = 0; index < records.size(); ++index)
        {
            const auto& value = records[index];
            output << "    {\"fixture\":\"" << jsonEscape (value.name) << "\",\"pass\":" << (value.passed ? "true" : "false")
                   << ",\"expected_bpm\":" << value.expectedBpm << ",\"detected_bpm\":" << value.detectedBpm
                   << ",\"bpm_error\":" << std::abs (value.expectedBpm - value.detectedBpm)
                   << ",\"expected_root\":" << value.expectedRoot << ",\"expected_mode\":" << value.expectedMode
                   << ",\"detected_root\":" << value.detectedRoot << ",\"detected_mode\":" << value.detectedMode
                   << ",\"bpm_confidence\":" << value.bpmConfidence << ",\"key_confidence\":" << value.keyConfidence
                   << ",\"warning\":\"" << jsonEscape (value.warning) << "\"}" << (index + 1 == records.size() ? "\n" : ",\n");
        }
        output << "  ]\n}\n";
    }

    bool checkTempo (std::vector<TestRecord>& records, double sampleRate, double bpm)
    {
        const auto result = tunerite::AnalysisCore::analyzeBeat (makeClickBeat (bpm, 16.0, sampleRate), sampleRate);
        const auto pass = result.tempoValid && std::abs (result.bpm - bpm) <= 0.25;
        std::cout << "Expected BPM: " << bpm << " Detected BPM: " << result.bpm << " BPM error: " << std::abs (result.bpm - bpm)
                  << " Confidence: " << result.bpmConfidence << " Result: " << (pass ? "PASS" : "FAIL") << "\n";
        records.push_back ({ "tempo_" + std::to_string (static_cast<int> (sampleRate)) + "_" + std::to_string (bpm), pass, bpm, result.bpm,
                             -1, -1, result.keyRoot, result.keyMode, result.bpmConfidence, result.keyConfidence, result.warning });
        return pass;
    }

    bool checkClippedTempo (std::vector<TestRecord>& records, double sampleRate)
    {
        auto samples = makeClickBeat (120.0, 16.0, sampleRate);
        for (auto& sample : samples) sample *= 1.65f;
        samples[samples.size() / 3] = std::numeric_limits<float>::infinity();
        const auto result = tunerite::AnalysisCore::analyzeBeat (samples, sampleRate);
        const auto pass = result.usableAudio && result.clippingDetected && result.nonFiniteSamples == 1
            && result.analysisBufferScale < 1.0 && result.tempoValid && std::abs (result.bpm - 120.0) <= 0.25;
        std::cout << "Clipped tempo detected BPM: " << result.bpm << " scale: " << result.analysisBufferScale
                  << " non-finite replaced: " << result.nonFiniteSamples << " Result: " << (pass ? "PASS" : "FAIL") << "\n";
        records.push_back ({ "clipped_and_nonfinite_tempo_120", pass, 120.0, result.bpm,
                             -1, -1, result.keyRoot, result.keyMode, result.bpmConfidence, result.keyConfidence, result.warning });
        return pass;
    }

    bool checkKey (std::vector<TestRecord>& records, int root, int mode, double detuneCents = 0.0)
    {
        constexpr double sampleRate = 44100.0;
        const auto result = tunerite::AnalysisCore::analyzeBeat (makeProgression (root, mode, sampleRate, detuneCents), sampleRate);
        const auto pass = result.keyValid && result.keyRoot == root && result.keyMode == mode;
        std::cout << "Expected key: " << names[root] << (mode == 0 ? " major" : " minor")
                  << " Detected key: " << (result.keyRoot >= 0 ? names[result.keyRoot] : "UNKNOWN")
                  << (result.keyMode == 0 ? " major" : result.keyMode == 1 ? " minor" : "")
                  << " Key confidence: " << result.keyConfidence
                  << " Harmonic: " << result.harmonicContentSufficient
                  << " Clarity: " << result.tonalClarity
                  << " Agreement: " << result.tonalWindowAgreement
                  << " Profile disagreement: " << result.profileDisagreement
                  << " Candidates: " << (result.keyCandidates.empty() ? "none" : result.keyCandidates[0])
                  << "," << (result.keyCandidates.size() < 2 ? "none" : result.keyCandidates[1])
                  << "," << (result.keyCandidates.size() < 3 ? "none" : result.keyCandidates[2])
                  << " Result: " << (pass ? "PASS" : "FAIL") << "\n";
        const auto suffix = detuneCents == 0.0 ? "" : "_detuned_" + std::to_string (static_cast<int> (detuneCents));
        records.push_back ({ "key_" + std::string (names[root]) + (mode == 0 ? "_major" : "_minor") + suffix, pass, 0.0, result.bpm,
                             root, mode, result.keyRoot, result.keyMode, result.bpmConfidence, result.keyConfidence, result.warning });
        return pass;
    }
}

int main()
{
    std::vector<TestRecord> records;
    bool passed = true;
    for (const auto sampleRate : { 44100.0, 48000.0 })
        for (const auto bpm : { 101.0, 120.0, 200.0 })
            passed = checkTempo (records, sampleRate, bpm) && passed;
    passed = checkClippedTempo (records, 48000.0) && passed;

    for (int root = 0; root < 12; ++root)
        for (int mode = 0; mode < 2; ++mode)
            passed = checkKey (records, root, mode) && passed;

    // Development adversarial cases: mild global detuning must not erase a clear tonic resolution.
    passed = checkKey (records, 0, 0, 22.0) && passed;
    passed = checkKey (records, 9, 1, -19.0) && passed;

    constexpr double sampleRate = 44100.0;
    const auto ambiguous = tunerite::AnalysisCore::analyzeBeat (makeAmbiguousTriad (sampleRate), sampleRate);
    const auto ambiguousPass = ! ambiguous.keyValid && ambiguous.keyUncertain;
    std::cout << "Ambiguous triad key valid: " << ambiguous.keyValid << " Result: " << (ambiguousPass ? "PASS" : "FAIL") << "\n";
    records.push_back ({ "ambiguous_c_e_g", ambiguousPass, 0.0, ambiguous.bpm, -1, -1, ambiguous.keyRoot, ambiguous.keyMode, ambiguous.bpmConfidence, ambiguous.keyConfidence, ambiguous.warning });
    passed = ambiguousPass && passed;

    const auto percussion = tunerite::AnalysisCore::analyzeBeat (makePercussionOnly (sampleRate), sampleRate);
    const auto percussionPass = ! percussion.keyValid && ! percussion.harmonicContentSufficient;
    std::cout << "Percussion-only key valid: " << percussion.keyValid << " Result: " << (percussionPass ? "PASS" : "FAIL") << "\n";
    records.push_back ({ "percussion_only", percussionPass, 120.0, percussion.bpm, -1, -1, percussion.keyRoot, percussion.keyMode, percussion.bpmConfidence, percussion.keyConfidence, percussion.warning });
    passed = percussionPass && passed;

    const std::vector<float> silence (static_cast<size_t> (sampleRate * 8.0), 0.0f);
    const auto silenceResult = tunerite::AnalysisCore::analyzeBeat (silence, sampleRate);
    const auto silencePass = ! silenceResult.usableAudio;
    std::cout << "Silence usable audio: " << silenceResult.usableAudio << " Result: " << (silencePass ? "PASS" : "FAIL") << "\n";
    records.push_back ({ "silence", silencePass, 0.0, silenceResult.bpm, -1, -1, silenceResult.keyRoot, silenceResult.keyMode, silenceResult.bpmConfidence, silenceResult.keyConfidence, silenceResult.warning });
    passed = silencePass && passed;

    writeReport (records);
    return passed ? 0 : 1;
}
