#pragma once
// COOLTONE by ORYMY - YMY RECORDS
// Builds the per-frame pitch ratio / formant factor from notes + edits + settings:
// scale snapping, retune speed, humanize, drift, transitions, melismas, vibrato, transposition.
#include "NoteEditor.h"
#include "Params.h"
#include "ScaleEngine.h"
#include <vector>

namespace cooltone
{
struct CorrectionResult
{
    std::vector<double> origMidi;   // detected pitch per frame (0 = unvoiced)
    std::vector<double> finalMidi;  // pitch the output will have (0 = unvoiced)
    std::vector<double> ratio;      // pitch-shift ratio per frame (1 = untouched)
    std::vector<double> formant;    // grain resampling factor per frame (1 = untouched)
    std::vector<int> noteIndex;     // owning note or -1
};

// Nearest allowed note (from the 12 note switches) honouring the Pitch Center tuning.
double snapToScale (double midi, const Settings&);
// The pitch a note aims for before humanize/drift (manual target, or snapped, or its own median).
double noteBaseTarget (const EditNote&, const Settings&);

CorrectionResult buildCorrection (const std::vector<PitchFrame>&, double hop, const std::vector<EditNote>&, const Settings&);
} // namespace cooltone
