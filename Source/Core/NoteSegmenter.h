#pragma once
// COOLTONE by ORYMY - YMY RECORDS
// Turns a pitch track into discrete editable notes.
#include "PitchDetection.h"
#include <cstddef>
#include <vector>

namespace cooltone
{
struct SegmenterParams
{
    double splitSemitones = 0.70;  // deviation from the running median that starts a new note
    int splitConfirmFrames = 3;    // frames the new pitch must persist (rejects glitches)
    double minNoteSec = 0.06;      // shorter notes are discarded
    int smoothRadius = 2;          // median smoothing radius over voiced neighbours
    int maxGapFrames = 1;          // unvoiced frames tolerated inside a note
};

struct Note
{
    double startSec = 0, endSec = 0;
    double midiPitch = 0;          // median detected pitch (fractional MIDI)
    std::size_t firstFrame = 0, lastFrame = 0;
    double meanRms = 0;
};

std::vector<Note> segmentNotes (const std::vector<PitchFrame>& frames, double hopSeconds,
                                const SegmenterParams& = {});
} // namespace cooltone
