#include "PitchDetection.h"
#include "DSPUtilities.h"
#include <cmath>

namespace cooltone
{
int PitchDetector::hopSamples (const YinParams& p)
{
    if (! std::isfinite (p.sampleRate) || ! std::isfinite (p.hopSeconds) || p.sampleRate < 8000.0 || p.sampleRate > 384000.0 || p.hopSeconds <= 0.0 || p.hopSeconds > 1.0)
        return 1;
    const double samples = p.sampleRate * p.hopSeconds;
    if (! std::isfinite (samples) || samples >= 1000000.0) return 1000000;
    return std::max (1, static_cast<int> (std::lround (samples)));
}

// NOTE: the difference function is computed directly (O(W * tauMax) per frame). It is exact and
// fine for offline analysis; an FFT-based version is a planned optimisation (see README).
std::vector<PitchFrame> PitchDetector::analyse (const float* x, std::size_t n, const YinParams& p)
{
    std::vector<PitchFrame> out;
    if (x == nullptr || ! std::isfinite (p.sampleRate) || ! std::isfinite (p.minHz)
        || ! std::isfinite (p.maxHz) || ! std::isfinite (p.threshold)
        || ! std::isfinite (p.maxAperiodicity) || ! std::isfinite (p.hopSeconds)
        || p.sampleRate < 8000.0 || p.sampleRate > 384000.0
        || p.minHz < 20.0 || p.maxHz <= p.minHz || p.maxHz >= p.sampleRate * 0.45
        || p.threshold < 0.0 || p.threshold > 1.0 || p.maxAperiodicity < 0.0 || p.maxAperiodicity > 1.0
        || p.hopSeconds <= 0.0 || p.hopSeconds > 1.0
        || p.minRms < 0.0 || ! std::isfinite (p.minRms))
        return out;
    const int tauMax = static_cast<int> (std::ceil (p.sampleRate / p.minHz)) + 1;
    if (tauMax < 4 || tauMax > 20000) return out;
    const int tauMin = std::max (2, static_cast<int> (std::floor (p.sampleRate / p.maxHz)));
    const int W = tauMax;
    const std::size_t frameLen = static_cast<std::size_t> (W + tauMax);
    const std::size_t hop = static_cast<std::size_t> (hopSamples (p));
    if (n < frameLen) return out;

    std::vector<double> dn (static_cast<std::size_t> (tauMax) + 1);
    std::vector<double> d (static_cast<std::size_t> (tauMax) + 1);

    for (std::size_t start = 0; start + frameLen <= n; start += hop)
    {
        const float* f = x + start;
        PitchFrame pf;
        pf.timeSec = (static_cast<double> (start) + static_cast<double> (frameLen) * 0.5) / p.sampleRate;

        double e = 0.0;
        for (int j = 0; j < W; ++j) e += static_cast<double> (f[j]) * f[j];
        pf.rms = static_cast<float> (std::sqrt (e / W));
        if (! std::isfinite (pf.rms)) { out.push_back (pf); continue; }
        if (pf.rms < p.minRms) { out.push_back (pf); continue; }

        d[0] = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double s = 0.0;
            for (int j = 0; j < W; ++j) { const double diff = static_cast<double> (f[j]) - f[j + tau]; s += diff * diff; }
            d[static_cast<std::size_t> (tau)] = s;
        }
        dn[0] = 1.0;
        double running = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += d[static_cast<std::size_t> (tau)];
            dn[static_cast<std::size_t> (tau)] = running > 0.0 ? d[static_cast<std::size_t> (tau)] * tau / running : 1.0;
        }

        int tau = -1;
        for (int t = tauMin; t < tauMax; ++t)
        {
            if (dn[static_cast<std::size_t> (t)] < p.threshold)
            {
                while (t + 1 < tauMax && dn[static_cast<std::size_t> (t + 1)] < dn[static_cast<std::size_t> (t)]) ++t;
                tau = t; break;
            }
        }
        if (tau < 0)
        {
            tau = tauMin;
            for (int t = tauMin; t < tauMax; ++t)
                if (dn[static_cast<std::size_t> (t)] < dn[static_cast<std::size_t> (tau)]) tau = t;
        }

        const double best = dn[static_cast<std::size_t> (tau)];
        if (! std::isfinite (best)) { out.push_back (pf); continue; }
        double tauF = tau;
        if (tau > 1 && tau < tauMax)
        {
            const double s0 = dn[static_cast<std::size_t> (tau - 1)], s1 = best, s2 = dn[static_cast<std::size_t> (tau + 1)];
            const double den = s0 + s2 - 2.0 * s1;
            if (den > 1e-12) tauF = tau + (s0 - s2) / (2.0 * den);
        }

        if (! std::isfinite (tauF) || tauF < tauMin || tauF > tauMax) tauF = static_cast<double> (tau);
        pf.confidence = static_cast<float> (clampValue (1.0 - best, 0.0, 1.0));
        if (best <= p.maxAperiodicity)
        {
            pf.voiced = true;
            pf.f0Hz = p.sampleRate / tauF;
            pf.midi = hzToMidi (pf.f0Hz);
        }
        out.push_back (pf);
    }
    return out;
}
} // namespace cooltone
