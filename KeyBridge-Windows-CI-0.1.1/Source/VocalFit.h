#pragma once

#include <array>
#include <string>

namespace tunerite
{
    struct VocalFitRequest
    {
        // Beat evidence is authoritative for key and scale. Preferences never modify it.
        bool beatKeyValid = false;
        int beatKeyRoot = -1;
        int beatKeyMode = -1; // 0 major, 1 minor
        bool beatTempoValid = false;
        double beatBpm = 0.0;

        // Vocal evidence is authoritative for range-sensitive recommendations.
        bool vocalValid = false;
        bool vocalMelodic = false;
        double vocalLowMidi = 0.0;
        double vocalHighMidi = 0.0;
        double vocalAverageMidi = 0.0;
        double vocalConfidence = 0.0;
        double vocalSustainedPercent = 0.0;
        double vocalNoteChangeRate = 0.0;

        // User preferences are creative controls only. They never enter audio analysis.
        std::string voiceProfile; // Auto range, Male hint, Female hint, Custom
        std::string genre;
        std::string delivery;
        std::string mood;
    };

    struct VocalFitRecommendation
    {
        bool ready = false;
        bool requiresBeatKey = true;
        bool requiresVocal = true;
        int keyRoot = -1;
        int keyMode = -1;
        std::array<bool, 12> enabledNotes {};
        int retuneSpeedMs = 0;
        int humanize = 0;
        int flexTune = 0;
        bool classicMode = false;
        bool highQuality = false;
        std::string keyScale;
        std::string vocalRange;
        std::string processingMode;
        std::string status;
        std::string rationale;
    };

    class VocalFit
    {
    public:
        static VocalFitRecommendation recommend (const VocalFitRequest& request);
    };
}
