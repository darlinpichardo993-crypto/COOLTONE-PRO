#include "MagicVibrato.h"
#include "DSPUtilities.h"
#include <algorithm>
#include <cmath>

namespace cooltone
{
MagicSettings magicSettingsFromSettings (const Settings& s)
{
    MagicSettings m;
    m.level = static_cast<Intelligence> (static_cast<int> (s.get (pVibIntel)));
    m.style = static_cast<int> (s.get (pVibStyle));
    m.noteSens = s.get (pVibNoteSens) / 100.0; m.durSens = s.get (pVibDurSens) / 100.0; m.stability = s.get (pVibStability) / 100.0;
    return m;
}

bool measureNaturalVibrato (const EditNote& en, const std::vector<PitchFrame>& fr, double hop, double* rateHz, double* depthCents)
{
    std::vector<double> dev;
    for (std::size_t k = en.note.firstFrame; k <= en.note.lastFrame; ++k)
        if (fr[k].voiced) dev.push_back ((fr[k].midi - en.note.midiPitch) * 100.0);
    if (dev.size() < static_cast<std::size_t> (0.4 / hop)) return false;
    const std::size_t w = std::max<std::size_t> (3, static_cast<std::size_t> (0.3 / hop));
    std::vector<double> hp (dev.size());
    for (std::size_t i = 0; i < dev.size(); ++i)
    {
        const std::size_t a = i > w / 2 ? i - w / 2 : 0, b = std::min (dev.size() - 1, i + w / 2);
        double m = 0; for (std::size_t k = a; k <= b; ++k) m += dev[k];
        hp[i] = dev[i] - m / static_cast<double> (b - a + 1);
    }
    double e = 0; for (double v : hp) e += v * v;
    const double rms = std::sqrt (e / static_cast<double> (hp.size()));
    if (rms < 14.0) return false;
    int zc = 0; for (std::size_t i = 1; i < hp.size(); ++i) if ((hp[i - 1] < 0) != (hp[i] < 0)) ++zc;
    const double rate = 0.5 * zc / (static_cast<double> (hp.size()) * hop);
    if (rate < 4.0 || rate > 8.0) return false;
    if (rateHz) *rateHz = rate;
    if (depthCents) *depthCents = rms * std::sqrt (2.0);
    return true;
}

bool noteHasNaturalVibrato (const EditNote& en, const std::vector<PitchFrame>& fr, double hop)
{
    return measureNaturalVibrato (en, fr, hop, nullptr, nullptr);
}

namespace
{
bool noteHasUsableVoicing (const EditNote& en, const std::vector<PitchFrame>& fr, double hop)
{
    if (fr.empty() || ! std::isfinite (hop) || hop <= 0.0) return false;
    const std::size_t first = std::min (en.note.firstFrame, fr.size());
    const std::size_t last = std::min (en.note.lastFrame, fr.size() - 1);
    if (first > last) return false;
    std::size_t voiced = 0;
    double confidence = 0.0;
    for (std::size_t i = first; i <= last; ++i)
        if (fr[i].voiced && std::isfinite (fr[i].midi))
        { ++voiced; confidence += clampValue (static_cast<double> (fr[i].confidence), 0.0, 1.0); }
    const std::size_t total = last - first + 1;
    const std::size_t minFrames = static_cast<std::size_t> (std::ceil (0.12 / hop));
    return voiced >= std::max<std::size_t> (3, minFrames)
        && static_cast<double> (voiced) / static_cast<double> (total) >= 0.55
        && confidence / static_cast<double> (voiced) >= 0.25;
}
} // namespace

double notePitchSpread (const EditNote& en, const std::vector<PitchFrame>& fr, double hop)
{
    const std::size_t skip = static_cast<std::size_t> (0.03 / hop) + 1;
    const std::size_t a = en.note.firstFrame + skip, b = en.note.lastFrame > skip ? en.note.lastFrame - skip : 0;
    if (b <= a) return 0.0;
    double e = 0; std::size_t n = 0;
    for (std::size_t k = a; k <= b; ++k) if (fr[k].voiced) { const double d = (fr[k].midi - en.note.midiPitch) * 100.0; e += d * d; ++n; }
    return n ? std::sqrt (e / static_cast<double> (n)) : 0.0;
}

std::vector<int> findRunEndings (const std::vector<EditNote>& notes)
{
    std::vector<int> ends;
    const std::size_t n = notes.size();
    auto isShort = [&] (std::size_t i) { return (notes[i].note.endSec - notes[i].note.startSec) < 0.28; };
    auto linked = [&] (std::size_t i) // note i flows into note i+1 stepwise
    {
        if (i + 1 >= n) return false;
        const double gap = notes[i + 1].note.startSec - notes[i].note.endSec;
        return gap < 0.06 && std::abs (notes[i + 1].note.midiPitch - notes[i].note.midiPitch) <= 4.5;
    };
    for (std::size_t i = 0; i < n;)
    {
        std::size_t j = i;
        while (j < n && isShort (j) && linked (j) && isShort (j + 1 < n ? j + 1 : j)) ++j;   // chain of short linked notes i..j
        const std::size_t chain = j - i + 1;
        if (chain >= 3 && isShort (i))
        {
            // the ending is the note that closes the run: the next note if it is a longer one, else the last short note
            if (j + 1 < n && linked (j) && ! isShort (j + 1)) ends.push_back (static_cast<int> (j + 1));
            else if (notes[j].note.endSec - notes[j].note.startSec >= 0.12) ends.push_back (static_cast<int> (j));
            i = j + 1;
        }
        else ++i;
    }
    return ends;
}

std::vector<MagicDecision> analyseForVibrato (const std::vector<EditNote>& notes, const std::vector<PitchFrame>& fr, double hop, const MagicSettings& ms)
{
    std::vector<MagicDecision> out (notes.size());
    if (notes.empty()) return out;
    std::vector<double> rmsv; for (const auto& n : notes) rmsv.push_back (n.note.meanRms);
    std::sort (rmsv.begin(), rmsv.end());
    const double medRms = rmsv[rmsv.size() / 2];

    double minDur = 0.6, strictNonEnd = 1.0;
    switch (ms.level)
    {
        case Intelligence::Minimal:      minDur = 0.90; strictNonEnd = 1.5; break;
        case Intelligence::Natural:      minDur = 0.60; strictNonEnd = 1.2; break;
        case Intelligence::Expressive:   minDur = 0.45; strictNonEnd = 1.0; break;
        case Intelligence::Professional: minDur = 0.50; strictNonEnd = 1.5; break;
        case Intelligence::Dramatic:     minDur = 0.30; strictNonEnd = 1.0; break;
    }
    minDur *= 1.5 - ms.durSens;                       // 0.5 -> x1, 1.0 -> x0.5, 0 -> x1.5
    const double maxSpread = 80.0 - 60.0 * ms.stability;   // stability 1 -> only very steady notes (<=20 cents)
    const double softThresh = 0.5 + 0.4 * ms.noteSens;

    const std::vector<int> runEnds = findRunEndings (notes);

    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        const EditNote& n = notes[i];
        MagicDecision d;
        const double dur = n.note.endSec - n.note.startSec;
        if (n.edit.deleted || ! noteHasUsableVoicing (n, fr, hop) || noteHasNaturalVibrato (n, fr, hop) || notePitchSpread (n, fr, hop) > maxSpread) { out[i] = d; continue; }

        if (ms.style == 1)   // RUN: only the endings of runs / melismas, entering late
        {
            if (std::find (runEnds.begin(), runEnds.end(), static_cast<int> (i)) != runEnds.end() && dur >= 0.12)
            { d.apply = true; d.start = 0.4; d.amountScale = 1.0; d.delayScale = 0.0; }
            out[i] = d; continue;
        }

        const bool last = (i + 1 == notes.size());
        const double gapNext = last ? 1e9 : notes[i + 1].note.startSec - n.note.endSec;
        const bool phraseEnd = gapNext > 0.12;
        double need = minDur * (phraseEnd ? 0.7 : strictNonEnd);
        if (! last && gapNext < 0.03 && std::abs (notes[i + 1].note.midiPitch - n.note.midiPitch) > 3.0) need *= 1.3;
        if (dur < need) { out[i] = d; continue; }
        d.apply = true;
        d.amountScale = clampValue ((dur - need) / 1.0 + 0.6, 0.6, 1.3) * (phraseEnd ? 1.15 : 1.0);
        if (n.note.meanRms < softThresh * medRms) d.amountScale *= 0.6;
        if (ms.level == Intelligence::Minimal) d.amountScale *= 0.7;
        if (ms.level == Intelligence::Dramatic) d.amountScale *= 1.25;
        d.delayScale = phraseEnd ? 0.7 : 1.0;
        out[i] = d;
    }
    return out;
}

