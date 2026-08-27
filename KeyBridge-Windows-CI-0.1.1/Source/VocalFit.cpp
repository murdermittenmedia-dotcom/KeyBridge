#include "VocalFit.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    constexpr std::array<const char*, 12> noteNames { { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" } };
    constexpr std::array<int, 7> majorDegrees { { 0, 2, 4, 5, 7, 9, 11 } };
    constexpr std::array<int, 7> minorDegrees { { 0, 2, 3, 5, 7, 8, 10 } };

    int clampSetting (int value) { return std::clamp (value, 0, 100); }

    std::string midiName (double midi)
    {
        if (midi <= 0.0) return "Measured range unavailable";
        const auto rounded = static_cast<int> (std::round (midi));
        const auto note = ((rounded % 12) + 12) % 12;
        return std::string (noteNames[static_cast<size_t> (note)]) + std::to_string (rounded / 12 - 1);
    }
}

namespace tunerite
{
    VocalFitRecommendation VocalFit::recommend (const VocalFitRequest& request)
    {
        VocalFitRecommendation result;
        result.requiresBeatKey = ! request.beatKeyValid || request.beatKeyRoot < 0 || request.beatKeyRoot >= 12
            || (request.beatKeyMode != 0 && request.beatKeyMode != 1);
        result.requiresVocal = ! request.vocalValid || request.vocalConfidence < 0.55;

        if (result.requiresBeatKey)
        {
            result.status = "BEAT INPUT REQUIRED FOR AUTO-TUNE KEY AND SCALE";
            result.rationale = "TuneRite will not infer an Auto-Tune key or scale when the saved beat key is uncertain.";
            return result;
        }
        if (result.requiresVocal)
        {
            result.keyRoot = request.beatKeyRoot;
            result.keyMode = request.beatKeyMode;
            result.keyScale = std::string (noteNames[static_cast<size_t> (request.beatKeyRoot)])
                + (request.beatKeyMode == 0 ? " Major" : " Minor");
            result.status = "VOCAL INPUT REQUIRED FOR VOCAL-SPECIFIC AUTO-TUNE RECOMMENDATIONS";
            result.rationale = "Saved beat key/scale is available, but TuneRite needs a valid Vocal Only capture before recommending vocal-specific settings.";
            return result;
        }

        result.ready = true;
        result.requiresBeatKey = false;
        result.requiresVocal = false;
        result.keyRoot = request.beatKeyRoot;
        result.keyMode = request.beatKeyMode;
        result.keyScale = std::string (noteNames[static_cast<size_t> (request.beatKeyRoot)])
            + (request.beatKeyMode == 0 ? " Major" : " Minor");
        const auto& degrees = request.beatKeyMode == 0 ? majorDegrees : minorDegrees;
        for (const auto degree : degrees)
            result.enabledNotes[static_cast<size_t> ((request.beatKeyRoot + degree) % 12)] = true;

        const bool hardTune = request.mood == "Hard tune";
        const bool rapLike = request.delivery == "Rap" || request.delivery == "Spoken"
            || request.genre == "Rap" || request.genre == "Trap" || request.genre == "Hip-hop";
        const bool expressive = request.delivery == "Sung" || request.delivery == "Melodic"
            || request.genre == "R&B" || request.genre == "Soul" || request.genre == "Gospel" || request.genre == "Pop";
        const bool aggressive = request.mood == "Aggressive" || request.mood == "Energetic";
        const bool dark = request.mood == "Dark";

        result.retuneSpeedMs = hardTune ? 5 : (rapLike ? 16 : (expressive || request.vocalMelodic ? 35 : 24));
        result.humanize = hardTune ? 6 : (expressive ? 34 : (rapLike ? 12 : 22));
        result.flexTune = hardTune ? 6 : (expressive ? 40 : (rapLike ? 18 : 28));
        if (aggressive)
        {
            result.retuneSpeedMs -= hardTune ? 0 : 5;
            result.flexTune -= 6;
        }
        if (dark)
        {
            result.retuneSpeedMs -= hardTune ? 0 : 3;
            result.humanize -= 4;
        }
        if (request.vocalSustainedPercent >= 0.55)
            result.humanize += 8;
        if (request.vocalNoteChangeRate >= 4.0)
            result.retuneSpeedMs -= 4;

        result.retuneSpeedMs = std::clamp (result.retuneSpeedMs, 0, 100);
        result.humanize = clampSetting (result.humanize);
        result.flexTune = clampSetting (result.flexTune);
        result.classicMode = hardTune;
        result.highQuality = request.delivery == "Sung" || request.delivery == "Melodic" || expressive;
        result.processingMode = result.highQuality ? "HQ processing" : "Low-latency processing";

        if (request.voiceProfile == "Male hint")
            result.vocalRange = "Male profile hint; measured range " + midiName (request.vocalLowMidi) + " to " + midiName (request.vocalHighMidi);
        else if (request.voiceProfile == "Female hint")
            result.vocalRange = "Female profile hint; measured range " + midiName (request.vocalLowMidi) + " to " + midiName (request.vocalHighMidi);
        else if (request.voiceProfile == "Custom")
            result.vocalRange = "Custom profile; measured range " + midiName (request.vocalLowMidi) + " to " + midiName (request.vocalHighMidi);
        else
            result.vocalRange = "Measured range " + midiName (request.vocalLowMidi) + " to " + midiName (request.vocalHighMidi);

        result.status = "AUTO-TUNE STARTING POINT READY";
        result.rationale = "Measured beat key/scale and saved vocal evidence determine the valid recommendation. Voice profile, genre, delivery, and mood only shape the creative starting values.";
        return result;
    }
}
