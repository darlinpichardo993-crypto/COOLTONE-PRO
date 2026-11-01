#include "MelismaEngine.h"
#include "DSPUtilities.h"
#include <algorithm>
#include <cmath>

namespace cooltone
{
MelismaParams melismaParamsFromSettings (const Settings& s)
{
    MelismaParams p;
    p.mode = static_cast<int> (s.get (pMelMode)); p.numNotes = static_cast<int> (std::lround (s.get (pMelNotes)));
    p.lengthFrac = s.get (pMelLength) / 100.0; p.startDegree = static_cast<int> (std::lround (s.get (pMelStart)));
    p.endDegree = static_cast<int> (std::lround (s.get (pMelEnd))); p.rhythm = static_cast<int> (s.get (pMelRhythm));
    p.humanize = s.get (pMelHuman) / 100.0; p.scaleLock = s.get (pMelLock) > 0.5;
    return p;
}

namespace
{
struct Rng { std::uint32_t s; explicit Rng (std::uint32_t x) : s (x ? x : 77u) { next(); next(); }
    std::uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    double uni() { return static_cast<double> (next() >> 8) / 16777216.0; } };
}

std::vector<MelismaStep> generateMelisma (double t0, double t1, double base, std::uint16_t mask, const MelismaParams& p, std::uint32_t seed)
{
    std::vector<MelismaStep> out;
    if (! std::isfinite (t0) || ! std::isfinite (t1) || ! std::isfinite (base)
        || ! std::isfinite (p.lengthFrac) || ! std::isfinite (p.humanize) || t1 <= t0)
        return out;
    const double dur = t1 - t0;
    if (dur < 0.2 || dur > 600.0) return out;

    // pitch ladder (allowed MIDI notes, or chromatic when scale lock is off)
    std::vector<int> ladder;
    const int c = static_cast<int> (std::lround (base));
    for (int n = c - 36; n <= c + 36; ++n)
        if (! p.scaleLock || mask == 0 || ((mask >> (((n % 12) + 12) % 12)) & 1u)) ladder.push_back (n);
    if (ladder.empty()) return out;
    std::size_t bi = 0; double bd = 1e9;
    for (std::size_t i = 0; i < ladder.size(); ++i) { const double d = std::abs (ladder[i] - base); if (d < bd) { bd = d; bi = i; } }
    auto idxAt = [&] (int degree) { return static_cast<int> (std::min<long> (static_cast<long> (ladder.size()) - 1, std::max<long> (0, static_cast<long> (bi) + degree))); };

    // limit the step count so that no step is shorter than ~45 ms
    const double runLen = dur * clampValue (p.lengthFrac, 0.1, 1.0);
    int N = std::max (2, std::min (12, p.numNotes));
    while (N > 2 && (runLen / (N - 1)) < 0.045) --N;

    const int sIdx = idxAt (p.startDegree), eIdx = idxAt (p.endDegree);
    const int lo = std::min (sIdx, eIdx), hi = std::max (sIdx, eIdx);
    std::vector<int> seq (static_cast<std::size_t> (N));
    Rng rng (seed);
    auto lerpIdx = [&] (double u, int a, int b) { return static_cast<int> (std::lround (a + (b - a) * u)); };
    for (int i = 0; i < N; ++i)
    {
        const double u = N > 1 ? static_cast<double> (i) / (N - 1) : 1.0;
        int idx = eIdx;
        switch (p.mode)
        {
            case 0: idx = lerpIdx (u, lo, hi); break;                                      // ascending
            case 1: idx = lerpIdx (u, hi, lo); break;                                      // descending
            case 2: { const double w = 1.0 - std::abs (2.0 * u - 1.0); idx = lerpIdx (w, sIdx, (sIdx == hi ? lo : hi)); break; } // wave
            case 3: { const int step = (i % 2 == 0) ? (i / 2) * 2 : (i / 2) * 2 - 1; idx = std::min (hi, lo + std::max (0, step)); break; } // soul run: +2 -1
            case 4: { const int a = lerpIdx (u, hi, lo); idx = (i % 2 == 1 && i < N - 1) ? std::min (hi, a + 1) : a; break; }  // R&B: descending with upper neighbours
            case 5: idx = std::min (hi, lo + i * 2); break;                                // Latin: ascending in thirds
            default: idx = lo + static_cast<int> (rng.uni() * (hi - lo + 1)); break;      // custom: seeded random walk
        }
        seq[static_cast<std::size_t> (i)] = std::min (hi, std::max (lo, idx));
    }
    seq.back() = eIdx;   // always resolve on the requested ending note

    // step durations
    std::vector<double> w (static_cast<std::size_t> (N - 1));
    for (int i = 0; i < N - 1; ++i)
    {
        const double u = (N > 2) ? static_cast<double> (i) / (N - 2) : 0.0;
        double wi = 1.0;
        if (p.rhythm == 1) wi = (i % 2 == 0) ? 1.3 : 0.7;
        else if (p.rhythm == 2) wi = 1.5 - u;
        else if (p.rhythm == 3) wi = 0.5 + u;
        wi *= 1.0 + p.humanize * 0.3 * (rng.uni() * 2.0 - 1.0);
        w[static_cast<std::size_t> (i)] = std::max (0.2, wi);
    }
    double sum = 0; for (double x : w) sum += x;
    double t = t0;
    for (int i = 0; i < N - 1; ++i)
    {
        const double d = runLen * w[static_cast<std::size_t> (i)] / sum;
        out.push_back ({ t, t + d, static_cast<double> (ladder[static_cast<std::size_t> (seq[static_cast<std::size_t> (i)])]) });
        t += d;
    }
    out.push_back ({ t, t1, static_cast<double> (ladder[static_cast<std::size_t> (seq.back())]) });
    return out;
}
} // namespace cooltone
