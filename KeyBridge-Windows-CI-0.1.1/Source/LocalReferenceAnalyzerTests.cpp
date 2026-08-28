#include "LocalReferenceAnalyzer.h"

#include <cmath>
#include <iostream>

namespace
{
    bool check (bool condition, const char* name)
    {
        std::cout << name << ": " << (condition ? "PASS" : "FAIL") << "\n";
        return condition;
    }
}

int main()
{
    bool passed = true;

    const auto complete = tunerite::LocalReferenceAnalyzer::parseJsonResult (R"json(
        {"schema_version":1,
         "tempo":{"valid":true,"bpm":182.0,"confidence":0.80,"candidates":[{"bpm":182.0,"score":0.70},{"bpm":91.0,"score":0.20}]},
         "key":{"valid":true,"root":0,"mode":"minor","confidence":0.66,"candidates":[{"root":0,"mode":"minor","score":0.66}]}}
    )json");
    passed = check (complete.usableAudio && complete.tempoValid && complete.keyValid
                    && std::abs (complete.bpm - 182.0) < 1.0e-9 && complete.keyRoot == 0 && complete.keyMode == 1
                    && complete.tempoCandidates.size() == 2 && complete.keyCandidateScores.size() == 1,
                    "valid independent BPM/key JSON") && passed;

    const auto tempoOnly = tunerite::LocalReferenceAnalyzer::parseJsonResult (R"json(
        {"schema_version":1,
         "tempo":{"valid":true,"bpm":208.0,"confidence":0.77,"candidates":[{"bpm":208.0,"score":0.77}]},
         "key":{"valid":false,"root":null,"mode":"unknown","confidence":0.0,"candidates":[]}}
    )json");
    passed = check (tempoOnly.usableAudio && tempoOnly.tempoValid && ! tempoOnly.keyValid
                    && tempoOnly.keyUncertain && std::abs (tempoOnly.bpm - 208.0) < 1.0e-9,
                    "valid BPM with uncertain key remains independent") && passed;

    const auto keyOnly = tunerite::LocalReferenceAnalyzer::parseJsonResult (R"json(
        {"schema_version":1,
         "tempo":{"valid":false,"bpm":null,"confidence":0.0,"candidates":[]},
         "key":{"valid":true,"root":7,"mode":"major","confidence":0.61,"candidates":[{"root":7,"mode":"major","score":0.61}]}}
    )json");
    passed = check (keyOnly.usableAudio && ! keyOnly.tempoValid && keyOnly.keyValid
                    && keyOnly.keyRoot == 7 && keyOnly.keyMode == 0,
                    "valid key with uncertain BPM remains independent") && passed;

    const auto lowConfidence = tunerite::LocalReferenceAnalyzer::parseJsonResult (R"json(
        {"schema_version":1,
         "tempo":{"valid":true,"bpm":160.0,"confidence":0.04,"candidates":[{"bpm":160.0,"score":0.04}]},
         "key":{"valid":true,"root":8,"mode":"minor","confidence":0.03,"candidates":[{"root":8,"mode":"minor","score":0.03}]}}
    )json");
    passed = check (lowConfidence.tempoValid && lowConfidence.keyValid
                    && std::abs (lowConfidence.bpm - 160.0) < 1.0e-9
                    && lowConfidence.keyRoot == 8 && lowConfidence.keyMode == 1
                    && std::abs (lowConfidence.bpmConfidence - 0.04) < 1.0e-9
                    && std::abs (lowConfidence.keyConfidence - 0.03) < 1.0e-9,
                    "low-confidence valid estimates are still published") && passed;

    const auto malformed = tunerite::LocalReferenceAnalyzer::parseJsonResult ("{not-json}");
    passed = check (! malformed.usableAudio && ! malformed.tempoValid && ! malformed.keyValid
                    && malformed.warning.find ("invalid JSON schema") != std::string::npos,
                    "malformed JSON rejected") && passed;

    const auto invalidKey = tunerite::LocalReferenceAnalyzer::parseJsonResult (R"json(
        {"schema_version":1,
         "tempo":{"valid":true,"bpm":120.0,"confidence":0.70,"candidates":[]},
         "key":{"valid":true,"root":99,"mode":"major","confidence":0.70,"candidates":[]}}
    )json");
    passed = check (invalidKey.tempoValid && ! invalidKey.keyValid && invalidKey.keyRoot == -1
                    && invalidKey.warning.find ("invalid key") != std::string::npos,
                    "invalid key JSON never replaces valid BPM") && passed;

    return passed ? 0 : 1;
}
