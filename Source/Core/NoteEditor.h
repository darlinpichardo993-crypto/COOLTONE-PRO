#pragma once
// COOLTONE by ORYMY - YMY RECORDS - editable note model (split, glue, delete, targets, per-note flags)
#include "NoteSegmenter.h"
#include <cstdint>
#include <string>
#include <vector>

namespace cooltone
{
struct MelismaStep { double startSec = 0, endSec = 0, midi = 0; };

struct NoteEdit
{
    bool hasTarget = false;          // user dragged the note to a new pitch
    double targetMidi = 0;
    bool deleted = false;            // the note's audio is muted
    bool vibrato = false;            // synthetic vibrato enabled on this note
    std::uint32_t vibSeed = 1;
    double vibAmountScale = 1.0, vibDelayScale = 1.0;
    double vibStart = 0.0, vibEnd = 1.0;          // part of the note that carries the vibrato (0..1)
    double vibSpeedScale = 1.0, vibDepthScale = 1.0; // per-note multipliers on top of the global controls
    std::vector<MelismaStep> melisma; // absolute times / MIDI pitches; empty = none
};

struct EditNote { Note note; NoteEdit edit; };

void refreshNote (EditNote&, const std::vector<PitchFrame>&, double hop);
std::vector<EditNote> makeEditNotes (const std::vector<Note>&);
bool splitNote (std::vector<EditNote>&, const std::vector<PitchFrame>&, double hop, std::size_t index, double timeSec);
bool glueNotes (std::vector<EditNote>&, const std::vector<PitchFrame>&, double hop, std::size_t index); // with the next note
int findNoteAt (const std::vector<EditNote>&, double timeSec);   // -1 if none

std::string serializeEdits (const std::vector<EditNote>&);
bool parseEdits (const std::string&, std::vector<EditNote>&, std::size_t numFrames, const std::vector<PitchFrame>&, double hop);
} // namespace cooltone
