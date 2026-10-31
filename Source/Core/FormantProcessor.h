#pragma once
// COOLTONE by ORYMY - YMY RECORDS
// Time-domain pitch-synchronous overlap-add (TD-PSOLA) resynthesis with independent formant control.
// Pitch changes keep the duration and, with formant factor 1, keep the vocal tract (formants) intact.
#include "PitchDetection.h"
#include <vector>

namespace cooltone
{
// in/out: channels. ratio: pitch ratio per analysis frame. formant: grain-resampling factor per frame
// (1 = formants untouched, >1 shifts them up). Unvoiced audio and untouched regions are copied bit-exactly.
void psolaProcess (const std::vector<std::vector<float>>& in, double sampleRate,
                   const std::vector<PitchFrame>& frames, double hopSeconds,
                   const std::vector<double>& ratio, const std::vector<double>& formant,
                   std::vector<std::vector<float>>& out);
} // namespace cooltone
