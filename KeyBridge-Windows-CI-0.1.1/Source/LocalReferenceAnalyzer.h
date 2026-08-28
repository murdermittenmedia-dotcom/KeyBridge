#pragma once

#include "AnalysisCore.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <vector>

namespace tunerite
{
    // Non-commercial beta bridge for a locally bundled analyzer process. It is called
    // solely by the existing background worker after capture finalization; processBlock
    // must never invoke it. The process receives no host metadata or network access.
    class LocalReferenceAnalyzer final
    {
    public:
        static BeatAnalysisResult analyzeFinalizedCapture (const std::vector<float>& monoSamples,
                                                           double sampleRate,
                                                           std::uint64_t generation);

        // Exposed for deterministic schema tests. Invalid/missing fields never create
        // a valid BPM or key and always return a concise diagnostic warning.
        static BeatAnalysisResult parseJsonResult (const juce::String& jsonText);

    private:
        static juce::File resolveAnalyzerExecutable();
    };
}
