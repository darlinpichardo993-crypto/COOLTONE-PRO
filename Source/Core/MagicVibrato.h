#pragma once
// COOLTONE by ORYMY - YMY RECORDS - "Magic Vibrato": rule-based decision of WHERE (and from when) vibrato belongs.
#include "NoteEditor.h"
#include "Params.h"
#include <vector>

namespace cooltone
{
enum class Intelligence { Minimal, Natural, Expressive, Professional, Dramatic };

struct MagicSettings
{
    Intelligence level = Intelligence::Natural;
    int style = 0;               // VibratoStyle; Run only targets the endings of runs / melismas
    double noteSens = 0.5, durSens = 0.5, stability = 0.5;   // 0..1
};
MagicSettings magicSettingsFromSettings (const Settings&);

struct MagicDecision { bool apply = false; double amountScale = 1.0, delayScale = 1.0, start = 0.0; };

// Measures an existing vibrato: rate (Hz) and peak depth (cents). False when the note has none.
bool measureNaturalVibrato (const EditNote&, const std::vector<PitchFrame>&, double hop, double* rateHz, double* depthCents);
// True when the recording already contains a vibrato (periodic 4-8 Hz pitch modulation > ~14 cents rms).
bool noteHasNaturalVibrato (const EditNote&, const std::vector<PitchFrame>&, double hop);
// Standard deviation (cents) of the note's pitch around its median, ignoring the note edges.
double notePitchSpread (const EditNote&, const std::vector<PitchFrame>&, double hop);
// Indices of notes that END a run / melisma (>=3 short, touching, stepwise notes).
std::vector<int> findRunEndings (const std::vector<EditNote>&);

std::vector<MagicDecision> analyseForVibrato (const std::vector<EditNote>&, const std::vector<PitchFrame>&, double hop, const MagicSettings&);

// Writes the decisions into the notes. selection empty = all notes. regenerate = new random seeds.
// Returns how many notes now carry vibrato.
int applyMagicVibrato (std::vector<EditNote>&, const std::vector<PitchFrame>&, double hop, const MagicSettings&,
                       const std::vector<int>& selection, bool regenerate, std::uint32_t seedBase);
} // namespace cooltone
