#pragma once
// COOLTONE by ORYMY - YMY RECORDS
// Small, dependency-free DSP helpers shared by every Core module.
#include <algorithm>
#include <cmath>

namespace cooltone
{
constexpr double kPi = 3.14159265358979323846;

inline double hzToMidi (double hz) { return 69.0 + 12.0 * std::log2 (hz / 440.0); }
inline double midiToHz (double midi) { return 440.0 * std::pow (2.0, (midi - 69.0) / 12.0); }
inline double centsBetween (double hzA, double hzB) { return 1200.0 * std::log2 (hzA / hzB); }
template <typename T> inline T clampValue (T v, T lo, T hi) { return std::min (hi, std::max (lo, v)); }
} // namespace cooltone
