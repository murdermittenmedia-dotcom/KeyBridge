#include "AnalysisCore.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>

namespace
{
    constexpr double pi = 3.14159265358979323846;
    constexpr std::array<double, 12> krumhanslMajor { { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 } };
    constexpr std::array<double, 12> krumhanslMinor { { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 } };
    constexpr std::array<double, 12> temperleyMajor { { 0.748, 0.060, 0.488, 0.082, 0.670, 0.460, 0.096, 0.715, 0.104, 0.366, 0.057, 0.400 } };
    constexpr std::array<double, 12> temperleyMinor { { 0.712, 0.084, 0.474, 0.618, 0.049, 0.460, 0.105, 0.747, 0.404, 0.067, 0.330, 0.133 } };
    constexpr std::array<const char*, 12> noteNames { { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" } };

    double clamp01 (double value) { return std::clamp (value, 0.0, 1.0); }

    double percentile (std::vector<double> values, double fraction)
    {
        if (values.empty()) return 0.0;
        std::sort (values.begin(), values.end());
        const auto index = static_cast<size_t> (std::round (std::clamp (fraction, 0.0, 1.0) * static_cast<double> (values.size() - 1)));
        return values[index];
    }

    double midiFromFrequency (double hz, double tuningHz = 440.0)
    {
        return 69.0 + 12.0 * std::log2 (hz / tuningHz);
    }

    int positiveMod (int value, int modulus)
    {
        const auto result = value % modulus;
        return result < 0 ? result + modulus : result;
    }

    std::string keyName (int root, int mode)
    {
        if (root < 0 || mode < 0) return "Unknown";
        return std::string (noteNames[static_cast<size_t> (positiveMod (root, 12))]) + (mode == 0 ? " Major" : " Minor");
    }

    double profileCorrelation (const std::array<double, 12>& chroma, const std::array<double, 12>& profile, int root)
    {
        const auto meanA = std::accumulate (chroma.begin(), chroma.end(), 0.0) / 12.0;
        const auto meanB = std::accumulate (profile.begin(), profile.end(), 0.0) / 12.0;
        double numerator = 0.0, energyA = 0.0, energyB = 0.0;
        for (int i = 0; i < 12; ++i)
        {
            const auto a = chroma[static_cast<size_t> (i)] - meanA;
            const auto b = profile[static_cast<size_t> (positiveMod (i - root, 12))] - meanB;
            numerator += a * b;
            energyA += a * a;
            energyB += b * b;
        }
        return numerator / std::sqrt (energyA * energyB + 1.0e-15);
    }

    double safeLogFlatness (const std::vector<float>& spectrum, int lowBin, int highBin)
    {
        double logSum = 0.0, linearSum = 0.0;
        int count = 0;
        for (int bin = lowBin; bin <= highBin; ++bin)
        {
            const auto value = std::max (1.0e-12, static_cast<double> (spectrum[static_cast<size_t> (bin)]));
            logSum += std::log (value);
            linearSum += value;
            ++count;
        }
        if (count == 0) return 1.0;
        return std::exp (logSum / count) / (linearSum / count + 1.0e-15);
    }

    struct TempoWindow
    {
        double bpm = 0.0;
        double score = 0.0;
        double phase = 0.0;
    };

    TempoWindow estimateTempoWindow (const std::vector<double>& onset, int begin, int end, double onsetRate)
    {
        TempoWindow best;
        if (end - begin < 32) return best;
        std::vector<double> values (onset.begin() + begin, onset.begin() + end);
        const auto mean = std::accumulate (values.begin(), values.end(), 0.0) / values.size();
        for (auto& value : values) value = std::max (0.0, value - mean * 0.25);
        double energy = 0.0;
        for (const auto value : values) energy += value * value;
        if (energy < 1.0e-12) return best;

        const auto minLag = std::max (1, static_cast<int> (std::floor (60.0 * onsetRate / 240.0)));
        const auto maxLag = std::min (static_cast<int> (values.size()) - 2, static_cast<int> (std::ceil (60.0 * onsetRate / 40.0)));
        std::vector<double> autocorrelation (static_cast<size_t> (maxLag + 1), 0.0);
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double sum = 0.0, leftEnergy = 0.0, rightEnergy = 0.0;
            for (size_t index = static_cast<size_t> (lag); index < values.size(); ++index)
            {
                const auto left = values[index];
                const auto right = values[index - static_cast<size_t> (lag)];
                sum += left * right;
                leftEnergy += left * left;
                rightEnergy += right * right;
            }
            autocorrelation[static_cast<size_t> (lag)] = sum / std::sqrt (leftEnergy * rightEnergy + 1.0e-15);
        }

        std::vector<TempoWindow> localCandidates;
        for (int lag = minLag + 1; lag < maxLag - 1; ++lag)
        {
            const auto value = autocorrelation[static_cast<size_t> (lag)];
            if (value <= 0.0 || value < autocorrelation[static_cast<size_t> (lag - 1)] || value < autocorrelation[static_cast<size_t> (lag + 1)]) continue;
            const auto y0 = autocorrelation[static_cast<size_t> (lag - 1)];
            const auto y1 = value;
            const auto y2 = autocorrelation[static_cast<size_t> (lag + 1)];
            const auto offset = std::clamp (0.5 * (y0 - y2) / (y0 - 2.0 * y1 + y2 + 1.0e-15), -0.5, 0.5);
            const auto refinedLag = static_cast<double> (lag) + offset;
            double bestPhaseEnergy = 0.0, totalPhaseEnergy = 0.0;
            const auto roundedLag = std::max (1, static_cast<int> (std::round (refinedLag)));
            for (int phase = 0; phase < roundedLag; ++phase)
            {
                double phaseEnergy = 0.0;
                for (int index = phase; index < static_cast<int> (values.size()); index += roundedLag)
                    phaseEnergy += values[static_cast<size_t> (index)];
                bestPhaseEnergy = std::max (bestPhaseEnergy, phaseEnergy);
                totalPhaseEnergy += phaseEnergy;
            }
            const auto phaseScore = bestPhaseEnergy / (totalPhaseEnergy + 1.0e-15);
            const auto score = value * (0.78 + 0.22 * std::min (1.0, phaseScore * roundedLag));
            const TempoWindow candidate { 60.0 * onsetRate / refinedLag, score, phaseScore };
            localCandidates.push_back (candidate);
            if (score > best.score)
                best = candidate;
        }
        // A clearly supported shorter integer subperiod is the beat pulse, not merely its repeated bar period.
        if (best.bpm > 0.0)
        {
            for (const auto& candidate : localCandidates)
            {
                const auto ratio = candidate.bpm / best.bpm;
                const auto multiple = static_cast<int> (std::round (ratio));
                if (multiple >= 2 && multiple <= 4 && std::abs (ratio - multiple) < 0.04
                    && candidate.score >= best.score * 0.38 && candidate.phase >= 0.12)
                    best = candidate;
            }
        }

        // Local onset intervals resolve period multiples that remain equally periodic in autocorrelation.
        const auto peakThreshold = percentile (values, 0.72);
        const auto minimumPeakDistance = std::max (1, static_cast<int> (std::round (onsetRate * 0.12)));
        std::vector<int> peaks;
        for (int index = 1; index + 1 < static_cast<int> (values.size()); ++index)
        {
            if (values[static_cast<size_t> (index)] < peakThreshold
                || values[static_cast<size_t> (index)] < values[static_cast<size_t> (index - 1)]
                || values[static_cast<size_t> (index)] < values[static_cast<size_t> (index + 1)]) continue;
            if (! peaks.empty() && index - peaks.back() < minimumPeakDistance)
            {
                if (values[static_cast<size_t> (index)] > values[static_cast<size_t> (peaks.back())]) peaks.back() = index;
                continue;
            }
            peaks.push_back (index);
        }
        if (peaks.size() >= 6)
        {
            std::vector<double> intervals;
            for (size_t index = 1; index < peaks.size(); ++index)
            {
                const auto interval = peaks[index] - peaks[index - 1];
                const auto bpm = 60.0 * onsetRate / interval;
                if (bpm >= 40.0 && bpm <= 240.0) intervals.push_back (interval);
            }
            if (intervals.size() >= 5)
            {
                const auto medianInterval = percentile (intervals, 0.5);
                std::vector<double> deviations;
                for (const auto interval : intervals) deviations.push_back (std::abs (interval - medianInterval));
                const auto stability = clamp01 (1.0 - percentile (deviations, 0.5) / std::max (1.0, medianInterval * 0.08));
                const auto ioiScore = 0.70 + 0.30 * stability;
                if (stability >= 0.70 && (best.bpm <= 0.0 || ioiScore >= best.score * 0.88))
                    best = { 60.0 * onsetRate / medianInterval, ioiScore, stability };
            }
        }
        return best;
    }

    TempoWindow estimateDirectTransientTempo (const std::vector<float>& samples, double sampleRate)
    {
        TempoWindow result;
        if (samples.size() < static_cast<size_t> (sampleRate * 4.0)) return result;
        constexpr int envelopeFrame = 256;
        constexpr int envelopeHop = 128;
        std::vector<double> attackEnergy;
        std::vector<int> attackPositions;
        for (int start = 1; start + envelopeFrame < static_cast<int> (samples.size()); start += envelopeHop)
        {
            double energy = 0.0;
            for (int index = 0; index < envelopeFrame; ++index)
            {
                const auto difference = static_cast<double> (samples[static_cast<size_t> (start + index)]) - samples[static_cast<size_t> (start + index - 1)];
                energy += difference * difference;
            }
            attackEnergy.push_back (std::sqrt (energy / envelopeFrame));
            attackPositions.push_back (start);
        }
        if (attackEnergy.size() < 12) return result;
        const auto threshold = percentile (attackEnergy, 0.970);
        const auto minimumDistance = std::max (1, static_cast<int> (std::round (0.12 * sampleRate / envelopeHop)));
        std::vector<int> peaks;
        for (int index = 1; index + 1 < static_cast<int> (attackEnergy.size()); ++index)
        {
            if (attackEnergy[static_cast<size_t> (index)] < threshold
                || attackEnergy[static_cast<size_t> (index)] < attackEnergy[static_cast<size_t> (index - 1)]
                || attackEnergy[static_cast<size_t> (index)] < attackEnergy[static_cast<size_t> (index + 1)]) continue;
            if (! peaks.empty() && index - peaks.back() < minimumDistance)
            {
                if (attackEnergy[static_cast<size_t> (index)] > attackEnergy[static_cast<size_t> (peaks.back())]) peaks.back() = index;
                continue;
            }
            peaks.push_back (index);
        }
        if (peaks.size() < 6) return result;
        std::vector<int> refinedPeaks;
        refinedPeaks.reserve (peaks.size());
        for (const auto peak : peaks)
        {
            const auto centre = attackPositions[static_cast<size_t> (peak)];
            const auto begin = std::max (1, centre - envelopeHop);
            const auto end = std::min (static_cast<int> (samples.size()) - 1, centre + envelopeHop);
            auto strongest = begin;
            auto strongestDerivative = 0.0;
            for (int index = begin; index <= end; ++index)
            {
                const auto derivative = std::abs (static_cast<double> (samples[static_cast<size_t> (index)]) - samples[static_cast<size_t> (index - 1)]);
                if (derivative > strongestDerivative)
                {
                    strongestDerivative = derivative;
                    strongest = index;
                }
            }
            if (refinedPeaks.empty() || strongest - refinedPeaks.back() >= static_cast<int> (sampleRate * 0.12))
                refinedPeaks.push_back (strongest);
        }
        if (refinedPeaks.size() < 6) return result;
        std::vector<double> intervals;
        for (size_t index = 1; index < refinedPeaks.size(); ++index)
        {
            const auto interval = refinedPeaks[index] - refinedPeaks[index - 1];
            const auto bpm = 60.0 * sampleRate / interval;
            if (bpm >= 40.0 && bpm <= 240.0) intervals.push_back (interval);
        }
        if (intervals.size() < 5) return result;
        const auto medianInterval = percentile (intervals, 0.5);
        std::vector<double> deviations;
        for (const auto interval : intervals) deviations.push_back (std::abs (interval - medianInterval));
        const auto stability = clamp01 (1.0 - percentile (deviations, 0.5) / std::max (1.0, medianInterval * 0.025));
        if (stability < 0.70) return result;
        result = { 60.0 * sampleRate / medianInterval, 0.75 + 0.25 * stability, stability };
        return result;
    }

    struct KeyScore
    {
        int root = -1;
        int mode = -1;
        double score = -1.0;
    };

    std::vector<KeyScore> scoreKeys (const std::array<double, 12>& chroma)
    {
        std::vector<KeyScore> result;
        result.reserve (24);
        for (int root = 0; root < 12; ++root)
        {
            const auto major = 0.60 * profileCorrelation (chroma, krumhanslMajor, root)
                + 0.40 * profileCorrelation (chroma, temperleyMajor, root);
            const auto minor = 0.60 * profileCorrelation (chroma, krumhanslMinor, root)
                + 0.40 * profileCorrelation (chroma, temperleyMinor, root);
            result.push_back ({ root, 0, major });
            result.push_back ({ root, 1, minor });
        }
        std::sort (result.begin(), result.end(), [] (const auto& a, const auto& b) { return a.score > b.score; });
        return result;
    }

    void addSoftChroma (std::array<double, 12>& chroma, double midi, double weight)
    {
        const auto lower = static_cast<int> (std::floor (midi));
        const auto fraction = midi - lower;
        const auto leftWeight = std::cos (fraction * pi * 0.5);
        const auto rightWeight = std::sin (fraction * pi * 0.5);
        chroma[static_cast<size_t> (positiveMod (lower, 12))] += weight * leftWeight * leftWeight;
        chroma[static_cast<size_t> (positiveMod (lower + 1, 12))] += weight * rightWeight * rightWeight;
    }

    double interpolatedPeakHz (const std::vector<float>& spectrum, int bin, double hzPerBin)
    {
        const auto previous = std::max (1.0e-12, static_cast<double> (spectrum[static_cast<size_t> (bin - 1)]));
        const auto centre = std::max (1.0e-12, static_cast<double> (spectrum[static_cast<size_t> (bin)]));
        const auto next = std::max (1.0e-12, static_cast<double> (spectrum[static_cast<size_t> (bin + 1)]));
        const auto logPrevious = std::log (previous);
        const auto logCentre = std::log (centre);
        const auto logNext = std::log (next);
        const auto offset = std::clamp (0.5 * (logPrevious - logNext) / (logPrevious - 2.0 * logCentre + logNext + 1.0e-15), -0.5, 0.5);
        return (bin + offset) * hzPerBin;
    }

    double chromaCosine (const std::array<double, 12>& left, const std::array<double, 12>& right)
    {
        double dot = 0.0, leftEnergy = 0.0, rightEnergy = 0.0;
        for (int index = 0; index < 12; ++index)
        {
            dot += left[static_cast<size_t> (index)] * right[static_cast<size_t> (index)];
            leftEnergy += left[static_cast<size_t> (index)] * left[static_cast<size_t> (index)];
            rightEnergy += right[static_cast<size_t> (index)] * right[static_cast<size_t> (index)];
        }
        return dot / std::sqrt (leftEnergy * rightEnergy + 1.0e-15);
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
        std::vector<float> output (samples.size());
        double energy = 0.0;
        for (size_t index = 0; index < samples.size(); ++index)
        {
            const auto value = static_cast<float> (samples[index] - dc);
            output[index] = value;
            peak = std::max (peak, std::abs (static_cast<double> (value)));
            energy += static_cast<double> (value) * value;
        }
        rms = std::sqrt (energy / samples.size());
        const auto analysisGain = std::min (8.0, 0.18 / (rms + 1.0e-12));
        for (auto& value : output) value = static_cast<float> (value * analysisGain);
        return output;
    }

    BeatAnalysisResult AnalysisCore::analyzeBeat (const std::vector<float>& monoSamples, double sampleRate)
    {
        BeatAnalysisResult result;
        if (sampleRate < 8000.0 || sampleRate > 192000.0)
        {
            result.warning = "Unsupported sample rate for beat analysis.";
            return result;
        }
        if (monoSamples.size() < static_cast<size_t> (sampleRate * 6.0))
        {
            result.warning = "Analysis needs at least six seconds of beat audio.";
            return result;
        }

        auto samples = preprocessMono (monoSamples, result.rms, result.peak);
        result.durationSeconds = static_cast<double> (samples.size()) / sampleRate;
        if (result.rms < 1.0e-4)
        {
            result.warning = "Input is too quiet for beat analysis.";
            return result;
        }
        if (result.peak >= 0.9995)
        {
            result.clippingDetected = true;
            result.warning = "Input is clipped beyond reliable beat analysis.";
            return result;
        }
        result.usableAudio = true;

        constexpr int fftOrder = 11;
        constexpr int fftSize = 1 << fftOrder;
        constexpr int hopSize = 256;
        juce::dsp::FFT fft (fftOrder);
        std::vector<float> fftData (static_cast<size_t> (fftSize * 2), 0.0f);
        std::vector<float> window (static_cast<size_t> (fftSize));
        for (int index = 0; index < fftSize; ++index)
            window[static_cast<size_t> (index)] = static_cast<float> (0.5 - 0.5 * std::cos (2.0 * pi * index / (fftSize - 1)));

        const auto frameCount = std::max (0, static_cast<int> ((samples.size() - fftSize) / hopSize) + 1);
        if (frameCount < 12)
        {
            result.usableAudio = false;
            result.warning = "Insufficient usable analysis frames.";
            return result;
        }

        const auto hzPerBin = sampleRate / fftSize;
        const auto lowBin = std::max (1, static_cast<int> (std::ceil (30.0 / hzPerBin)));
        const auto highBin = std::min (fftSize / 2 - 1, static_cast<int> (std::floor (8000.0 / hzPerBin)));
        std::array<double, 3> previousBands {};
        std::vector<double> onset;
        onset.reserve (static_cast<size_t> (frameCount));
        std::vector<double> tuningAngles, tuningWeights;

        for (int frame = 0; frame < frameCount; ++frame)
        {
            const auto start = frame * hopSize;
            std::fill (fftData.begin(), fftData.end(), 0.0f);
            for (int index = 0; index < fftSize; ++index)
                fftData[static_cast<size_t> (index)] = samples[static_cast<size_t> (start + index)] * window[static_cast<size_t> (index)];
            fft.performFrequencyOnlyForwardTransform (fftData.data());

            std::array<double, 3> bandEnergy {};
            for (int bin = lowBin; bin <= highBin; ++bin)
            {
                const auto hz = bin * hzPerBin;
                const auto magnitude = static_cast<double> (fftData[static_cast<size_t> (bin)]);
                const auto band = hz < 180.0 ? 0 : hz < 1400.0 ? 1 : 2;
                bandEnergy[static_cast<size_t> (band)] += magnitude;
            }
            double flux = 0.0;
            for (int band = 0; band < 3; ++band)
            {
                const auto change = std::max (0.0, bandEnergy[static_cast<size_t> (band)] - previousBands[static_cast<size_t> (band)]);
                flux += change * (band == 0 ? 1.15 : band == 1 ? 1.0 : 0.85);
                previousBands[static_cast<size_t> (band)] = bandEnergy[static_cast<size_t> (band)];
            }
            onset.push_back (std::log1p (flux));

            if (frame % 4 != 0) continue;
            const auto flatness = safeLogFlatness (fftData, lowBin, highBin);
            const auto totalEnergy = std::accumulate (bandEnergy.begin(), bandEnergy.end(), 0.0);
            if (flatness > 0.68 || totalEnergy < 1.0e-7) continue;

            for (int bin = lowBin; bin <= highBin; ++bin)
            {
                const auto magnitude = static_cast<double> (fftData[static_cast<size_t> (bin)]);
                if (magnitude <= 1.0e-8) continue;
                const auto previous = bin > lowBin ? fftData[static_cast<size_t> (bin - 1)] : 0.0f;
                const auto next = bin < highBin ? fftData[static_cast<size_t> (bin + 1)] : 0.0f;
                if (magnitude < previous || magnitude < next) continue;
                const auto hz = interpolatedPeakHz (fftData, bin, hzPerBin);
                const auto midi = midiFromFrequency (hz);
                const auto nearest = std::round (midi);
                const auto cents = (midi - nearest) * 100.0;
                const auto weight = magnitude / std::sqrt (std::max (40.0, hz));
                tuningAngles.push_back (2.0 * pi * cents / 100.0);
                tuningWeights.push_back (weight);
            }
        }

        const auto onsetMean = std::accumulate (onset.begin(), onset.end(), 0.0) / onset.size();
        for (auto& value : onset) value = std::max (0.0, value - onsetMean * 0.40);
        result.onsetCoverage = static_cast<double> (std::count_if (onset.begin(), onset.end(), [] (double value) { return value > 0.0; })) / onset.size();
        if (result.onsetCoverage < 0.03)
        {
            result.warning = "No stable onset activity was found for tempo analysis.";
            return result;
        }

        double tuningX = 0.0, tuningY = 0.0, tuningWeight = 0.0;
        for (size_t index = 0; index < tuningAngles.size(); ++index)
        {
            tuningX += tuningWeights[index] * std::cos (tuningAngles[index]);
            tuningY += tuningWeights[index] * std::sin (tuningAngles[index]);
            tuningWeight += tuningWeights[index];
        }
        const auto tuningConcentration = std::sqrt (tuningX * tuningX + tuningY * tuningY) / (tuningWeight + 1.0e-15);
        const auto tuningCents = std::atan2 (tuningY, tuningX) * 100.0 / (2.0 * pi);
        result.tuningHz = 440.0 * std::pow (2.0, tuningCents / 1200.0);
        result.tuningConfidence = clamp01 (tuningConcentration);
        result.tuningAssumed = result.tuningConfidence < 0.45;
        if (result.tuningAssumed) result.tuningHz = 440.0;

        const auto onsetRate = sampleRate / hopSize;
        const auto windowFrames = std::max (24, static_cast<int> (std::round (8.0 * onsetRate)));
        const auto windowHop = std::max (1, windowFrames / 2);
        std::vector<TempoWindow> windowTempi;
        for (int start = 0; start + windowFrames <= static_cast<int> (onset.size()); start += windowHop)
        {
            const auto estimate = estimateTempoWindow (onset, start, start + windowFrames, onsetRate);
            if (estimate.bpm >= 40.0 && estimate.bpm <= 240.0 && estimate.score > 0.04)
                windowTempi.push_back (estimate);
        }
        if (windowTempi.empty())
        {
            result.warning = "No stable tempo candidate was found across analysis windows.";
            return result;
        }
        result.usableTempoWindows = static_cast<int> (windowTempi.size());
        const auto directTempo = estimateDirectTransientTempo (samples, sampleRate);

        struct Aggregate { double weightedBpm = 0.0; double score = 0.0; int count = 0; };
        std::map<int, Aggregate> aggregate;
        for (const auto& windowTempo : windowTempi)
        {
            const auto bucket = static_cast<int> (std::round (windowTempo.bpm * 4.0));
            auto& value = aggregate[bucket];
            value.weightedBpm += windowTempo.bpm * windowTempo.score;
            value.score += windowTempo.score;
            ++value.count;
        }
        for (const auto& [bucket, value] : aggregate)
        {
            juce::ignoreUnused (bucket);
            if (value.score > 0.0)
                result.tempoCandidates.push_back ({ value.weightedBpm / value.score, value.score / std::max (1, value.count) });
        }
        if (directTempo.bpm > 0.0)
            result.tempoCandidates.push_back ({ directTempo.bpm, directTempo.score * 1.03 });
        std::sort (result.tempoCandidates.begin(), result.tempoCandidates.end(), [] (const auto& a, const auto& b) { return a.score > b.score; });

        // Resolve a repeated-pulse family only when the faster integer multiple has comparable evidence across windows.
        if (! result.tempoCandidates.empty())
        {
            const auto leadingScore = result.tempoCandidates.front().score;
            for (auto& candidate : result.tempoCandidates)
            {
                const auto ratio = candidate.bpm / result.tempoCandidates.front().bpm;
                const auto multiple = static_cast<int> (std::round (ratio));
                if (multiple >= 2 && multiple <= 4 && std::abs (ratio - multiple) < 0.035 && candidate.score >= leadingScore * 0.42)
                    candidate.score = leadingScore * 1.002;
            }
            std::sort (result.tempoCandidates.begin(), result.tempoCandidates.end(), [] (const auto& a, const auto& b) { return a.score > b.score; });
        }
        if (result.tempoCandidates.empty())
        {
            result.warning = "No aggregate tempo candidate was found.";
            return result;
        }
        if (result.tempoCandidates.size() > 3) result.tempoCandidates.resize (3);
        const auto highStabilityDirectTempo = directTempo.bpm > 0.0 && directTempo.phase >= 0.93;
        result.bpm = highStabilityDirectTempo ? directTempo.bpm : result.tempoCandidates.front().bpm;
        result.alternativeBpm = 0.0;
        for (const auto& candidate : result.tempoCandidates)
            if (std::abs (candidate.bpm - result.bpm) > 0.25) { result.alternativeBpm = candidate.bpm; break; }
        result.halfTimeBpm = result.bpm * 0.5;
        result.doubleTimeBpm = result.bpm * 2.0;
        const auto runnerScore = result.tempoCandidates.size() > 1 ? result.tempoCandidates[1].score : 0.0;
        const auto margin = (result.tempoCandidates.front().score - runnerScore) / (result.tempoCandidates.front().score + 1.0e-15);
        std::vector<double> bpmValues;
        for (const auto& windowTempo : windowTempi) bpmValues.push_back (windowTempo.bpm);
        const auto medianBpm = percentile (bpmValues, 0.5);
        std::vector<double> deviations;
        for (const auto bpm : bpmValues) deviations.push_back (std::abs (bpm - medianBpm));
        result.tempoStability = highStabilityDirectTempo ? directTempo.phase : clamp01 (1.0 - percentile (deviations, 0.5) / 3.0);
        result.tempoAmbiguous = ! highStabilityDirectTempo && result.tempoCandidates.size() > 1
            && (margin < 0.16 || std::abs (result.bpm / result.alternativeBpm - 2.0) < 0.05 || std::abs (result.bpm / result.alternativeBpm - 0.5) < 0.05);
        result.bpmConfidence = highStabilityDirectTempo
            ? clamp01 (0.76 + 0.18 * directTempo.phase + 0.06 * std::min (1.0, result.usableTempoWindows / 3.0))
            : clamp01 (0.38 * clamp01 (margin / 0.35) + 0.36 * result.tempoStability + 0.16 * std::min (1.0, result.usableTempoWindows / 3.0) + 0.10 * clamp01 (result.onsetCoverage / 0.20));
        result.bpmUncertain = result.bpmConfidence < 0.60 || result.tempoAmbiguous;
        result.tempoValid = ! result.bpmUncertain;

        // Re-run tonal frames at a modest rate using the selected tuning and soft, non-nearest chroma binning.
        const auto transientThreshold = percentile (onset, 0.90);
        std::vector<std::array<double, 12>> tonalChromas;
        for (int frame = 0; frame < frameCount; frame += 4)
        {
            if (onset[static_cast<size_t> (frame)] > transientThreshold && onset[static_cast<size_t> (frame)] > 0.0) continue;
            const auto start = frame * hopSize;
            std::fill (fftData.begin(), fftData.end(), 0.0f);
            for (int index = 0; index < fftSize; ++index)
                fftData[static_cast<size_t> (index)] = samples[static_cast<size_t> (start + index)] * window[static_cast<size_t> (index)];
            fft.performFrequencyOnlyForwardTransform (fftData.data());
            const auto flatness = safeLogFlatness (fftData, lowBin, highBin);
            if (flatness > 0.68) continue;
            std::array<double, 12> frameChroma {};
            double frameWeight = 0.0;
            for (int bin = lowBin; bin <= highBin; ++bin)
            {
                const auto magnitude = static_cast<double> (fftData[static_cast<size_t> (bin)]);
                const auto previous = bin > lowBin ? fftData[static_cast<size_t> (bin - 1)] : 0.0f;
                const auto next = bin < highBin ? fftData[static_cast<size_t> (bin + 1)] : 0.0f;
                if (magnitude <= 1.0e-8 || magnitude < previous || magnitude < next) continue;
                const auto hz = interpolatedPeakHz (fftData, bin, hzPerBin);
                const auto weight = magnitude / std::sqrt (std::max (40.0, hz));
                addSoftChroma (frameChroma, midiFromFrequency (hz, result.tuningHz), weight);
                frameWeight += weight;
            }
            if (frameWeight > 1.0e-8)
            {
                for (auto& value : frameChroma) value /= frameWeight;
                tonalChromas.push_back (frameChroma);
            }
        }
        result.usableTonalWindows = static_cast<int> (tonalChromas.size());
        if (tonalChromas.size() < 6)
        {
            result.harmonicContentSufficient = false;
            result.keyValid = false;
            result.keyUncertain = true;
            result.warning = "Tempo estimate available, but harmonic content is insufficient for key detection.";
            return result;
        }

        for (const auto& frameChroma : tonalChromas)
            for (int index = 0; index < 12; ++index)
                result.chroma[static_cast<size_t> (index)] += frameChroma[static_cast<size_t> (index)];
        for (auto& value : result.chroma) value /= tonalChromas.size();
        const auto energy = std::accumulate (result.chroma.begin(), result.chroma.end(), 0.0);
        if (energy <= 1.0e-10)
        {
            result.harmonicContentSufficient = false;
            result.warning = "No reliable harmonic pitch-class energy was found.";
            return result;
        }
        for (auto& value : result.chroma) value /= energy;
        double chromaEntropy = 0.0;
        for (const auto value : result.chroma)
            if (value > 1.0e-12) chromaEntropy -= value * std::log (value);
        const auto pitchClassConcentration = clamp01 (1.0 - chromaEntropy / std::log (12.0));
        const auto keys = scoreKeys (result.chroma);
        for (int index = 0; index < 3; ++index)
        {
            result.keyCandidates.push_back (keyName (keys[static_cast<size_t> (index)].root, keys[static_cast<size_t> (index)].mode));
            result.keyCandidateScores.push_back ({ keys[static_cast<size_t> (index)].root, keys[static_cast<size_t> (index)].mode, keys[static_cast<size_t> (index)].score });
        }
        const auto clarity = keys.front().score - keys[1].score;
        result.tonalClarity = clamp01 (clarity / 0.25);
        double agreementSum = 0.0;
        for (const auto& frameChroma : tonalChromas)
            agreementSum += std::max (0.0, chromaCosine (frameChroma, result.chroma));
        result.tonalWindowAgreement = agreementSum / tonalChromas.size();
        const auto tonalMotion = clamp01 (1.0 - result.tonalWindowAgreement);
        result.harmonicContentSufficient = result.tonalWindowAgreement >= 0.42
            && result.tonalClarity >= 0.12
            && pitchClassConcentration >= 0.12
            && tonalMotion >= 0.025;
        result.relativeModeAmbiguous = keys.front().mode != keys[1].mode && clarity < 0.08;
        result.keyConfidence = clamp01 (0.48 * result.tonalClarity + 0.32 * result.tonalWindowAgreement + 0.12 * std::min (1.0, tonalChromas.size() / 24.0) + 0.08 * result.tuningConfidence);
        result.modeConfidence = clamp01 (clarity / 0.18);
        result.keyUncertain = ! result.harmonicContentSufficient || result.keyConfidence < 0.55 || result.relativeModeAmbiguous;
        result.keyValid = ! result.keyUncertain;
        if (result.keyValid)
        {
            result.keyRoot = keys.front().root;
            result.keyMode = keys.front().mode;
        }
        if (! result.keyValid)
            result.warning = result.harmonicContentSufficient ? "Key uncertainty: competing tonal candidates or unstable harmonic windows." : "Insufficient harmonic content for key detection.";
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
