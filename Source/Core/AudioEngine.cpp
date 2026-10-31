#include "AudioEngine.h"
#include "DSPUtilities.h"
#include <algorithm>
#include <cmath>

namespace cooltone
{
Analysis analyseClip (const AudioClip& clip)
{
    Analysis a;
    const std::size_t n = clip.numFrames();
    std::vector<float> mono (n, 0.f);
    for (std::size_t i = 0; i < n; ++i) { double s = 0; for (const auto& c : clip.ch) s += c[i]; mono[i] = static_cast<float> (s / static_cast<double> (clip.ch.size())); }

    YinParams p; p.sampleRate = clip.sampleRate;
    a.frames = PitchDetector::analyse (mono.data(), mono.size(), p);
    a.hop = static_cast<double> (PitchDetector::hopSamples (p)) / clip.sampleRate;
    if (! a.frames.empty()) a.notes = makeEditNotes (segmentNotes (a.frames, a.hop));

    const std::size_t blocks = (n + static_cast<std::size_t> (a.peakBlock) - 1) / static_cast<std::size_t> (a.peakBlock);
    a.peakMin.assign (blocks, 0.f); a.peakMax.assign (blocks, 0.f);
    for (std::size_t b = 0; b < blocks; ++b)
    {
        float lo = 1e9f, hi = -1e9f;
        for (std::size_t i = b * static_cast<std::size_t> (a.peakBlock); i < std::min (n, (b + 1) * static_cast<std::size_t> (a.peakBlock)); ++i) { lo = std::min (lo, mono[i]); hi = std::max (hi, mono[i]); }
        a.peakMin[b] = lo > hi ? 0.f : lo; a.peakMax[b] = hi < lo ? 0.f : hi;
    }
    return a;
}

RenderOutput renderProject (const AudioClip& clip, const Analysis& an, const std::vector<EditNote>& notes, const Settings& s)
{
    RenderOutput out;
    out.sampleRate = clip.sampleRate;
    if (clip.ch.empty() || an.frames.empty()) { out.ch = clip.ch; return out; }

    const CorrectionResult corr = buildCorrection (an.frames, an.hop, notes, s);
    psolaProcess (clip.ch, clip.sampleRate, an.frames, an.hop, corr.ratio, corr.formant, out.ch);
    out.finalMidi = corr.finalMidi;

    const std::size_t nS = out.ch[0].size();
    const long fade = std::max<long> (2, std::lround (0.005 * clip.sampleRate));
    for (const auto& en : notes)       // deleted notes are muted with short fades
    {
        if (! en.edit.deleted) continue;
        const long a = std::max<long> (0, std::lround (en.note.startSec * clip.sampleRate));
        const long b = std::min<long> (static_cast<long> (nS) - 1, std::lround (en.note.endSec * clip.sampleRate));
        for (long n = a - fade; n <= b + fade; ++n)
        {
            if (n < 0 || n >= static_cast<long> (nS)) continue;
            double g = 0.0;
            if (n < a) g = 1.0 - static_cast<double> (n - (a - fade)) / static_cast<double> (fade);
            else if (n > b) g = static_cast<double> (n - b) / static_cast<double> (fade);
            for (auto& c : out.ch) c[static_cast<std::size_t> (n)] = static_cast<float> (c[static_cast<std::size_t> (n)] * clampValue (g, 0.0, 1.0));
        }
    }

    applyVoiceExpression (out.ch, clip.sampleRate, an.frames, an.hop, corr.finalMidi, notes, expressionFromSettings (s));

    const double mix = s.get (pMix) / 100.0, gain = std::pow (10.0, s.get (pOutGain) / 20.0);
    if (mix != 1.0 || gain != 1.0)
        for (std::size_t c = 0; c < out.ch.size(); ++c)
            for (std::size_t n = 0; n < nS; ++n)
                out.ch[c][n] = static_cast<float> ((mix * out.ch[c][n] + (1.0 - mix) * clip.ch[c][n]) * gain);
    return out;
}

KeyEstimate detectKeyFromNotes (const std::vector<EditNote>& notes, double centerHz)
{
    std::array<double, 12> h {};
    const double c0 = 12.0 * std::log2 (centerHz / 440.0);
    for (const auto& n : notes)
    {
        if (n.edit.deleted) continue;
        const int pc = ((static_cast<int> (std::lround (n.note.midiPitch - c0)) % 12) + 12) % 12;
        h[static_cast<std::size_t> (pc)] += n.note.endSec - n.note.startSec;
    }
    return ScaleEngine::detectKey (h);
}

double noteDeviationCents (const EditNote& en, double centerHz)
{
    const double c0 = 12.0 * std::log2 (centerHz / 440.0);
    const double m = en.note.midiPitch - c0;
    return (m - std::round (m)) * 100.0;
}

VoiceReport buildVoiceReport (const AudioClip& clip, const Analysis& an, const std::vector<EditNote>& notes)
{
    VoiceReport r;
    r.durationSec = clip.durationSec();
    std::vector<double> f0; std::vector<double> vr;
    for (const auto& f : an.frames) if (f.voiced) { f0.push_back (f.f0Hz); vr.push_back (f.rms); }
    r.voicedFraction = an.frames.empty() ? 0.0 : static_cast<double> (f0.size()) / static_cast<double> (an.frames.size());
    if (! f0.empty()) { std::sort (f0.begin(), f0.end()); r.f0MinHz = f0.front(); r.f0MaxHz = f0.back(); r.f0MedianHz = f0[f0.size() / 2]; }
    r.numNotes = static_cast<int> (notes.size());
    r.key = detectKeyFromNotes (notes);
    r.keyName = ScaleEngine::keyName (r.key.root); r.keyMinor = r.key.minor;

    double wsum = 0, dsum = 0, rsum = 0, depsum = 0; int vcount = 0;
    for (const auto& n : notes)
    {
        const double d = n.note.endSec - n.note.startSec;
        wsum += d; dsum += std::abs (noteDeviationCents (n)) * d;
        if (d >= 1.0) ++r.numSustained;
        double rate = 0, dep = 0;
        if (measureNaturalVibrato (n, an.frames, an.hop, &rate, &dep)) { ++r.notesWithVibrato; rsum += rate; depsum += dep; ++vcount; }
    }
    r.avgAbsDeviationCents = wsum > 0 ? dsum / wsum : 0.0;
    if (vcount) { r.avgVibratoRateHz = rsum / vcount; r.avgVibratoDepthCents = depsum / vcount; }
    r.numRuns = static_cast<int> (findRunEndings (notes).size());

    if (! vr.empty())   // breaths: quiet, unvoiced stretches of >= 80 ms
    {
        std::sort (vr.begin(), vr.end());
        const double med = vr[vr.size() / 2];
        std::size_t run = 0;
        for (std::size_t i = 0; i <= an.frames.size(); ++i)
        {
            const bool b = i < an.frames.size() && ! an.frames[i].voiced && an.frames[i].rms > 0.0005 && an.frames[i].rms < 0.25 * med;
            if (b) ++run; else { if (static_cast<double> (run) * an.hop >= 0.08) ++r.numBreaths; run = 0; }
        }
    }
    return r;
}

WavData buildExport (const std::vector<std::vector<float>>& ch, int sr, const ExportOptions& o)
{
    WavData w; w.sampleRate = sr; w.format = o.format; w.channels = static_cast<int> (std::max<std::size_t> (1, ch.size()));
    const std::size_t n = ch.empty() ? 0 : ch[0].size();
    w.samples.assign (n * static_cast<std::size_t> (w.channels), 0.f);
    std::size_t a = 0, b = n;
    if (o.selectionOnly) { a = static_cast<std::size_t> (std::max (0.0, o.selStartSec * sr)); b = std::min (n, static_cast<std::size_t> (std::max (0.0, o.selEndSec * sr))); }
    const std::size_t f = static_cast<std::size_t> (0.002 * sr);
    for (std::size_t i = a; i < b; ++i)
    {
        double g = 1.0;
        if (o.selectionOnly && f > 0) { if (i - a < f) g = static_cast<double> (i - a) / static_cast<double> (f); if (b - i < f) g = std::min (g, static_cast<double> (b - i) / static_cast<double> (f)); }
        for (std::size_t c = 0; c < ch.size(); ++c) w.samples[i * static_cast<std::size_t> (w.channels) + c] = static_cast<float> (ch[c][i] * g);
    }
    return w;
}
} // namespace cooltone
