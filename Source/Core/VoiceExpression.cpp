#include "VoiceExpression.h"
#include "DSPUtilities.h"
#include <algorithm>
#include <cmath>

namespace cooltone
{
ExpressionParams expressionFromSettings (const Settings& s)
{
    ExpressionParams p;
    p.breath = s.get (pBreath) / 100.0; p.airDb = s.get (pAir); p.presenceDb = s.get (pPresence);
    p.softness = s.get (pSoftness) / 100.0; p.dynamics = s.get (pDynamics) / 100.0;
    p.attackSec = s.get (pAttack) / 1000.0; p.releaseSec = s.get (pRelease) / 1000.0;
    return p;
}

void applyVoiceExpression (std::vector<std::vector<float>>& ch, double sr, const std::vector<PitchFrame>& fr, double hop,
                           const std::vector<double>& finalMidi, const std::vector<EditNote>& notes, const ExpressionParams& p)
{
    if (p.neutral() || ch.empty() || fr.empty()) return;
    processBreathAndTone (ch, sr, fr, hop, finalMidi, p);
    const std::size_t nS = ch[0].size();

    if (std::abs (p.dynamics) > 0.001)
    {
        std::vector<float> env (nS);
        const double aAtt = 1.0 - std::exp (-1.0 / (0.005 * sr)), aRel = 1.0 - std::exp (-1.0 / (0.060 * sr));
        double e = 0;
        for (std::size_t n = 0; n < nS; ++n)
        {
            double m = 0; for (auto& c : ch) m += std::abs (c[n]); m /= static_cast<double> (ch.size());
            e += (m > e ? aAtt : aRel) * (m - e);
            env[n] = static_cast<float> (e);
        }
        std::vector<double> lv;
        for (const auto& f : fr) if (f.voiced) { const std::size_t n = static_cast<std::size_t> (std::max (0.0, f.timeSec * sr)); if (n < nS) lv.push_back (env[n]); }
        if (! lv.empty())
        {
            std::sort (lv.begin(), lv.end());
            const double ref = std::max (1e-5, lv[lv.size() / 2]);
            const double aG = 1.0 - std::exp (-1.0 / (0.020 * sr));
            double g = 1.0;
            for (std::size_t n = 0; n < nS; ++n)
            {
                const double target = clampValue (std::pow (std::max (static_cast<double> (env[n]), ref * 0.1) / ref, p.dynamics), 0.35, 2.0);
                g += aG * (target - g);
                for (auto& c : ch) c[n] = static_cast<float> (c[n] * g);
            }
        }
    }

    if (p.attackSec > 0.0 || p.releaseSec > 0.0)
    {
        for (std::size_t i = 0; i < notes.size(); ++i)
        {
            const auto& nt = notes[i].note;
            if (notes[i].edit.deleted) continue;
            const double gapPrev = i > 0 ? nt.startSec - notes[i - 1].note.endSec : 1e9;
            const double gapNext = i + 1 < notes.size() ? notes[i + 1].note.startSec - nt.endSec : 1e9;
            if (p.attackSec > 0.0 && gapPrev > 0.03)
            {
                const long s0 = std::lround (nt.startSec * sr), len = std::lround (p.attackSec * sr);
                for (long k = 0; k < len; ++k) { const long n = s0 + k; if (n < 0 || n >= static_cast<long> (nS)) continue;
                    const double g = 0.5 - 0.5 * std::cos (kPi * static_cast<double> (k) / static_cast<double> (len));
                    for (auto& c : ch) c[static_cast<std::size_t> (n)] = static_cast<float> (c[static_cast<std::size_t> (n)] * g); }
            }
            if (p.releaseSec > 0.0 && gapNext > 0.03)
            {
                const long e0 = std::lround (nt.endSec * sr), len = std::lround (p.releaseSec * sr);
                for (long k = 0; k < len; ++k) { const long n = e0 - len + k; if (n < 0 || n >= static_cast<long> (nS)) continue;
                    const double g = 0.5 + 0.5 * std::cos (kPi * static_cast<double> (k) / static_cast<double> (len));
                    for (auto& c : ch) c[static_cast<std::size_t> (n)] = static_cast<float> (c[static_cast<std::size_t> (n)] * g); }
            }
        }
    }
}
} // namespace cooltone
