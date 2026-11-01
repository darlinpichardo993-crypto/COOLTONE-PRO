#include "VibratoEngine.h"
#include "DSPUtilities.h"
#include <algorithm>
#include <cmath>

namespace cooltone
{
namespace
{
struct Rng
{
    std::uint32_t s;
    explicit Rng (std::uint32_t seed) : s (seed ? seed : 0x9E3779B9u) { for (int i = 0; i < 4; ++i) next(); }
    std::uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    double uni() { return static_cast<double> (next() >> 8) / 16777216.0; }
    double bi() { return uni() * 2.0 - 1.0; }
};
struct SmoothNoise   // value noise, linear interpolation, ~freq Hz
{
    Rng r; double freq, phase = 0, a, b;
    SmoothNoise (std::uint32_t seed, double f) : r (seed), freq (f) { a = r.bi(); b = r.bi(); }
    double step (double dt) { phase += dt * freq; while (phase >= 1.0) { phase -= 1.0; a = b; b = r.bi(); } return a + (b - a) * phase; }
};
double smooth01 (double x) { x = clampValue (x, 0.0, 1.0); return 0.5 - 0.5 * std::cos (kPi * x); }
} // namespace

VibratoParams vibratoParamsFromSettings (const Settings& s)
{
    VibratoParams p;
    p.style = static_cast<int> (s.get (pVibStyle));
    p.rateHz = s.get (pVibRate); p.depthCents = s.get (pVibDepth);
    p.delaySec = s.get (pVibDelay) / 1000.0; p.attackSec = s.get (pVibAttack) / 1000.0; p.releaseSec = s.get (pVibRelease) / 1000.0;
    p.shape = s.get (pVibShape) / 100.0; p.randomness = s.get (pVibRandom) / 100.0; p.humanize = s.get (pVibHuman) / 100.0;
    p.speed = s.get (pVibSpeed) / 100.0; p.smoothness = s.get (pVibSmooth) / 100.0; p.variation = s.get (pVibVariation) / 100.0;
    p.amount = s.get (pVibAmount) / 100.0; p.phase = s.get (pVibPhase) * kPi / 180.0;
    return p;
}

std::vector<double> generateVibratoCents (double dur, double hop, const VibratoParams& p0, std::uint32_t seed)
{
    // Guard public DSP entry point against invalid settings and pathological frame counts.
    if (! std::isfinite (dur) || ! std::isfinite (hop) || dur <= 0.0 || hop <= 0.0 || dur > 600.0)
        return {};
    VibratoParams p = p0;
    p.rateHz = std::isfinite (p.rateHz) ? clampValue (p.rateHz, 2.0, 10.0) : 5.5;
    p.depthCents = std::isfinite (p.depthCents) ? clampValue (p.depthCents, 0.0, 100.0) : 0.0;
    p.delaySec = std::isfinite (p.delaySec) ? clampValue (p.delaySec, 0.0, dur) : 0.0;
    p.attackSec = std::isfinite (p.attackSec) ? clampValue (p.attackSec, 0.0, 2.0) : 0.0;
    p.releaseSec = std::isfinite (p.releaseSec) ? clampValue (p.releaseSec, 0.0, 2.0) : 0.0;
    p.shape = std::isfinite (p.shape) ? clampValue (p.shape, 0.0, 1.0) : 0.0;
    p.randomness = std::isfinite (p.randomness) ? clampValue (p.randomness, 0.0, 1.0) : 0.0;
    p.humanize = std::isfinite (p.humanize) ? clampValue (p.humanize, 0.0, 1.0) : 0.0;
    p.speed = std::isfinite (p.speed) ? clampValue (p.speed, -0.5, 0.5) : 0.0;
    p.smoothness = std::isfinite (p.smoothness) ? clampValue (p.smoothness, 0.0, 1.0) : 0.3;
    p.variation = std::isfinite (p.variation) ? clampValue (p.variation, 0.0, 1.0) : 0.0;
    p.amount = std::isfinite (p.amount) ? clampValue (p.amount, 0.0, 1.0) : 0.0;
    p.phase = std::isfinite (p.phase) ? std::fmod (p.phase, 2.0 * kPi) : 0.0;
    const double frameCount = std::ceil (dur / hop);
    if (! std::isfinite (frameCount) || frameCount > 5000000.0) return {};
    double rateMul = 1.0;
    // ---- algorithmic character of each style (applied on top of the user's controls) ----
    switch (p.style)
    {
        case VibRun:       p.attackSec = std::min (p.attackSec, 0.10); break;
        case VibSpeed:     rateMul = 1.35; p.depthCents *= 0.70; p.shape = std::max (p.shape, 0.25); p.variation *= 0.6; break;
        case VibSlow:      rateMul = 0.75; p.depthCents *= 1.15; p.shape = 0.0; p.smoothness = std::max (p.smoothness, 0.5); break;
        case VibNatural:   p.randomness = std::min (1.0, p.randomness * 2.0 + 0.3); p.humanize = std::min (1.0, p.humanize * 1.5 + 0.2);
                           p.variation = std::min (1.0, p.variation * 1.5 + 0.25); break;
        case VibWide:      p.depthCents *= 1.25; rateMul = 0.92; break;
        case VibTight:     p.randomness *= 0.2; p.humanize *= 0.2; p.variation *= 0.2; p.depthCents *= 0.60; p.attackSec = std::min (p.attackSec, 0.15); break;
        default: break;    // Bachata / Emotional: handled inside the loop
    }

    const std::size_t n = static_cast<std::size_t> (std::max (1.0, frameCount));
    std::vector<double> out (n, 0.0);
    Rng rng (seed);
    SmoothNoise rateNoise (seed * 2654435761u + 1u, 0.9), depthNoise (seed * 40503u + 7u, 0.6);
    double phase = p.phase, cycleAmp = 1.0, prev = 0.0;
    const double a = 1.0 - clampValue (p.smoothness, 0.0, 1.0) * 0.75;
    for (std::size_t i = 0; i < n; ++i)
    {
        const double t = static_cast<double> (i) * hop;
        const double ramp = t / std::max (dur, 1e-3);
        const double up = p.attackSec > 1e-6 ? smooth01 ((t - p.delaySec) / p.attackSec) : (t >= p.delaySec ? 1.0 : 0.0);
        const double down = p.releaseSec > 1e-6 ? smooth01 ((dur - t) / p.releaseSec) : 1.0;
        double env = up * down;
        double rate = p.rateHz * rateMul * (1.0 + p.speed * ramp);
        if (p.style == VibBachata)   rate *= 1.0 - 0.08 * ramp;                       // relaxes towards the end
        if (p.style == VibEmotional) { rate *= 1.0 + 0.12 * ramp; env *= std::min (1.3, 0.25 + 0.75 * std::pow (ramp, 1.5) * 1.3); } // swells
        rate = clampValue (rate * (1.0 + p.randomness * 0.2 * rateNoise.step (hop)), 2.0, 10.0);
        phase += 2.0 * kPi * rate * hop;
        if (phase >= p.phase + 2.0 * kPi) { phase -= 2.0 * kPi; cycleAmp = 1.0 + p.variation * 0.5 * rng.bi(); }
        const double wob = 1.0 + p.humanize * 0.3 * depthNoise.step (hop);
        double sn = std::sin (phase);
        if (p.style == VibBachata) sn = (sn + 0.18 * std::sin (2.0 * phase + 0.6)) / 1.12;   // warm, slightly asymmetric
        const double tri = (2.0 / kPi) * std::asin (std::sin (phase));
        const double s = (1.0 - p.shape) * sn + p.shape * tri;
        const double c = p.depthCents * p.amount * env * cycleAmp * wob * s;
        prev += a * (c - prev);
        out[i] = prev;
    }
    return out;
}
} // namespace cooltone
