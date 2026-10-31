#pragma once
// COOLTONE by ORYMY - YMY RECORDS
// Tonal / breath separation and shaping: Breath Amount, Airiness, Vocal Presence, Softness.
// Tonal part = pitch-synchronous comb estimate of the periodic voice; residual = what is left (aspiration, breath).
#include "PitchDetection.h"
#include <vector>

namespace cooltone
{
struct ExpressionParams
{
    double breath = 1.0;      // gain of the breath / aperiodic component (1 = untouched)
    double airDb = 0.0;       // high shelf on the breath component
    double presenceDb = 0.0;  // peaking EQ at 3 kHz on the tonal component
    double softness = 0.0;    // 0..1 : high-frequency roll-off of the tonal component (up to -8 dB shelf at 6 kHz)
    double dynamics = 0.0;    // -0.5..+0.5 : compress (<0) / expand (>0) around the typical level
    double attackSec = 0.0, releaseSec = 0.0;  // note attack / release shaping
    bool neutral() const { return breath == 1.0 && airDb == 0.0 && presenceDb == 0.0 && softness == 0.0 && dynamics == 0.0 && attackSec == 0.0 && releaseSec == 0.0; }
    bool breathStageNeutral() const { return breath == 1.0 && airDb == 0.0 && presenceDb == 0.0 && softness == 0.0; }
};

// finalMidi: pitch of the OUTPUT per analysis frame (0 = unvoiced) - sets the comb delay.
void processBreathAndTone (std::vector<std::vector<float>>& ch, double sampleRate, const std::vector<PitchFrame>& frames,
                           double hop, const std::vector<double>& finalMidi, const ExpressionParams&);
} // namespace cooltone
