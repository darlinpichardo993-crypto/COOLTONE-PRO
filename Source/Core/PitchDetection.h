#pragma once
// COOLTONE by ORYMY - YMY RECORDS
// YIN fundamental-frequency tracker (de Cheveigne & Kawahara, 2002), offline.
#include <cstddef>
#include <vector>

namespace cooltone
{
struct YinParams
{
    double sampleRate = 44100.0;
    double minHz = 70.0;
    double maxHz = 1000.0;
    double hopSeconds = 0.010;       // analysis hop
    double threshold = 0.15;         // YIN absolute threshold for the first dip
    double maxAperiodicity = 0.30;   // frames whose best dip is above this are unvoiced
    double minRms = 0.003;           // below this level (~ -50 dBFS) a frame is unvoiced
};

struct PitchFrame
{
    double timeSec = 0.0;    // centre of the analysis window
    double f0Hz = 0.0;       // 0 when unvoiced
    double midi = 0.0;       // 0 when unvoiced
    float confidence = 0.f;  // 1 - aperiodicity
    float rms = 0.f;
    bool voiced = false;
};

class PitchDetector
{
public:
    static int hopSamples (const YinParams&);
    static std::vector<PitchFrame> analyse (const float* mono, std::size_t numSamples, const YinParams&);
};
} // namespace cooltone
