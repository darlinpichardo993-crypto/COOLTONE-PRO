#include "BreathProcessor.h"
#include "Biquad.h"
#include "DSPUtilities.h"
#include <algorithm>
#include <cmath>

namespace cooltone
{
void processBreathAndTone (std::vector<std::vector<float>>& ch, double sr, const std::vector<PitchFrame>& fr, double hop,
                           const std::vector<double>& finalMidi, const ExpressionParams& p)
{
    if (p.breathStageNeutral() || ch.empty() || fr.empty() || finalMidi.size() != fr.size()) return;
    const std::size_t N = fr.size(), nS = ch[0].size();

    std::vector<double> vr;
    for (const auto& f : fr) if (f.voiced) vr.push_back (f.rms);
    if (vr.empty()) return;
    std::sort (vr.begin(), vr.end());
    const double medV = vr[vr.size() / 2];

    std::vector<float> isV (N, 0.f), isB (N, 0.f);       // frame classes: voiced / breath (quiet unvoiced)
    for (std::size_t i = 0; i < N; ++i)
    {
        if (fr[i].voiced) isV[i] = 1.f;
        else if (fr[i].rms > 0.0005 && fr[i].rms < 0.25 * medV) isB[i] = 1.f;
    }
    const double t0 = fr[0].timeSec;

    for (auto& x : ch)
    {
        const std::vector<float> src = x;
        Biquad pres = Biquad::peaking (sr, 3000.0, 0.9, p.presenceDb);
        Biquad soft = Biquad::highShelf (sr, 6000.0, -8.0 * p.softness);
        Biquad air = Biquad::highShelf (sr, 5000.0, p.airDb);
        const bool doPres = std::abs (p.presenceDb) > 0.01, doSoft = p.softness > 0.001, doAir = std::abs (p.airDb) > 0.01;

        for (std::size_t n = 0; n < nS; ++n)
        {
            double u = (static_cast<double> (n) / sr - t0) / hop;
            u = clampValue (u, 0.0, static_cast<double> (N - 1));
            const std::size_t i0 = static_cast<std::size_t> (u), i1 = std::min (N - 1, i0 + 1);
            const double f = u - static_cast<double> (i0);
            const double wT = isV[i0] * (1.0 - f) + isV[i1] * f, wB = isB[i0] * (1.0 - f) + isB[i1] * f;
            const double y = src[n];
            double h = 0.0;
            if (wT > 0.0)
            {
                const double m = finalMidi[i0] > 0 ? finalMidi[i0] : finalMidi[i1];
                const double T = m > 0 ? sr / midiToHz (m) : 0.0;
                if (T > 20.0)
                {
                    auto at = [&] (double pos) { const long i = static_cast<long> (std::floor (pos)); const double fr2 = pos - static_cast<double> (i);
                        const double a = (i >= 0 && i < static_cast<long> (nS)) ? src[static_cast<std::size_t> (i)] : 0.0;
                        const double b = (i + 1 >= 0 && i + 1 < static_cast<long> (nS)) ? src[static_cast<std::size_t> (i + 1)] : 0.0;
                        return a + (b - a) * fr2; };
                    h = 0.25 * at (static_cast<double> (n) - T) + 0.5 * y + 0.25 * at (static_cast<double> (n) + T);
                }
                else h = y;
            }
            double A = wT * h;                      // tonal
            double R = wT * (y - h) + wB * y;       // breath / aperiodic
            const double O = y - A - R;             // everything else (sibilants, silence) stays untouched
            if (doPres) A = pres.process (static_cast<float> (A));
            if (doSoft) A = soft.process (static_cast<float> (A));
            if (doAir)  R = air.process (static_cast<float> (R));
            x[n] = static_cast<float> (A + R * p.breath + O);
        }
    }
}
} // namespace cooltone
