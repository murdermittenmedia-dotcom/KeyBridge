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

    double trimmedMean (std::vector<double> values, double trimFraction = 0.15)
    {
        if (values.empty()) return 0.0;
        std::sort (values.begin(), values.end());
        const auto trim = std::min (values.size() / 4, static_cast<size_t> (std::floor (values.size() * trimFraction)));
        const auto first = values.begin() + static_cast<std::ptrdiff_t> (trim);
        const auto last = values.end() - static_cast<std::ptrdiff_t> (trim);
        if (first >= last) return percentile (values, 0.5);
        return std::accumulate (first, last, 0.0) / static_cast<double> (std::distance (first, last));
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

    struct PreparedAnalysisInput
    {
        std::vector<float> samples;
        double rms = 0.0;
        double peak = 0.0;
        double clippingAmount = 0.0;
        double scale = 1.0;
        double quality = 1.0;
        int nonFiniteSamples = 0;
    };

    // This conditioning is deliberately analysis-only. Callers retain ownership of the original audio.
    PreparedAnalysisInput prepareAnalysisInput (const std::vector<float>& input)
    {
        PreparedAnalysisInput prepared;
        prepared.samples.resize (input.size(), 0.0f);
        if (input.empty()) return prepared;

        double energy = 0.0;
        for (size_t index = 0; index < input.size(); ++index)
        {
            const auto source = static_cast<double> (input[index]);
            const auto finite = std::isfinite (source) ? source : 0.0;
            if (! std::isfinite (source)) ++prepared.nonFiniteSamples;
            prepared.samples[index] = static_cast<float> (finite);
            prepared.peak = std::max (prepared.peak, std::abs (finite));
            energy += finite * finite;
        }
        prepared.rms = std::sqrt (energy / static_cast<double> (input.size()));
        prepared.clippingAmount = std::max (0.0, prepared.peak - 0.99);
        if (prepared.peak > 0.99)
        {
            prepared.scale = 0.99 / prepared.peak;
            for (auto& value : prepared.samples) value = static_cast<float> (value * prepared.scale);
        }

        const auto nonFiniteRatio = static_cast<double> (prepared.nonFiniteSamples) / static_cast<double> (input.size());
        const auto clippingPenalty = clamp01 (prepared.clippingAmount / 0.50) * 0.35;
        prepared.quality = clamp01 (1.0 - clippingPenalty - std::min (0.50, nonFiniteRatio * 8.0));
        return prepared;
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

        std::vector<double> rawDerivative;
        rawDerivative.reserve (samples.size() - 1);
        for (size_t index = 1; index < samples.size(); ++index)
            rawDerivative.push_back (std::abs (static_cast<double> (samples[index]) - samples[index - 1]));
        const auto rawThreshold = percentile (rawDerivative, 0.9999);
        const auto rawMinimumDistance = std::max (1, static_cast<int> (std::round (sampleRate * 0.12)));
        std::vector<int> rawPeaks;
        for (int index = 1; index + 1 < static_cast<int> (rawDerivative.size()); ++index)
        {
            if (rawDerivative[static_cast<size_t> (index)] < rawThreshold
                || rawDerivative[static_cast<size_t> (index)] < rawDerivative[static_cast<size_t> (index - 1)]
                || rawDerivative[static_cast<size_t> (index)] < rawDerivative[static_cast<size_t> (index + 1)]) continue;
            if (! rawPeaks.empty() && index - rawPeaks.back() < rawMinimumDistance)
            {
                if (rawDerivative[static_cast<size_t> (index)] > rawDerivative[static_cast<size_t> (rawPeaks.back())]) rawPeaks.back() = index;
                continue;
            }
            rawPeaks.push_back (index);
        }
        if (rawPeaks.size() >= 6)
        {
            std::vector<double> rawIntervals;
            for (size_t index = 1; index < rawPeaks.size(); ++index)
            {
                const auto interval = rawPeaks[index] - rawPeaks[index - 1];
                const auto bpm = 60.0 * sampleRate / interval;
                if (bpm >= 40.0 && bpm <= 240.0) rawIntervals.push_back (interval);
            }
            if (rawIntervals.size() >= 5)
            {
                const auto medianInterval = percentile (rawIntervals, 0.5);
                std::vector<double> deviations;
                for (const auto interval : rawIntervals) deviations.push_back (std::abs (interval - medianInterval));
                const auto stability = clamp01 (1.0 - percentile (deviations, 0.5) / std::max (1.0, medianInterval * 0.01));
                if (stability >= 0.85)
                    return { 60.0 * sampleRate / medianInterval, 0.78 + 0.22 * stability, stability };
            }
        }

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

    KeyScore bestKeyForProfileFamily (const std::array<double, 12>& chroma,
                                      const std::array<double, 12>& majorProfile,
                                      const std::array<double, 12>& minorProfile)
    {
        KeyScore best;
        for (int root = 0; root < 12; ++root)
        {
            const auto major = profileCorrelation (chroma, majorProfile, root);
            const auto minor = profileCorrelation (chroma, minorProfile, root);
            if (major > best.score) best = { root, 0, major };
            if (minor > best.score) best = { root, 1, minor };
        }
        return best;
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

        // A second finite-value guard keeps direct callers, including vocal analysis, safe.
        double sum = 0.0;
        for (const auto sample : samples)
            sum += std::isfinite (static_cast<double> (sample)) ? static_cast<double> (sample) : 0.0;
        const auto dc = sum / static_cast<double> (samples.size());

        std::vector<float> output (samples.size());
        double energy = 0.0;
        for (size_t index = 0; index < samples.size(); ++index)
        {
            const auto source = static_cast<double> (samples[index]);
            const auto finite = std::isfinite (source) ? source : 0.0;
            const auto value = static_cast<float> (finite - dc);
            output[index] = value;
            peak = std::max (peak, std::abs (static_cast<double> (value)));
            energy += static_cast<double> (value) * value;
        }
        rms = std::sqrt (energy / static_cast<double> (samples.size()));
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

        const auto prepared = prepareAnalysisInput (monoSamples);
        result.rms = prepared.rms;
        result.peak = prepared.peak;
        result.clippingAmount = prepared.clippingAmount;
        result.analysisBufferScale = prepared.scale;
        result.inputQuality = prepared.quality;
        result.nonFiniteSamples = prepared.nonFiniteSamples;
        result.clippingDetected = prepared.clippingAmount > 0.0;
        double conditionedRms = 0.0, conditionedPeak = 0.0;
        auto samples = preprocessMono (prepared.samples, conditionedRms, conditionedPeak);
        result.durationSeconds = static_cast<double> (samples.size()) / sampleRate;
        const auto appendInputQualityWarning = [&result]()
        {
            if (result.nonFiniteSamples > 0)
            {
                if (! result.warning.empty()) result.warning += " ";
                result.warning += std::to_string (result.nonFiniteSamples) + " non-finite sample(s) were replaced with zero.";
            }
            if (result.clippingDetected)
            {
                if (! result.warning.empty()) result.warning += " ";
                result.warning += "Clipping detected; analysis copy scaled by " + std::to_string (result.analysisBufferScale) + ".";
            }
        };
        if (conditionedRms < 1.0e-4)
        {
            result.warning = "Input is too quiet for beat analysis.";
            appendInputQualityWarning();
            return result;
        }
        // Clipping is retained as a quality warning. It must never independently reject BPM or key analysis.
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
            appendInputQualityWarning();
            return result;
        }

        const auto hzPerBin = sampleRate / fftSize;
        const auto lowBin = std::max (1, static_cast<int> (std::ceil (30.0 / hzPerBin)));
        const auto highBin = std::min (fftSize / 2 - 1, static_cast<int> (std::floor (8000.0 / hzPerBin)));
        std::array<double, 3> previousBands {};
        std::vector<double> onset;
        std::vector<double> tonalOnset;
        onset.reserve (static_cast<size_t> (frameCount));
        tonalOnset.reserve (static_cast<size_t> (frameCount));
        std::vector<double> tuningAngles, tuningWeights;

        for (int frame = 0; frame < frameCount; ++frame)
        {
            const auto start = frame * hopSize;
            std::fill (fftData.begin(), fftData.end(), 0.0f);
            for (int index = 0; index < fftSize; ++index)
                fftData[static_cast<size_t> (index)] = samples[static_cast<size_t> (start + index)] * window[static_cast<size_t> (index)];
            fft.performFrequencyOnlyForwardTransform (fftData.data());

            // Log-magnitude, multi-band spectral flux keeps a single loud band from dominating onset evidence.
            std::array<double, 3> bandEnergy {};
            for (int bin = lowBin; bin <= highBin; ++bin)
            {
                const auto hz = bin * hzPerBin;
                const auto magnitude = static_cast<double> (fftData[static_cast<size_t> (bin)]);
                const auto band = hz < 180.0 ? 0 : hz < 1400.0 ? 1 : 2;
                bandEnergy[static_cast<size_t> (band)] += std::log1p (magnitude);
            }
            double flux = 0.0;
            for (int band = 0; band < 3; ++band)
            {
                const auto change = std::max (0.0, bandEnergy[static_cast<size_t> (band)] - previousBands[static_cast<size_t> (band)]);
                flux += change * (band == 0 ? 1.15 : band == 1 ? 1.0 : 0.85);
                previousBands[static_cast<size_t> (band)] = bandEnergy[static_cast<size_t> (band)];
            }
            double positiveAttack = 0.0;
            for (int index = 1; index < fftSize; ++index)
                positiveAttack += std::max (0.0, static_cast<double> (samples[static_cast<size_t> (start + index)]) - samples[static_cast<size_t> (start + index - 1)]);
            tonalOnset.push_back (std::log1p (flux));
            onset.push_back (tonalOnset.back() + 1.25 * std::log1p (positiveAttack));

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
        const auto tonalOnsetMean = std::accumulate (tonalOnset.begin(), tonalOnset.end(), 0.0) / tonalOnset.size();
        for (auto& value : onset) value = std::max (0.0, value - onsetMean * 0.40);
        for (auto& value : tonalOnset) value = std::max (0.0, value - tonalOnsetMean * 0.40);
        result.onsetCoverage = static_cast<double> (std::count_if (onset.begin(), onset.end(), [] (double value) { return value > 0.0; })) / onset.size();
        if (result.onsetCoverage < 0.03)
        {
            result.warning = "No stable onset activity was found for tempo analysis.";
            appendInputQualityWarning();
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
            appendInputQualityWarning();
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
            appendInputQualityWarning();
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
        const auto transientThreshold = percentile (tonalOnset, 0.90);
        std::vector<std::array<double, 12>> tonalChromas;
        for (int frame = 0; frame < frameCount; frame += 4)
        {
            if (tonalOnset[static_cast<size_t> (frame)] > transientThreshold && tonalOnset[static_cast<size_t> (frame)] > 0.0) continue;
            const auto start = frame * hopSize;
            std::fill (fftData.begin(), fftData.end(), 0.0f);
            for (int index = 0; index < fftSize; ++index)
                fftData[static_cast<size_t> (index)] = samples[static_cast<size_t> (start + index)] * window[static_cast<size_t> (index)];
            fft.performFrequencyOnlyForwardTransform (fftData.data());
            const auto flatness = safeLogFlatness (fftData, lowBin, highBin);
            if (flatness > 0.68) continue;
            // Suppress transient/noise-like frames. Per-frame unit normalization makes the later robust
            // aggregate insensitive to loudness; frequency weighting prevents upper partials from dominating.
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
            appendInputQualityWarning();
            return result;
        }

        // Robust aggregation suppresses short arrangement changes and residual percussive frames.
        for (int pitchClass = 0; pitchClass < 12; ++pitchClass)
        {
            std::vector<double> values;
            values.reserve (tonalChromas.size());
            for (const auto& frameChroma : tonalChromas)
                values.push_back (frameChroma[static_cast<size_t> (pitchClass)]);
            result.chroma[static_cast<size_t> (pitchClass)] = trimmedMean (std::move (values));
        }
        const auto energy = std::accumulate (result.chroma.begin(), result.chroma.end(), 0.0);
        if (energy <= 1.0e-10)
        {
            result.harmonicContentSufficient = false;
            result.warning = "No reliable harmonic pitch-class energy was found.";
            appendInputQualityWarning();
            return result;
        }
        for (auto& value : result.chroma) value /= energy;
        double chromaEntropy = 0.0;
        for (const auto value : result.chroma)
            if (value > 1.0e-12) chromaEntropy -= value * std::log (value);
        const auto pitchClassConcentration = clamp01 (1.0 - chromaEntropy / std::log (12.0));
        const auto keys = scoreKeys (result.chroma);
        const auto krumhanslWinner = bestKeyForProfileFamily (result.chroma, krumhanslMajor, krumhanslMinor);
        const auto temperleyWinner = bestKeyForProfileFamily (result.chroma, temperleyMajor, temperleyMinor);
        result.profileDisagreement = krumhanslWinner.root != temperleyWinner.root || krumhanslWinner.mode != temperleyWinner.mode;
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
        // Stable harmony is valid evidence; tonal movement is retained implicitly through window agreement rather than required for validity.
        result.harmonicContentSufficient = result.tonalWindowAgreement >= 0.42
            && result.tonalClarity >= 0.12
            && pitchClassConcentration >= 0.12;
        result.relativeModeAmbiguous = keys.front().mode != keys[1].mode && clarity < 0.08;
        result.keyConfidence = clamp01 (0.48 * result.tonalClarity + 0.32 * result.tonalWindowAgreement + 0.12 * std::min (1.0, tonalChromas.size() / 24.0) + 0.08 * result.tuningConfidence);
        // Profile-family disagreement is reported for offline comparison; it is not itself a mathematical reason to erase an otherwise clear key.
        result.modeConfidence = clamp01 (clarity / 0.18);
        result.keyUncertain = ! result.harmonicContentSufficient || result.keyConfidence < 0.55 || result.relativeModeAmbiguous;
        result.keyValid = ! result.keyUncertain;
        if (result.keyValid)
        {
            result.keyRoot = keys.front().root;
            result.keyMode = keys.front().mode;
        }
        if (! result.keyValid)
            result.warning = result.harmonicContentSufficient ? "Key uncertainty: competing tonal candidates or unstable harmonic windows."
                : "Insufficient harmonic content for key detection.";
        else if (result.profileDisagreement)
            result.warning = "Key detected with profile-family disagreement; verify against independent offline references.";
        appendInputQualityWarning();
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
