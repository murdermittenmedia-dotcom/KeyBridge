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
        if (values.empty()) return 0.0;
        std::sort (values.begin(), values.end());
        return values[static_cast<size_t> (std::round (std::clamp (fraction, 0.0, 1.0) * static_cast<double> (values.size() - 1)))];
    }

    double midiFromFrequency (double hz)
    {
        return 69.0 + 12.0 * std::log2 (hz / 440.0);
    }

    double goertzelPower (const std::vector<float>& samples, int start, int size, double frequency, double sampleRate)
    {
        const auto omega = 2.0 * pi * frequency / sampleRate;
        const auto coefficient = 2.0 * std::cos (omega);
        double previous = 0.0;
        double previousPrevious = 0.0;
        for (int n = 0; n < size; ++n)
        {
            const auto window = 0.5 - 0.5 * std::cos (2.0 * pi * static_cast<double> (n) / static_cast<double> (size - 1));
            const auto current = static_cast<double> (samples[static_cast<size_t> (start + n)]) * window + coefficient * previous - previousPrevious;
            previousPrevious = previous;
            previous = current;
        }
        return previousPrevious * previousPrevious + previous * previous - coefficient * previous * previousPrevious;
    }

    double profileCorrelation (const std::array<double, 12>& chroma, const std::array<double, 12>& profile, int shift)
    {
        double meanA = std::accumulate (chroma.begin(), chroma.end(), 0.0) / 12.0;
        double meanB = std::accumulate (profile.begin(), profile.end(), 0.0) / 12.0;
        double numerator = 0.0, energyA = 0.0, energyB = 0.0;
        for (int i = 0; i < 12; ++i)
        {
            const auto a = chroma[static_cast<size_t> (i)] - meanA;
            const auto b = profile[static_cast<size_t> ((i - shift + 12) % 12)] - meanB;
            numerator += a * b;
            energyA += a * a;
            energyB += b * b;
        }
        return numerator / std::sqrt (energyA * energyB + 1.0e-12);
    }

    std::string keyName (int root, int mode)
    {
        return std::string (noteNames[static_cast<size_t> ((root + 12) % 12)]) + (mode == 0 ? " Major" : " Minor");
    }
}

