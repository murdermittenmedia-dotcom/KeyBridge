#pragma once

#include <array>
#include <string>
#include <vector>

namespace tunerite
{
    struct TempoCandidate
    {
        double bpm = 0.0;
        double score = 0.0;
    };

    struct BeatAnalysisResult
    {
        bool usableAudio = false;
        bool bpmUncertain = true;
        bool keyUncertain = true;
        double bpm = 0.0;
        double alternativeBpm = 0.0;
        double halfTimeBpm = 0.0;
        double doubleTimeBpm = 0.0;
        double bpmConfidence = 0.0;
        int keyRoot = 0;
        int keyMode = 0; // 0 major, 1 minor
        double keyConfidence = 0.0;
        double modeConfidence = 0.0;
        double rms = 0.0;
        double peak = 0.0;
        double durationSeconds = 0.0;
        std::array<double, 12> chroma {};
        std::vector<TempoCandidate> tempoCandidates;
        std::vector<std::string> keyCandidates;
        std::string warning;
    };

    struct VocalAnalysisResult
    {
        bool usableAudio = false;
        bool uncertain = true;
        double rms = 0.0;
        double peak = 0.0;
        double voicedPercent = 0.0;
        double confidence = 0.0;
        double lowMidi = 0.0;
        double highMidi = 0.0;
        double p05Midi = 0.0;
        double p95Midi = 0.0;
        double averageMidi = 0.0;
        double pitchStabilityCents = 0.0;
        double sustainedPercent = 0.0;
        double noteChangeRate = 0.0;
        double vibratoDepthCents = 0.0;
        bool melodic = false;
        std::vector<double> midiContour;
        std::string warning;
    };

    class AnalysisCore
    {
    public:
        static BeatAnalysisResult analyzeBeat (const std::vector<float>& monoSamples, double sampleRate);
        static VocalAnalysisResult analyzeVocal (const std::vector<float>& monoSamples, double sampleRate);

        static std::vector<float> preprocessMono (const std::vector<float>& samples, double& rms, double& peak);
    };
}
