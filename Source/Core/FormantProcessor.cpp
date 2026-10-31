#include "FormantProcessor.h"
#include "Biquad.h"
#include "DSPUtilities.h"
#include <algorithm>
#include <cmath>

namespace cooltone
{
namespace
{
struct Track
{
    double t0 = 0, hop = 0.01; const std::vector<double>* v = nullptr; std::size_t lo = 0, hi = 0;
    double at (double t) const
    {
        double u = (t - t0) / hop;
        u = std::max (static_cast<double> (lo), std::min (static_cast<double> (hi), u));
        const std::size_t i = static_cast<std::size_t> (u);
        if (i >= hi) return (*v)[hi];
        const double f = u - static_cast<double> (i);
        return (*v)[i] * (1.0 - f) + (*v)[i + 1] * f;
    }
};

inline double sampleAt (const std::vector<float>& x, long i) { return (i < 0 || i >= static_cast<long> (x.size())) ? 0.0 : static_cast<double> (x[static_cast<std::size_t> (i)]); }

inline double interp4 (const std::vector<float>& x, double pos)
{
    const long i = static_cast<long> (std::floor (pos));
    const double f = pos - static_cast<double> (i);
    if (f == 0.0) return sampleAt (x, i);
    const double y0 = sampleAt (x, i - 1), y1 = sampleAt (x, i), y2 = sampleAt (x, i + 1), y3 = sampleAt (x, i + 2);
    return y1 + 0.5 * f * (y2 - y0 + f * (2 * y0 - 5 * y1 + 4 * y2 - y3 + f * (3 * (y1 - y2) + y3 - y0)));
}
} // namespace

void psolaProcess (const std::vector<std::vector<float>>& in, double sr, const std::vector<PitchFrame>& frames, double hop,
                   const std::vector<double>& ratioIn, const std::vector<double>& formantIn, std::vector<std::vector<float>>& out)
{
    out = in;
    if (in.empty() || frames.empty()) return;
    const std::size_t nCh = in.size(), nS = in[0].size(), N = frames.size();
    if (ratioIn.size() != N || formantIn.size() != N) return;

    std::vector<double> f0 (N, 0.0), rr = ratioIn, ff = formantIn;
    std::vector<char> v (N, 0);
    for (std::size_t i = 0; i < N; ++i) if (frames[i].voiced) { f0[i] = frames[i].f0Hz; v[i] = 1; }
    for (std::size_t i = 0; i < N;)   // bridge unvoiced gaps of <= 2 frames
    {
        if (v[i]) { ++i; continue; }
        std::size_t j = i; while (j < N && ! v[j]) ++j;
        if (i > 0 && j < N && (j - i) <= 2)
            for (std::size_t k = i; k < j; ++k)
            {
                const double u = static_cast<double> (k - i + 1) / static_cast<double> (j - i + 1);
                f0[k] = f0[i - 1] + (f0[j] - f0[i - 1]) * u; rr[k] = rr[i - 1] + (rr[j] - rr[i - 1]) * u; ff[k] = ff[i - 1] + (ff[j] - ff[i - 1]) * u; v[k] = 1;
            }
        i = j;
    }

    // zero-phase low-passed mono mix, only used to place pitch marks
    std::vector<float> lp (nS, 0.f);
    for (std::size_t n = 0; n < nS; ++n) { double a = 0; for (std::size_t c = 0; c < nCh; ++c) a += in[c][n]; lp[n] = static_cast<float> (a / static_cast<double> (nCh)); }
    { Biquad f = Biquad::lowpass (sr, 900.0); for (std::size_t n = 0; n < nS; ++n) lp[n] = f.process (lp[n]);
      f.reset(); for (std::size_t n = nS; n-- > 0;) lp[n] = f.process (lp[n]); }

    const double t0 = frames[0].timeSec;
    const long edge = std::max<long> (2, static_cast<long> (0.008 * sr));

    for (std::size_t i0 = 0; i0 < N;)
    {
        if (! v[i0]) { ++i0; continue; }
        std::size_t i1 = i0; while (i1 + 1 < N && v[i1 + 1]) ++i1;
        const std::size_t runStart = i0; i0 = i1 + 1;

        bool needed = false;
        for (std::size_t k = runStart; k <= i1; ++k) if (std::abs (rr[k] - 1.0) > 1e-4 || std::abs (ff[k] - 1.0) > 1e-4) { needed = true; break; }
        if (! needed || i1 - runStart < 3) continue;

        Track f0t { t0, hop, &f0, runStart, i1 }, rt { t0, hop, &rr, runStart, i1 }, ft { t0, hop, &ff, runStart, i1 };
        const long a = std::max<long> (0, std::lround ((frames[runStart].timeSec - 0.5 * hop) * sr));
        const long b = std::min<long> (static_cast<long> (nS) - 1, std::lround ((frames[i1].timeSec + 0.5 * hop) * sr));
        if (b - a < 64) continue;

        auto periodAt = [&] (double sample) { return clampValue (sr / std::max (1.0, f0t.at (sample / sr)), sr / 1200.0, sr / 50.0); };

        // ---- analysis pitch marks ----
        std::vector<long> marks;
        {
            const long lim = std::min (b, a + static_cast<long> (periodAt (static_cast<double> (a))));
            long p0 = a; float best = -1.f;
            for (long n = a; n <= lim; ++n) if (std::abs (lp[static_cast<std::size_t> (n)]) > best) { best = std::abs (lp[static_cast<std::size_t> (n)]); p0 = n; }
            const float sgn = lp[static_cast<std::size_t> (p0)] >= 0.f ? 1.f : -1.f;
            auto refine = [&] (long centre, double P)
            {
                const long lo = std::max<long> (0, centre - static_cast<long> (0.25 * P)), hi = std::min<long> (static_cast<long> (nS) - 1, centre + static_cast<long> (0.25 * P));
                long bp = std::min (std::max (centre, 0L), static_cast<long> (nS) - 1); float bv = -1e30f;
                for (long n = lo; n <= hi; ++n) { const float x = sgn * lp[static_cast<std::size_t> (n)]; if (x > bv) { bv = x; bp = n; } }
                return bp;
            };
            std::vector<long> back;
            long p = p0;
            for (;;) { const double P = periodAt (static_cast<double> (p)); const long tgt = p - std::lround (P); if (tgt < a - static_cast<long> (0.5 * P) || tgt < 0) break;
                       long r = refine (tgt, P); if (r >= p - static_cast<long> (0.5 * P)) r = tgt; back.push_back (r); p = r; }
            std::reverse (back.begin(), back.end());
            marks = back; marks.push_back (p0);
            p = p0;
            for (;;) { const double P = periodAt (static_cast<double> (p)); const long tgt = p + std::lround (P); if (tgt > b + static_cast<long> (0.5 * P) || tgt >= static_cast<long> (nS)) break;
                       long r = refine (tgt, P); if (r <= p + static_cast<long> (0.5 * P)) r = tgt; marks.push_back (r); p = r; }
        }
        if (marks.size() < 3) continue;

        // ---- synthesis ----
        const long G = static_cast<long> (sr / 50.0) + 8;
        const std::size_t span = static_cast<std::size_t> (b - a + 1 + 2 * G);
        std::vector<std::vector<double>> acc (nCh, std::vector<double> (span, 0.0));
        std::vector<double> norm (span, 0.0);
        double s = static_cast<double> (a);
        while (s <= static_cast<double> (b))
        {
            const double t = s / sr;
            const double fs = f0t.at (t), r = clampValue (rt.at (t), 0.5, 2.0), fct = clampValue (ft.at (t), 0.25, 4.0);
            const auto it = std::lower_bound (marks.begin(), marks.end(), static_cast<long> (std::lround (s)));
            long m = (it == marks.end()) ? marks.back() : *it;
            if (it != marks.begin() && it != marks.end() && std::labs (*(it - 1) - std::lround (s)) < std::labs (*it - std::lround (s))) m = *(it - 1);
            const long Pa = std::max<long> (8, std::lround (periodAt (static_cast<double> (m))));
            const long sInt = std::lround (s);
            for (long k = -Pa; k <= Pa; ++k)
            {
                const long idx = sInt + k;
                if (idx < a - G || idx > b + G) continue;
                const double w = 0.5 * (1.0 + std::cos (kPi * static_cast<double> (k) / static_cast<double> (Pa)));
                const std::size_t o = static_cast<std::size_t> (idx - (a - G));
                for (std::size_t c = 0; c < nCh; ++c) acc[c][o] += w * interp4 (in[c], static_cast<double> (m) + static_cast<double> (k) * fct);
                norm[o] += w;
            }
            s += sr / std::max (20.0, fs * r);
        }

        for (long n = a; n <= b; ++n)
        {
            const std::size_t o = static_cast<std::size_t> (n - (a - G));
            if (norm[o] < 1e-4) continue;
            double e = std::min (static_cast<double> (n - a), static_cast<double> (b - n)) / static_cast<double> (edge);
            e = clampValue (e, 0.0, 1.0); e = e * e * (3.0 - 2.0 * e);
            for (std::size_t c = 0; c < nCh; ++c)
                out[c][static_cast<std::size_t> (n)] = static_cast<float> (e * (acc[c][o] / norm[o]) + (1.0 - e) * static_cast<double> (in[c][static_cast<std::size_t> (n)]));
        }
    }
}
} // namespace cooltone
