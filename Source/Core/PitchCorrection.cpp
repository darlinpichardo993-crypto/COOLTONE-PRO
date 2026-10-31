#include "PitchCorrection.h"
#include "DSPUtilities.h"
#include "VibratoEngine.h"
#include <algorithm>
#include <cmath>

namespace cooltone
{
double snapToScale (double midi, const Settings& s)
{
    ScaleEngine e; e.setScale (ScaleType::Custom); e.setCustomMask (s.scaleMask());
    const double c0 = 12.0 * std::log2 (s.get (pCenterHz) / 440.0);
    return e.nearestAllowed (midi - c0) + c0;
}

double noteBaseTarget (const EditNote& en, const Settings& s)
{
    if (en.edit.hasTarget) return en.edit.targetMidi;
    if (s.get (pCorrectionOn) > 0.5) return snapToScale (en.note.midiPitch, s);
    return en.note.midiPitch;
}

CorrectionResult buildCorrection (const std::vector<PitchFrame>& fr, double hop, const std::vector<EditNote>& notes, const Settings& s)
{
    const std::size_t N = fr.size();
    CorrectionResult r;
    r.origMidi.assign (N, 0.0); r.finalMidi.assign (N, 0.0); r.ratio.assign (N, 1.0); r.formant.assign (N, 1.0); r.noteIndex.assign (N, -1);

    const double keep = s.get (pFormantKeep) / 100.0;
    const double globalF = std::pow (2.0, s.get (pFormantShift) / 12.0);
    const double T = s.get (pTranspose);
    const bool corrOn = s.get (pCorrectionOn) > 0.5;
    const double strengthAuto = s.get (pStrength) / 100.0, human = s.get (pHumanize) / 100.0, drift = s.get (pDrift) / 100.0;
    const double retune = s.get (pRetune) / 1000.0, trans = s.get (pTransition) / 1000.0;
    const VibratoParams vp0 = vibratoParamsFromSettings (s);
    const double flexSemis = s.get (pFlex) / 100.0, rangeSemis = s.get (pRange) / 100.0;
    const bool keepOrigVib = s.get (pVibKeepOrig) > 0.5;

    for (std::size_t i = 0; i < N; ++i)
        if (fr[i].voiced) { r.origMidi[i] = fr[i].midi; r.finalMidi[i] = fr[i].midi; r.formant[i] = globalF; }

    double prevLast = -100.0, prevCs = 0.0;
    for (std::size_t ni = 0; ni < notes.size(); ++ni)
    {
        const EditNote& en = notes[ni];
        if (en.edit.deleted) continue;
        const std::size_t first = en.note.firstFrame, last = std::min (en.note.lastFrame, N - 1);
        if (last < first) continue;
        const std::size_t len = last - first + 1;

        // original pitch per frame; unvoiced frames inside the note are interpolated
        std::vector<double> orig (len, 0.0);
        std::vector<char> vo (len, 0);
        std::ptrdiff_t lastV = -1; bool any = false;
        for (std::size_t k = 0; k < len; ++k) if (fr[first + k].voiced) { orig[k] = fr[first + k].midi; vo[k] = 1; any = true; }
        if (! any) continue;
        for (std::size_t k = 0; k < len; ++k)
        {
            if (vo[k]) { lastV = static_cast<std::ptrdiff_t> (k); continue; }
            std::size_t nx = k; while (nx < len && ! vo[nx]) ++nx;
            if (lastV >= 0 && nx < len)
            {
                const double a = orig[static_cast<std::size_t> (lastV)], b = orig[nx];
                const double u = static_cast<double> (static_cast<std::ptrdiff_t> (k) - lastV) / static_cast<double> (static_cast<std::ptrdiff_t> (nx) - lastV);
                orig[k] = a + (b - a) * u;
            }
            else orig[k] = lastV >= 0 ? orig[static_cast<std::size_t> (lastV)] : orig[nx];
        }

        // slow (drift) / fast (vibrato, jitter) split of the deviation from the note centre
        const double med = en.note.midiPitch;
        std::vector<double> slow (len);
        const double aLP = 1.0 - std::exp (-hop / 0.12);
        double y = orig[0] - med;
        for (std::size_t k = 0; k < len; ++k) { y += aLP * ((orig[k] - med) - y); slow[k] = y; }
        y = slow[len - 1];
        for (std::size_t k = len; k-- > 0;) { y += aLP * (slow[k] - y); slow[k] = y; }

        const bool manual = en.edit.hasTarget || ! en.edit.melisma.empty();
        const bool replaceVib = en.edit.vibrato && ! keepOrigVib;      // synthetic vibrato REPLACES the singer's own modulation
        const bool useCorr = corrOn || manual || replaceVib;
        const double str = corrOn ? strengthAuto : 1.0;
        double baseT = noteBaseTarget (en, s);
        if (corrOn && ! manual)   // Flex Tune: leave nearly-in-tune notes alone; Correction Range: do not 'fix' deliberate leaps
        {
            const double dev = std::abs (baseT - med);
            if (dev < flexSemis || dev > rangeSemis) baseT = med;
        }
        // replacing the singer's own vibrato: work on the pitch WITHOUT its fast modulation, and drop that modulation exactly
        std::vector<double> fastRem (len, 0.0), origUse = orig;
        if (replaceVib) for (std::size_t k = 0; k < len; ++k) { fastRem[k] = (orig[k] - med) - slow[k]; origUse[k] = orig[k] - fastRem[k]; }

        std::vector<double> c (len);
        for (std::size_t k = 0; k < len; ++k)
        {
            const double fastD = (origUse[k] - med) - slow[k];
            double tgt = baseT;
            if (! en.edit.melisma.empty())
            {
                const double t = fr[first + k].timeSec;
                for (const auto& m : en.edit.melisma) if (t >= m.startSec && t < m.endSec) { tgt = m.midi; break; }
                if (t >= en.edit.melisma.back().endSec) tgt = en.edit.melisma.back().midi;
            }
            double D;
            if (! useCorr) D = origUse[k] + T;
            else D = (1.0 - str) * (origUse[k] + T) + str * (tgt + T + human * fastD + drift * slow[k]);
            c[k] = D - origUse[k];
        }

        // retune-speed smoothing of the correction (one-pole), longer glide just after touching notes
        const bool touching = (static_cast<double> (first) - prevLast) <= 2.0;
        std::vector<double> cs (len);
        cs[0] = touching ? prevCs : c[0];
        const std::size_t transFrames = static_cast<std::size_t> (std::ceil (trans / hop));
        for (std::size_t k = 0; k < len; ++k)
        {
            double tau = retune;
            if (touching && k < transFrames) tau = std::max (tau, trans / 3.0);
            const double a = tau <= 1e-6 ? 1.0 : 1.0 - std::exp (-hop / tau);
            const double prev = (k == 0) ? cs[0] : cs[k - 1];
            cs[k] = (k == 0 && ! touching) ? c[0] : prev + a * (c[k] - prev);
        }
        prevLast = static_cast<double> (last); prevCs = cs[len - 1];

        std::vector<double> vib (len, 0.0);
        if (en.edit.vibrato)
        {
            VibratoParams vp = vp0;
            vp.amount *= en.edit.vibAmountScale; vp.delaySec *= en.edit.vibDelayScale;
            vp.rateHz *= en.edit.vibSpeedScale; vp.depthCents *= en.edit.vibDepthScale;
            const std::size_t ks = static_cast<std::size_t> (std::lround (clampValue (en.edit.vibStart, 0.0, 1.0) * static_cast<double> (len)));
            const std::size_t ke = static_cast<std::size_t> (std::lround (clampValue (en.edit.vibEnd, 0.0, 1.0) * static_cast<double> (len)));
            if (ke > ks + 1)
            {
                const auto seg = generateVibratoCents (static_cast<double> (ke - ks) * hop, hop, vp, en.edit.vibSeed);
                for (std::size_t k = 0; k < seg.size() && ks + k < len; ++k) vib[ks + k] = seg[k];
            }
        }

        for (std::size_t k = 0; k < len; ++k)
        {
            const std::size_t i = first + k;
            r.noteIndex[i] = static_cast<int> (ni);
            if (! fr[i].voiced) continue;
            const double add = clampValue (cs[k] - fastRem[k] + (k < vib.size() ? vib[k] / 100.0 : 0.0), -12.0, 12.0);
            r.finalMidi[i] = orig[k] + add;
            r.ratio[i] = std::pow (2.0, add / 12.0);
            r.formant[i] = std::pow (r.ratio[i], 1.0 - keep) * globalF;
        }
    }
    return r;
}
} // namespace cooltone
