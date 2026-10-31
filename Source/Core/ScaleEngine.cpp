#include "ScaleEngine.h"
#include <cmath>

namespace cooltone
{
static const char* kScaleNames[] = { "Major", "Natural Minor", "Harmonic Minor", "Melodic Minor",
    "Major Pentatonic", "Minor Pentatonic", "Major Blues", "Minor Blues", "Dorian", "Phrygian", "Lydian",
    "Mixolydian", "Aeolian", "Locrian", "Ionian", "Jazz", "Chromatic", "Whole Tone", "Diminished", "Custom Scale" };
static const char* kKeyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

const char* ScaleEngine::scaleName (ScaleType t) { return kScaleNames[static_cast<int> (t)]; }
const char* ScaleEngine::keyName (int root) { return kKeyNames[((root % 12) + 12) % 12]; }

std::vector<int> ScaleEngine::intervals (ScaleType t)
{
    switch (t)
    {
        case ScaleType::Major:           return { 0, 2, 4, 5, 7, 9, 11 };
        case ScaleType::NaturalMinor:    return { 0, 2, 3, 5, 7, 8, 10 };
        case ScaleType::HarmonicMinor:   return { 0, 2, 3, 5, 7, 8, 11 };
        case ScaleType::MelodicMinor:    return { 0, 2, 3, 5, 7, 9, 11 };
        case ScaleType::MajorPentatonic: return { 0, 2, 4, 7, 9 };
        case ScaleType::MinorPentatonic: return { 0, 3, 5, 7, 10 };
        case ScaleType::MajorBlues:      return { 0, 2, 3, 4, 7, 9 };
        case ScaleType::MinorBlues:      return { 0, 3, 5, 6, 7, 10 };
        case ScaleType::Aeolian:         return { 0, 2, 3, 5, 7, 8, 10 };
        case ScaleType::Locrian:         return { 0, 1, 3, 5, 6, 8, 10 };
        case ScaleType::Ionian:          return { 0, 2, 4, 5, 7, 9, 11 };
        case ScaleType::WholeTone:       return { 0, 2, 4, 6, 8, 10 };
        case ScaleType::Diminished:      return { 0, 2, 3, 5, 6, 8, 9, 11 };
        case ScaleType::Jazz:            return { 0, 2, 4, 5, 7, 9, 10, 11 };
        case ScaleType::Dorian:          return { 0, 2, 3, 5, 7, 9, 10 };
        case ScaleType::Phrygian:        return { 0, 1, 3, 5, 7, 8, 10 };
        case ScaleType::Lydian:          return { 0, 2, 4, 6, 7, 9, 11 };
        case ScaleType::Mixolydian:      return { 0, 2, 4, 5, 7, 9, 10 };
        case ScaleType::Chromatic:       return { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
        case ScaleType::Custom:          return {};
    }
    return {};
}

void ScaleEngine::recompute()
{
    if (scale_ == ScaleType::Custom)
        base_ = static_cast<std::uint16_t> (customMask_ & 0x0FFF);
    else
    {
        unsigned m = 0;
        for (int i : intervals (scale_)) m |= 1u << ((root_ + i) % 12);
        base_ = static_cast<std::uint16_t> (m);
    }
}

void ScaleEngine::clearOverrides() { forceOn_ = forceOff_ = 0; }
void ScaleEngine::setKey (int root) { root_ = ((root % 12) + 12) % 12; clearOverrides(); recompute(); }
void ScaleEngine::setScale (ScaleType t) { scale_ = t; clearOverrides(); recompute(); }
void ScaleEngine::setCustomMask (std::uint16_t m) { customMask_ = static_cast<std::uint16_t> (m & 0x0FFF); recompute(); }

void ScaleEngine::setNoteEnabled (int pc, bool enabled)
{
    const unsigned bit = 1u << (((pc % 12) + 12) % 12);
    if (enabled) { forceOn_ = static_cast<std::uint16_t> (forceOn_ | bit); forceOff_ = static_cast<std::uint16_t> (forceOff_ & ~bit); }
    else         { forceOff_ = static_cast<std::uint16_t> (forceOff_ | bit); forceOn_ = static_cast<std::uint16_t> (forceOn_ & ~bit); }
}

std::uint16_t ScaleEngine::mask() const
{
    return static_cast<std::uint16_t> (((base_ | forceOn_) & ~static_cast<unsigned> (forceOff_)) & 0x0FFFu);
}

bool ScaleEngine::isAllowed (int midiNote) const
{
    return (mask() >> (((midiNote % 12) + 12) % 12)) & 1u;
}

double ScaleEngine::nearestAllowed (double midi) const
{
    const unsigned m = mask();
    if (m == 0) return std::round (midi);
    const int c = static_cast<int> (std::lround (midi));
    double best = std::round (midi), bestD = 1e9;
    for (int n = c - 12; n <= c + 12; ++n)
    {
        if ((m >> (((n % 12) + 12) % 12)) & 1u)
        {
            const double d = std::abs (n - midi);
            if (d < bestD - 1e-12) { bestD = d; best = n; }
        }
    }
    return best;
}

KeyEstimate ScaleEngine::detectKey (const std::array<double, 12>& h)
{
    static const double major[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double minor[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    double meanH = 0; for (double v : h) meanH += v; meanH /= 12.0;
    KeyEstimate best; best.correlation = -2.0;
    for (int isMinor = 0; isMinor < 2; ++isMinor)
    {
        const double* prof = isMinor ? minor : major;
        double meanP = 0; for (int i = 0; i < 12; ++i) meanP += prof[i]; meanP /= 12.0;
        for (int r = 0; r < 12; ++r)
        {
            double num = 0, dh = 0, dp = 0;
            for (int pc = 0; pc < 12; ++pc)
            {
                const double a = h[pc] - meanH, b = prof[(pc - r + 12) % 12] - meanP;
                num += a * b; dh += a * a; dp += b * b;
            }
            const double corr = (dh > 0 && dp > 0) ? num / std::sqrt (dh * dp) : 0.0;
            if (corr > best.correlation) { best.correlation = corr; best.root = r; best.minor = (isMinor == 1); }
        }
    }
    return best;
}
} // namespace cooltone
