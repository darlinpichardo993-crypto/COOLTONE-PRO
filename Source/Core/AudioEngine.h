#pragma once
// COOLTONE by ORYMY - YMY RECORDS
// AudioEngine: analysis of a recording, full offline render (what you hear and what you export are the same buffer),
// voice report (AI Voice Analyzer), key detection and export helpers.
#include "FormantProcessor.h"
#include "MagicVibrato.h"
#include "NoteEditor.h"
#include "Params.h"
#include "PitchCorrection.h"
#include "ScaleEngine.h"
#include "VoiceExpression.h"
#include "WavIO.h"
#include <vector>

namespace cooltone
{
struct AudioClip
{
    int sampleRate = 44100;
    std::vector<std::vector<float>> ch;     // 1 or 2 channels
    std::size_t numFrames() const { return ch.empty() ? 0 : ch[0].size(); }
    double durationSec() const { return sampleRate > 0 ? static_cast<double> (numFrames()) / sampleRate : 0.0; }
};

struct Analysis
{
    std::vector<PitchFrame> frames;
    double hop = 0.01;                      // actual hop in seconds (hopSamples / sampleRate)
    std::vector<EditNote> notes;
    std::vector<float> peakMin, peakMax;    // waveform overview, one pair per peakBlock samples
    int peakBlock = 256;
};
Analysis analyseClip (const AudioClip&);

struct RenderOutput
{
    int sampleRate = 44100;
    std::vector<std::vector<float>> ch;
    std::vector<double> finalMidi;          // output pitch per analysis frame (0 = unvoiced)
};
// The complete chain: Auto Tune / manual edits -> Magic Vibrato -> deletions -> voice expression -> mix & gain.
RenderOutput renderProject (const AudioClip&, const Analysis&, const std::vector<EditNote>& notes, const Settings&);

KeyEstimate detectKeyFromNotes (const std::vector<EditNote>&, double centerHz = 440.0);

struct VoiceReport
{
    double durationSec = 0, voicedFraction = 0;
    double f0MinHz = 0, f0MedianHz = 0, f0MaxHz = 0;
    KeyEstimate key; const char* keyName = "C"; bool keyMinor = false;
    int numNotes = 0, numSustained = 0, numRuns = 0, numBreaths = 0, notesWithVibrato = 0;
    double avgAbsDeviationCents = 0;        // mean |distance to the nearest semitone|, duration weighted
    double avgVibratoRateHz = 0, avgVibratoDepthCents = 0;
};
VoiceReport buildVoiceReport (const AudioClip&, const Analysis&, const std::vector<EditNote>&);
double noteDeviationCents (const EditNote&, double centerHz = 440.0);   // signed, to the nearest semitone

struct ExportOptions
{
    WavFormat format = WavFormat::PCM24;
    bool selectionOnly = false;
    double selStartSec = 0, selEndSec = 0;  // outside the selection the file is silent: start position & duration are preserved
};
WavData buildExport (const std::vector<std::vector<float>>& ch, int sampleRate, const ExportOptions&);
} // namespace cooltone
