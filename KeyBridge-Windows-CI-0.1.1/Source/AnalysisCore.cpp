#include "AnalysisCore.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace
{
    constexpr double pi = 3.14159265358979323846;
    constexpr std::array<double, 12> majorProfile { { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 } };
    constexpr std::array<double, 12> minorProfile { { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 } };
    constexpr std::array<const char*, 12> noteNames { { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" } };

    double percentile (std::vector<double> values, double fraction)
    {
        if (values.empty())
            return 0.0;
        std::sort (values.begin(), values.end());
        const auto index = static_cast<size_t> (std::round (std::clamp (fraction, 0.0, 1.0) * static_cast<double> (values.size() - 1)));
        return values[index];
    }

    double correlation (const std::array<double, 12>& a, const std::array<double, 12>& b, int shift)
    {
        double meanA = std::accumulate (a.begin(), a.end(), 0.0) / 12.0;
        double meanB = std::accumulate (b.begin(), b.end(), 0.0) / 12.0;
        double numerator = 0.0;
        double energyA = 0.0;
        double energyB = 0.0;
        for (int i = 0; i < 12; ++i)
        {
            const auto da = a[static_cast<size_t> (i)] - meanA;
            const auto db = b[static_cast<size_t> ((i - shift + 12) % 12)] - meanB;
            numerator += da * db;
            energyA += da * da;
            energyB += db * db;
        }
        return numerator / std::sqrt (energyA * energyB + 1.0e-12);
    }

    std::string keyName (int root, int mode)
    {
        return std::string (noteNames[static_cast<size_t> ((root + 12) % 12)]) + (mode == 0 ? " Major" : " Minor");
    }

    double midiFromFrequency (double hz)
    {
        return 69.0 + 12.0 * std::log2 (hz / 440.0);
    }
}

namespace tunerite
{
    std::vector<float> AnalysisCore::preprocessMono (const std::vector<float>& samples, double& rms, double& peak)
    {
        rms = 0.0;
        peak = 0.0;
        if (samples.empty())
            return {};

        const auto mean = std::accumulate (samples.begin(), samples.end(), 0.0) / static_cast<double> (samples.size());
        std::vector<float> output;
        output.reserve (samples.size());
        double energy = 0.0;
        for (const auto sample : samples)
        {
            const auto value = static_cast<float> (sample - mean);
            peak = std::max (peak, std::abs (static_cast<double> (value)));
            energy += static_cast<double> (value) * static_cast<double> (value);
            output.push_back (value);
        }
        rms = std::sqrt (energy / static_cast<double> (samples.size()));
        return output;
    }

    BeatAnalysisResult AnalysisCore::analyzeBeat (const std::vector<float>& monoSamples, double sampleRate)
    {
        BeatAnalysisResult result;
        if (sampleRate <= 0.0 || monoSamples.size() < static_cast<size_t> (sampleRate * 4.0))
        {
            result.warning = "Analysis needs at least four seconds of beat audio.";
            return result;
        }

        auto samples = preprocessMono (monoSamples, result.rms, result.peak);
        result.durationSeconds = static_cast<double> (samples.size()) / sampleRate;
        if (result.rms < 1.0e-4)
        {
            result.warning = "Input is too quiet for beat analysis.";
            return result;
        }

        constexpr int frameSize = 1024;
        constexpr int hopSize = 256;
        const auto frameCount = static_cast<int> ((samples.size() - frameSize) / hopSize);
        if (frameCount < 16)
        {
            result.warning = "Analysis needs more beat frames.";
            return result;
        }

        std::vector<double> onset;
        onset.reserve (static_cast<size_t> (frameCount));
        std::vector<double> previousMagnitudes (frameSize / 2, 0.0);
        std::array<double, 12> chroma {};
        int tonalFrames = 0;

        for (int frame = 0; frame < frameCount; ++frame)
        {
            const int start = frame * hopSize;
            std::vector<double> magnitudes (frameSize / 2, 0.0);
            double flux = 0.0;
            double frameEnergy = 0.0;
            for (int k = 1; k < frameSize / 2; ++k)
            {
                double real = 0.0;
                double imag = 0.0;
                for (int n = 0; n < frameSize; ++n)
                {
                    const auto window = 0.5 - 0.5 * std::cos (2.0 * pi * static_cast<double> (n) / static_cast<double> (frameSize - 1));
                    const auto x = static_cast<double> (samples[static_cast<size_t> (start + n)]) * window;
                    const auto phase = 2.0 * pi * static_cast<double> (k * n) / static_cast<double> (frameSize);
                    real += x * std::cos (phase);
                    imag -= x * std::sin (phase);
                    frameEnergy += x * x;
                }
                const auto magnitude = std::log1p (std::sqrt (real * real + imag * imag));
                magnitudes[static_cast<size_t> (k)] = magnitude;
                flux += std::max (0.0, magnitude - previousMagnitudes[static_cast<size_t> (k)]);
            }
            onset.push_back (flux);

            const auto medianFlux = frame > 0 ? onset[static_cast<size_t> (frame - 1)] : flux;
            const bool tonalFrame = flux < medianFlux * 1.8 && frameEnergy > 1.0e-6;
            if (tonalFrame)
            {
                ++tonalFrames;
                for (int k = 1; k < frameSize / 2; ++k)
                {
                    const auto hz = static_cast<double> (k) * sampleRate / static_cast<double> (frameSize);
                    if (hz < 55.0 || hz > 5000.0)
                        continue;
                    const auto midi = midiFromFrequency (hz);
                    const auto nearest = static_cast<int> (std::lround (midi));
                    const auto centsDistance = std::abs (midi - static_cast<double> (nearest));
                    const auto weight = magnitudes[static_cast<size_t> (k)] * std::max (0.0, 1.0 - centsDistance * 1.5);
                    chroma[static_cast<size_t> ((nearest % 12 + 12) % 12)] += weight;
                }
            }
            previousMagnitudes.swap (magnitudes);
        }

        const auto onsetMean = std::accumulate (onset.begin(), onset.end(), 0.0) / static_cast<double> (onset.size());
        for (auto& value : onset)
            value = std::max (0.0, value - onsetMean * 0.4);

        std::vector<TempoCandidate> candidates;
        const int minLag = std::max (1, static_cast<int> (std::floor (60.0 * sampleRate / (240.0 * hopSize))));
        const int maxLag = std::min (static_cast<int> (onset.size()) - 2, static_cast<int> (std::ceil (60.0 * sampleRate / (40.0 * hopSize))));
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double score = 0.0;
            for (size_t i = static_cast<size_t> (lag); i < onset.size(); ++i)
                score += onset[i] * onset[i - static_cast<size_t> (lag)];
            const auto bpm = 60.0 * sampleRate / (static_cast<double> (hopSize) * static_cast<double> (lag));
            candidates.push_back ({ bpm, score / static_cast<double> (onset.size() - static_cast<size_t> (lag)) });
        }
        std::sort (candidates.begin(), candidates.end(), [] (const auto& a, const auto& b) { return a.score > b.score; });
        for (const auto& candidate : candidates)
        {
            bool distinct = true;
            for (const auto& kept : result.tempoCandidates)
                if (std::abs (kept.bpm - candidate.bpm) < 2.0)
                    distinct = false;
            if (distinct)
                result.tempoCandidates.push_back (candidate);
            if (result.tempoCandidates.size() == 3)
                break;
        }

        if (result.tempoCandidates.empty() || result.tempoCandidates.front().score <= 0.0)
        {
            result.warning = "No stable onset periodicity was found.";
            return result;
        }

        result.bpm = result.tempoCandidates.front().bpm;
        result.alternativeBpm = result.tempoCandidates.size() > 1 ? result.tempoCandidates[1].bpm : 0.0;
        result.halfTimeBpm = result.bpm * 0.5;
        result.doubleTimeBpm = result.bpm * 2.0;
        const auto runnerUp = result.tempoCandidates.size() > 1 ? result.tempoCandidates[1].score : 0.0;
        const auto margin = (result.tempoCandidates.front().score - runnerUp) / (result.tempoCandidates.front().score + 1.0e-12);
        const auto coverage = std::min (1.0, result.durationSeconds / 20.0);
        result.bpmConfidence = std::clamp (0.15 + 0.55 * margin + 0.30 * coverage, 0.0, 1.0);
        result.bpmUncertain = result.bpmConfidence < 0.55;

        double chromaTotal = std::accumulate (chroma.begin(), chroma.end(), 0.0);
        if (tonalFrames < 8 || chromaTotal < 1.0e-6)
        {
            result.usableAudio = true;
            result.keyUncertain = true;
            result.warning = "Tempo estimate available, but harmonic evidence is too weak for a confident key.";
            return result;
        }

        for (auto& value : chroma)
            value /= chromaTotal;
        result.chroma = chroma;

        struct KeyCandidate { int root; int mode; double score; };
        std::vector<KeyCandidate> keyCandidates;
        for (int root = 0; root < 12; ++root)
        {
            keyCandidates.push_back ({ root, 0, correlation (chroma, majorProfile, root) });
            keyCandidates.push_back ({ root, 1, correlation (chroma, minorProfile, root) });
        }
        std::sort (keyCandidates.begin(), keyCandidates.end(), [] (const auto& a, const auto& b) { return a.score > b.score; });
        for (int i = 0; i < std::min (3, static_cast<int> (keyCandidates.size())); ++i)
            result.keyCandidates.push_back (keyName (keyCandidates[static_cast<size_t> (i)].root, keyCandidates[static_cast<size_t> (i)].mode));

        const auto& winner = keyCandidates.front();
        const auto& second = keyCandidates[1];
        result.keyRoot = winner.root;
        result.keyMode = winner.mode;
        result.keyConfidence = std::clamp ((winner.score - second.score) / 0.35, 0.0, 1.0) * std::min (1.0, static_cast<double> (tonalFrames) / 40.0);
        result.modeConfidence = std::clamp (winner.score - second.score + 0.5, 0.0, 1.0);
        result.keyUncertain = result.keyConfidence < 0.55;
        result.usableAudio = true;
        if (result.keyUncertain)
            result.warning = "Key uncertainty: harmonic evidence or major/minor separation is weak.";
        return result;
    }

    VocalAnalysisResult AnalysisCore::analyzeVocal (const std::vector<float>& monoSamples, double sampleRate)
    {
        VocalAnalysisResult result;
        if (sampleRate <= 0.0 || monoSamples.size() < static_cast<size_t> (sampleRate * 2.0))
        {
            result.warning = "Analysis needs at least two seconds of isolated vocal audio.";
            return result;
        }

        auto samples = preprocessMono (monoSamples, result.rms, result.peak);
        if (result.rms < 1.0e-4)
        {
            result.warning = "Input is too quiet for vocal analysis.";
            return result;
        }

        constexpr int frameSize = 2048;
        constexpr int hopSize = 512;
        const int minLag = std::max (2, static_cast<int> (sampleRate / 1000.0));
        const int maxLag = std::min (frameSize - 2, static_cast<int> (sampleRate / 50.0));
        const auto frameCount = static_cast<int> ((samples.size() - frameSize) / hopSize);
        std::vector<double> voicedMidi;
        std::vector<double> stableDiffs;
        int sustainedFrames = 0;
        int voicedFrames = 0;
        double previousMidi = std::numeric_limits<double>::quiet_NaN();
        int noteChanges = 0;

        for (int frame = 0; frame < frameCount; ++frame)
        {
            const int start = frame * hopSize;
            double frameEnergy = 0.0;
            for (int i = 0; i < frameSize; ++i)
                frameEnergy += static_cast<double> (samples[static_cast<size_t> (start + i)]) * static_cast<double> (samples[static_cast<size_t> (start + i)]);
            const auto frameRms = std::sqrt (frameEnergy / frameSize);
            if (frameRms < result.rms * 0.28)
                continue;

            double bestCorrelation = -1.0;
            int bestLag = 0;
            for (int lag = minLag; lag <= maxLag; ++lag)
            {
                double numerator = 0.0;
                double leftEnergy = 0.0;
                double rightEnergy = 0.0;
                for (int i = 0; i < frameSize - lag; ++i)
                {
                    const auto a = static_cast<double> (samples[static_cast<size_t> (start + i)]);
                    const auto b = static_cast<double> (samples[static_cast<size_t> (start + i + lag)]);
                    numerator += a * b;
                    leftEnergy += a * a;
                    rightEnergy += b * b;
                }
                const auto score = numerator / std::sqrt (leftEnergy * rightEnergy + 1.0e-12);
                if (score > bestCorrelation)
                {
                    bestCorrelation = score;
                    bestLag = lag;
                }
            }

            if (bestCorrelation < 0.55 || bestLag == 0)
                continue;
            const auto midi = midiFromFrequency (sampleRate / static_cast<double> (bestLag));
            if (midi < 24.0 || midi > 108.0)
                continue;
            if (! std::isnan (previousMidi) && std::abs (midi - previousMidi) > 8.0)
                continue;

            ++voicedFrames;
            voicedMidi.push_back (midi);
            result.midiContour.push_back (midi);
            if (! std::isnan (previousMidi))
            {
                const auto difference = std::abs (midi - previousMidi);
                stableDiffs.push_back (difference * 100.0);
                if (difference < 0.15)
                    ++sustainedFrames;
                if (difference > 0.8)
                    ++noteChanges;
            }
            previousMidi = midi;
            result.confidence += bestCorrelation;
        }

        result.voicedPercent = frameCount > 0 ? static_cast<double> (voicedFrames) / static_cast<double> (frameCount) : 0.0;
        if (voicedMidi.size() < 4)
        {
            result.warning = "Vocal pitch analysis limited: not enough reliable voiced frames.";
            return result;
        }

        result.lowMidi = percentile (voicedMidi, 0.05);
        result.highMidi = percentile (voicedMidi, 0.95);
        result.p05Midi = result.lowMidi;
        result.p95Midi = result.highMidi;
        result.averageMidi = std::accumulate (voicedMidi.begin(), voicedMidi.end(), 0.0) / static_cast<double> (voicedMidi.size());
        result.pitchStabilityCents = percentile (stableDiffs, 0.5);
        result.sustainedPercent = voicedFrames > 1 ? static_cast<double> (sustainedFrames) / static_cast<double> (voicedFrames - 1) : 0.0;
        result.noteChangeRate = static_cast<double> (noteChanges) / (static_cast<double> (samples.size()) / sampleRate);
        result.confidence /= static_cast<double> (voicedFrames);
        result.confidence *= std::min (1.0, result.voicedPercent * 1.8);
        result.melodic = result.voicedPercent > 0.35 && result.confidence > 0.58;
        result.uncertain = result.confidence < 0.55 || result.voicedPercent < 0.20;
        result.usableAudio = true;
        if (result.uncertain)
            result.warning = "Vocal analysis uncertainty: sparse pitched material or weak F0 tracking.";
        return result;
    }
}
