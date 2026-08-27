#include "analysis/bpm_analyzer.h"
#include "analysis/key_analyzer.h"

#include <array>
#include <vector>

int main()
{
    const std::vector<float> onsetStrength { 0.0f, 0.8f, 0.1f, 0.7f, 0.1f, 0.8f, 0.1f, 0.7f, 0.1f, 0.8f };
    sonare::BpmConfig bpmConfig;
    bpmConfig.bpm_min = 30.0f;
    bpmConfig.bpm_max = 240.0f;
    const sonare::BpmAnalyzer bpmAnalyzer (onsetStrength, 44100, 512, bpmConfig);

    const std::array<float, 12> cMajorChroma { 1.0f, 0.1f, 0.2f, 0.1f, 0.8f, 0.4f,
                                                0.1f, 0.9f, 0.1f, 0.2f, 0.1f, 0.5f };
    sonare::KeyConfig keyConfig;
    keyConfig.genre_hint = "pop";
    const sonare::KeyAnalyzer keyAnalyzer (cMajorChroma, keyConfig);

    return bpmAnalyzer.candidates().empty() || keyAnalyzer.candidates().empty() ? 1 : 0;
}