namespace tunerite
{
    std::vector<float> AnalysisCore::preprocessMono (const std::vector<float>& samples, double& rms, double& peak)
    {
        rms = 0.0;
        peak = 0.0;
        if (samples.empty()) return {};
        const auto dc = std::accumulate (samples.begin(), samples.end(), 0.0) / static_cast<double> (samples.size());
        std::vector<float> output;
        output.reserve (samples.size());
        double energy = 0.0;
        for (const auto sample : samples)
        {
            const auto value = static_cast<float> (sample - dc);
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

        constexpr int onsetFrame = 1024;
        constexpr int onsetHop = 256;
        const auto onsetFrames = static_cast<int> ((samples.size() - onsetFrame) / onsetHop);
        std::vector<double> onset;
        onset.reserve (static_cast<size_t> (onsetFrames));
        double previousBandEnergy = 0.0;
        for (int frame = 0; frame < onsetFrames; ++frame)
        {
            const auto start = frame * onsetHop;
            double highPassedEnergy = 0.0;
            for (int n = 1; n < onsetFrame; ++n)
            {
                const auto difference = static_cast<double> (samples[static_cast<size_t> (start + n)]) - static_cast<double> (samples[static_cast<size_t> (start + n - 1)]);
                highPassedEnergy += difference * difference;
            }
            const auto flux = std::max (0.0, highPassedEnergy - previousBandEnergy);
            onset.push_back (std::log1p (flux));
            previousBandEnergy = highPassedEnergy;
        }
        const auto onsetMean = std::accumulate (onset.begin(), onset.end(), 0.0) / static_cast<double> (onset.size());
        for (auto& value : onset) value = std::max (0.0, value - onsetMean * 0.35);

        const int minLag = std::max (1, static_cast<int> (std::floor (60.0 * sampleRate / (240.0 * onsetHop))));
        const int maxLag = std::min (static_cast<int> (onset.size()) - 2, static_cast<int> (std::ceil (60.0 * sampleRate / (40.0 * onsetHop))));
        std::vector<TempoCandidate> candidates;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double score = 0.0;
            for (size_t i = static_cast<size_t> (lag); i < onset.size(); ++i)
                score += onset[i] * onset[i - static_cast<size_t> (lag)];
            candidates.push_back ({ 60.0 * sampleRate / (static_cast<double> (onsetHop) * lag), score / static_cast<double> (onset.size() - static_cast<size_t> (lag)) });
        }
        // Preserve raw onset scores while penalising longer periodic multiples below.
        std::vector<double> rawTempoScores;
        rawTempoScores.reserve (candidates.size());
        for (const auto& candidate : candidates) rawTempoScores.push_back (candidate.score);

        // A long autocorrelation lag can be an integer multiple of a shorter repeating beat period.
        // Penalise those multiples only when a corresponding shorter lag carries nearly the same onset evidence.
        for (auto& candidate : candidates)
        {
            const auto lag = static_cast<int> (std::round (60.0 * sampleRate / (static_cast<double> (onsetHop) * candidate.bpm)));
            if (candidate.score <= 0.0 || lag <= minLag) continue;
            double strongestSubperiod = 0.0;
            for (int divisor = 2; divisor <= 4; ++divisor)
            {
                const auto subLag = static_cast<int> (std::round (static_cast<double> (lag) / divisor));
                if (subLag < minLag || subLag >= lag || subLag > maxLag) continue;
                strongestSubperiod = std::max (strongestSubperiod, rawTempoScores[static_cast<size_t> (subLag - minLag)]);
            }
            const auto subperiodRatio = strongestSubperiod / candidate.score;
            if (subperiodRatio >= 0.75)
                candidate.score *= std::max (0.20, 1.0 - 0.60 * subperiodRatio);
        }
        std::sort (candidates.begin(), candidates.end(), [] (const auto& a, const auto& b) { return a.score > b.score; });
        if (! candidates.empty() && candidates.front().score > 0.0)
        {
            const auto leadingScore = candidates.front().score;
            const auto leadingBpm = candidates.front().bpm;
            for (auto& candidate : candidates)
            {
                const auto ratio = candidate.bpm / leadingBpm;
                const auto nearestMultiple = static_cast<int> (std::round (ratio));
                const bool isCloseHigherMultiple = nearestMultiple >= 2 && nearestMultiple <= 4
                    && std::abs (ratio - nearestMultiple) < 0.08;
                if (isCloseHigherMultiple && candidate.score >= leadingScore * 0.35)
                    candidate.score = leadingScore * 1.001;
            }
            std::sort (candidates.begin(), candidates.end(), [] (const auto& a, const auto& b) { return a.score > b.score; });
        }
        for (const auto& candidate : candidates)
        {
            const bool distinct = std::none_of (result.tempoCandidates.begin(), result.tempoCandidates.end(), [&candidate] (const auto& kept) { return std::abs (kept.bpm - candidate.bpm) < 1.5; });
            if (distinct) result.tempoCandidates.push_back (candidate);
            if (result.tempoCandidates.size() == 3) break;
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
        result.bpmConfidence = std::clamp (0.10 + 0.55 * margin + 0.35 * std::min (1.0, result.durationSeconds / 20.0), 0.0, 1.0);
        result.bpmUncertain = result.bpmConfidence < 0.60;

        constexpr int tonalFrame = 2048;
        constexpr int tonalHop = 1024;
        const auto tonalFrames = static_cast<int> ((samples.size() - tonalFrame) / tonalHop);
        std::array<double, 12> chroma {};
        int usableTonalFrames = 0;
        for (int frame = 0; frame < tonalFrames; ++frame)
        {
            const auto start = frame * tonalHop;
            double energy = 0.0;
            for (int n = 0; n < tonalFrame; ++n)
                energy += static_cast<double> (samples[static_cast<size_t> (start + n)]) * static_cast<double> (samples[static_cast<size_t> (start + n)]);
            if (energy < 1.0e-6) continue;
            ++usableTonalFrames;
            for (int midi = 36; midi <= 84; ++midi)
            {
                const auto frequency = 440.0 * std::pow (2.0, (static_cast<double> (midi) - 69.0) / 12.0);
                const auto power = std::max (0.0, goertzelPower (samples, start, tonalFrame, frequency, sampleRate));
                chroma[static_cast<size_t> (midi % 12)] += std::sqrt (power) / (1.0 + 0.015 * std::abs (midi - 60));
            }
        }
        const auto totalChroma = std::accumulate (chroma.begin(), chroma.end(), 0.0);
        if (usableTonalFrames < 4 || totalChroma < 1.0e-6)
        {
            result.usableAudio = true;
            result.warning = "Tempo estimate available, but harmonic evidence is weak.";
            return result;
        }
        for (auto& value : chroma) value /= totalChroma;
        result.chroma = chroma;

        struct Candidate { int root; int mode; double score; };
        std::vector<Candidate> keys;
        for (int root = 0; root < 12; ++root)
        {
            keys.push_back ({ root, 0, profileCorrelation (chroma, majorProfile, root) });
            keys.push_back ({ root, 1, profileCorrelation (chroma, minorProfile, root) });
        }
        std::sort (keys.begin(), keys.end(), [] (const auto& a, const auto& b) { return a.score > b.score; });
        for (int i = 0; i < 3; ++i) result.keyCandidates.push_back (keyName (keys[static_cast<size_t> (i)].root, keys[static_cast<size_t> (i)].mode));
        result.keyRoot = keys.front().root;
        result.keyMode = keys.front().mode;
        result.keyConfidence = std::clamp ((keys.front().score - keys[1].score) / 0.30, 0.0, 1.0) * std::min (1.0, usableTonalFrames / 24.0);
        result.modeConfidence = std::clamp (keys.front().score - keys[1].score + 0.5, 0.0, 1.0);
        result.keyUncertain = result.keyConfidence < 0.60;
        result.usableAudio = true;
        if (result.keyUncertain) result.warning = "Key uncertainty: the leading key hypothesis is too close to alternatives.";
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
        const auto minLag = std::max (2, static_cast<int> (sampleRate / 1000.0));
        const auto maxLag = std::min (frameSize - 2, static_cast<int> (sampleRate / 50.0));
        const auto frameCount = static_cast<int> ((samples.size() - frameSize) / hopSize);
        std::vector<double> voicedMidi, stability;
        int voiced = 0, sustained = 0, changes = 0;
        double previousMidi = std::numeric_limits<double>::quiet_NaN();

        for (int frame = 0; frame < frameCount; ++frame)
        {
            const auto start = frame * hopSize;
            double frameEnergy = 0.0;
            for (int n = 0; n < frameSize; ++n) frameEnergy += static_cast<double> (samples[static_cast<size_t> (start + n)]) * samples[static_cast<size_t> (start + n)];
            const auto frameRms = std::sqrt (frameEnergy / frameSize);
            if (frameRms < result.rms * 0.30) continue;

            std::vector<double> correlation;
            correlation.reserve (static_cast<size_t> (maxLag - minLag + 1));
            for (int lag = minLag; lag <= maxLag; ++lag)
            {
                double numerator = 0.0, leftEnergy = 0.0, rightEnergy = 0.0;
                for (int n = 0; n < frameSize - lag; ++n)
                {
                    const auto left = static_cast<double> (samples[static_cast<size_t> (start + n)]);
                    const auto right = static_cast<double> (samples[static_cast<size_t> (start + n + lag)]);
                    numerator += left * right;
                    leftEnergy += left * left;
                    rightEnergy += right * right;
                }
                correlation.push_back (numerator / std::sqrt (leftEnergy * rightEnergy + 1.0e-12));
            }
            double bestScore = -1.0;
            int bestLag = 0;
            for (size_t index = 1; index + 1 < correlation.size(); ++index)
            {
                const auto score = correlation[index];
                if (score >= 0.58 && score >= correlation[index - 1] && score > correlation[index + 1])
                {
                    bestScore = score;
                    bestLag = minLag + static_cast<int> (index);
                    break;
                }
            }
            if (bestScore < 0.58 || bestLag == 0) continue;
            const auto midi = midiFromFrequency (sampleRate / bestLag);
            if (midi < 24.0 || midi > 108.0) continue;
            if (! std::isnan (previousMidi) && std::abs (midi - previousMidi) > 8.0) continue;

            ++voiced;
            voicedMidi.push_back (midi);
            result.midiContour.push_back (midi);
            result.confidence += bestScore;
            if (! std::isnan (previousMidi))
            {
                const auto diff = std::abs (midi - previousMidi);
                stability.push_back (diff * 100.0);
                if (diff < 0.15) ++sustained;
                if (diff > 0.8) ++changes;
            }
            previousMidi = midi;
        }

        result.voicedPercent = frameCount > 0 ? static_cast<double> (voiced) / frameCount : 0.0;
        if (voicedMidi.size() < 4)
        {
            result.warning = "Vocal analysis limited: insufficient reliable voiced frames.";
            return result;
        }
        result.lowMidi = percentile (voicedMidi, 0.05);
        result.highMidi = percentile (voicedMidi, 0.95);
        result.p05Midi = result.lowMidi;
        result.p95Midi = result.highMidi;
        result.averageMidi = std::accumulate (voicedMidi.begin(), voicedMidi.end(), 0.0) / voicedMidi.size();
        result.pitchStabilityCents = percentile (stability, 0.50);
        result.sustainedPercent = voiced > 1 ? static_cast<double> (sustained) / (voiced - 1) : 0.0;
        result.noteChangeRate = static_cast<double> (changes) / (samples.size() / sampleRate);
        result.confidence = (result.confidence / voiced) * std::min (1.0, result.voicedPercent * 1.8);
        result.melodic = result.voicedPercent > 0.35 && result.confidence > 0.60;
        result.uncertain = result.confidence < 0.60 || result.voicedPercent < 0.20;
        result.usableAudio = true;
        if (result.uncertain) result.warning = "Vocal uncertainty: sparse pitched material or weak F0 evidence.";
        return result;
    }
}
