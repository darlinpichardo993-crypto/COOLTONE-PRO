#pragma once
// COOLTONE by ORYMY - YMY RECORDS - RBJ biquad filters
#include <cmath>
#include "DSPUtilities.h"
namespace cooltone
{
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    float process (float x) { const double xd = x; const double y = b0 * xd + z1; z1 = b1 * xd - a1 * y + z2; z2 = b2 * xd - a2 * y; return static_cast<float> (y); }
    void setNorm (double B0, double B1, double B2, double A0, double A1, double A2) { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
    static Biquad lowpass (double sr, double fc, double q = 0.70710678)
    {
        Biquad f; const double w = 2 * kPi * fc / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        f.setNorm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); return f;
    }
    static Biquad peaking (double sr, double fc, double q, double gainDb)
    {
        Biquad f; const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * fc / sr, c = std::cos (w), al = std::sin (w) / (2 * q);
        f.setNorm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A); return f;
    }
    static Biquad highShelf (double sr, double fc, double gainDb)
    {
        Biquad f; const double A = std::pow (10.0, gainDb / 40.0), w = 2 * kPi * fc / sr, c = std::cos (w);
        const double al = std::sin (w) / 2.0 * std::sqrt (2.0), sA = 2 * std::sqrt (A) * al;
        f.setNorm (A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
                   (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA);
        return f;
    }
};
} // namespace cooltone
