#pragma once
// COOLTONE by ORYMY - YMY RECORDS - voice expression stage (breath/tone, dynamics, note attack/release)
#include "BreathProcessor.h"
#include "NoteEditor.h"
#include "Params.h"

namespace cooltone
{
ExpressionParams expressionFromSettings (const Settings&);
void applyVoiceExpression (std::vector<std::vector<float>>& ch, double sampleRate, const std::vector<PitchFrame>& frames, double hop,
                           const std::vector<double>& finalMidi, const std::vector<EditNote>& notes, const ExpressionParams&);
} // namespace cooltone
