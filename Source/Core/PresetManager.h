#pragma once
// COOLTONE by ORYMY - YMY RECORDS - modes, factory presets (with categories), settings (de)serialisation
#include "Params.h"
#include <string>

namespace cooltone
{
void applyCorrectionMode (Settings&, int mode);      // Natural / Modern Pop / Hard Tune / Robotic / Vintage / Transparent
void applyVibratoStyle (Settings&, int style);       // default controls of each of the 8 vibrato styles
void applyScaleToNoteParams (Settings&);             // writes pNote0..11 from key + scale (no-op for Custom)
void resetToDefaults (Settings&, bool keepKeyScaleAndOutput = true);

int stylePresetCount();
const char* stylePresetName (int index);
int stylePresetCategory (int index);                 // index into presetCategoryName()
const char* presetCategoryName (int category);       // Artist Styles, Bachata, R&B, Pop, Emotional, Natural, Experimental
int presetCategoryCount();
void applyStylePreset (Settings&, int index);        // original processing configurations, not voice clones

std::string serializeSettings (const Settings&);
bool parseSettings (const std::string&, Settings&);  // unknown keys ignored, missing keys keep their value
} // namespace cooltone
