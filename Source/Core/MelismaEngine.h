#pragma once
// COOLTONE by ORYMY - YMY RECORDS - scale-aware melisma / vocal run generator
#include "NoteEditor.h"
#include "Params.h"
#include <cstdint>
#include <vector>

namespace cooltone
{
struct MelismaParams
{
    int mode = 0;            // 0 Asc, 1 Desc, 2 Wave, 3 Soul Run, 4 R&B Run, 5 Latin Run, 6 Custom
    int numNotes = 4;        // total steps, the last one sustains until the end of the note
    double lengthFrac = 0.6; // fraction of the note that the run occupies
    int startDegree = 0, endDegree = 3;   // scale degrees relative to the note's own pitch
    int rhythm = 0;          // 0 even, 1 swing, 2 accelerating, 3 decelerating
    double humanize = 0.2;
    bool scaleLock = true;
};

MelismaParams melismaParamsFromSettings (const Settings&);

// baseMidi = the pitch the note would have without melisma. mask = allowed pitch classes (bit 0 = C).
std::vector<MelismaStep> generateMelisma (double noteStart, double noteEnd, double baseMidi, std::uint16_t mask,
                                          const MelismaParams&, std::uint32_t seed);
} // namespace cooltone
