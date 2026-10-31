#pragma once
// COOLTONE by ORYMY - YMY RECORDS - Standard MIDI File export of the detected / edited notes.
// Only notes, durations, velocities and tempo are stored. Pitch curves and vibrato are NOT representable here.
#include "NoteEditor.h"
#include "Params.h"
#include <string>
#include <vector>

namespace cooltone
{
// MIDI pitch of a note as heard after editing: manual target / melisma steps are expanded to separate notes,
// automatic correction snaps to the scale; otherwise the nearest semitone of the detected pitch.
std::vector<unsigned char> buildMidiFile (const std::vector<EditNote>&, const Settings&, double bpm);
bool writeMidiFile (const std::string& path, const std::vector<EditNote>&, const Settings&, double bpm, std::string* error = nullptr);
} // namespace cooltone