int applyMagicVibrato (std::vector<EditNote>& notes, const std::vector<PitchFrame>& fr, double hop, const MagicSettings& ms,
                       const std::vector<int>& selection, bool regenerate, std::uint32_t seedBase)
{
    const auto dec = analyseForVibrato (notes, fr, hop, ms);
    int count = 0;
    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        const bool inSel = selection.empty() || std::find (selection.begin(), selection.end(), static_cast<int> (i)) != selection.end();
        if (inSel)
        {
            auto& e = notes[i].edit;
            MagicDecision decision = dec[i];
            // An explicit note selection is an instruction, not merely a suggestion:
            // allow Magic Vibrato on a usable sustained note even if the global
            // phrase heuristics would otherwise reject it as too short/non-terminal.
            // Preserve existing natural vibrato to avoid stacking two oscillators.
            const double duration = notes[i].note.endSec - notes[i].note.startSec;
            if (! selection.empty() && ! decision.apply && ! e.deleted && duration >= 0.25
                && noteHasUsableVoicing (notes[i], fr, hop)
                && ! noteHasNaturalVibrato (notes[i], fr, hop))
            {
                decision.apply = true;
                decision.start = ms.style == 1 ? 0.35 : 0.0;
                decision.amountScale = 0.85;
                decision.delayScale = 0.65;
            }
            e.vibrato = decision.apply;
            e.vibAmountScale = decision.amountScale; e.vibDelayScale = decision.delayScale;
            e.vibStart = decision.apply ? decision.start : 0.0; e.vibEnd = 1.0;
            if (regenerate || e.vibSeed == 1) e.vibSeed = seedBase * 2654435761u + static_cast<std::uint32_t> (i) * 40503u + 17u;
        }
        if (notes[i].edit.vibrato && (selection.empty() || inSel)) ++count;
    }
    return count;
}
} // namespace cooltone
