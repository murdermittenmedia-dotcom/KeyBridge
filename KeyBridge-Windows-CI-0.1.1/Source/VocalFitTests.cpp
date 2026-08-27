#include "VocalFit.h"

#include <iostream>

namespace
{
    bool expect (bool condition, const char* message)
    {
        if (! condition)
            std::cerr << "FAIL: " << message << '\n';
        return condition;
    }

    tunerite::VocalFitRequest validRequest()
    {
        tunerite::VocalFitRequest request;
        request.beatKeyValid = true;
        request.beatKeyRoot = 0;
        request.beatKeyMode = 1; // C minor
        request.beatTempoValid = true;
        request.beatBpm = 140.0;
        request.vocalValid = true;
        request.vocalMelodic = true;
        request.vocalLowMidi = 48.0;
        request.vocalHighMidi = 69.0;
        request.vocalAverageMidi = 60.0;
        request.vocalConfidence = 0.90;
        request.vocalSustainedPercent = 0.60;
        request.vocalNoteChangeRate = 2.0;
        request.voiceProfile = "Female hint";
        request.genre = "R&B";
        request.delivery = "Sung";
        request.mood = "Natural";
        return request;
    }
}

int main()
{
    auto ok = true;

    auto missingBeat = validRequest();
    missingBeat.beatKeyValid = false;
    const auto noBeat = tunerite::VocalFit::recommend (missingBeat);
    ok &= expect (! noBeat.ready && noBeat.requiresBeatKey && noBeat.status.find ("BEAT INPUT REQUIRED") != std::string::npos,
                  "Missing beat key must block Auto-Tune key/scale recommendations");

    auto missingVocal = validRequest();
    missingVocal.vocalValid = false;
    const auto noVocal = tunerite::VocalFit::recommend (missingVocal);
    ok &= expect (! noVocal.ready && ! noVocal.requiresBeatKey && noVocal.requiresVocal
                  && noVocal.keyRoot == 0 && noVocal.keyMode == 1
                  && noVocal.status.find ("VOCAL INPUT REQUIRED") != std::string::npos,
                  "Saved beat key must remain available while invalid vocal evidence blocks vocal-specific settings");

    const auto natural = tunerite::VocalFit::recommend (validRequest());
    ok &= expect (natural.ready && natural.keyRoot == 0 && natural.keyMode == 1 && natural.keyScale == "C Minor",
                  "Valid evidence must retain the measured C minor key exactly");
    ok &= expect (natural.enabledNotes[0] && natural.enabledNotes[2] && natural.enabledNotes[3]
                  && natural.enabledNotes[5] && natural.enabledNotes[7] && natural.enabledNotes[8] && natural.enabledNotes[10]
                  && ! natural.enabledNotes[1] && ! natural.enabledNotes[4],
                  "Minor scale note mask must be derived solely from the measured key/scale");

    auto aggressive = validRequest();
    aggressive.voiceProfile = "Male hint";
    aggressive.genre = "Trap";
    aggressive.delivery = "Rap";
    aggressive.mood = "Hard tune";
    const auto hardTune = tunerite::VocalFit::recommend (aggressive);
    ok &= expect (hardTune.ready && hardTune.keyRoot == natural.keyRoot && hardTune.keyMode == natural.keyMode,
                  "Preferences must never override the measured key or scale");
    ok &= expect (hardTune.classicMode && hardTune.retuneSpeedMs < natural.retuneSpeedMs
                  && hardTune.flexTune < natural.flexTune,
                  "Hard-tune rap preferences must change only creative starting settings");
    ok &= expect (hardTune.vocalRange.find ("Male profile hint") != std::string::npos,
                  "Voice profile must be labelled as a recommendation hint, not an inferred sex");

    std::cout << "vocal_fit_tests=" << (ok ? "PASS" : "FAIL") << '\n';
    return ok ? 0 : 1;
}
