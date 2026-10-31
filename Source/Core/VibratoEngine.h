#pragma once
// COOLTONE by ORYMY - YMY RECORDS - eight vibrato algorithms producing a cents curve per analysis frame
#include "Params.h"
#include <cstdint>
#include <vector>

namespace cooltone
{
enum VibratoStyle { VibBachata = 0, VibRun, VibSpeed, VibSlow, VibNatural, VibWide, VibTight, VibEmotional };

struct VibratoParams
{
    int style = VibBachata;
    double rateHz = 5.5, depthCents = 35.0;
    double delaySec = 0.25, attackSec = 0.30, releaseSec = 0.12;
    double shape = 0.0;        // 0 sine .. 1 triangle
    double randomness = 0.25;  // slow wobble of the rate
    double humanize = 0.30;    // slow wobble of the depth
    double speed = 0.0;        // rate ramp over the note: -0.5 .. +0.5
    double smoothness = 0.30;
    double variation = 0.25;   // cycle-to-cycle depth variation (Natural Variation)
    double amount = 1.0;       // Mix: master scale of the depth
    double phase = 0.0;        // initial phase (radians)
};

VibratoParams vibratoParamsFromSettings (const Settings&);

// Cents offset for each frame of a note of the given duration (frame period = hopSec).
// The curve is zero-mean-ish and centred on 0: it is ADDED to the (corrected) pitch, never shifts the note.
std::vector<double> generateVibratoCents (double durationSec, double hopSec, const VibratoParams&, std::uint32_t seed);
} // namespace cooltone
