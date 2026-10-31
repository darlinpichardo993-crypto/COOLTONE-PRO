#include "NoteSegmenter.h"
#include <algorithm>
#include <cmath>

namespace cooltone
{
static double medianOf (std::vector<double> v)
{
    if (v.empty()) return 0.0;
    std::sort (v.begin(), v.end());
    const std::size_t n = v.size();
    return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

std::vector<Note> segmentNotes (const std::vector<PitchFrame>& fr, double hop, const SegmenterParams& sp)
{
    const std::size_t N = fr.size();
    std::vector<double> sm (N, 0.0);
    for (std::size_t i = 0; i < N; ++i)
    {
        if (! fr[i].voiced) continue;
        std::vector<double> w;
        const std::ptrdiff_t lo = std::max<std::ptrdiff_t> (0, static_cast<std::ptrdiff_t> (i) - sp.smoothRadius);
        const std::ptrdiff_t hi = std::min<std::ptrdiff_t> (static_cast<std::ptrdiff_t> (N) - 1, static_cast<std::ptrdiff_t> (i) + sp.smoothRadius);
        for (std::ptrdiff_t k = lo; k <= hi; ++k)
            if (fr[static_cast<std::size_t> (k)].voiced) w.push_back (fr[static_cast<std::size_t> (k)].midi);
        sm[i] = medianOf (w);
    }

    std::vector<Note> out;
    bool open = false;
    std::size_t first = 0, last = 0, pendingStart = 0;
    int gap = 0;
    std::vector<double> vals, pend;
    std::vector<std::size_t> idx, pendIdx;

    auto closeNote = [&]()
    {
        if (! vals.empty() && static_cast<double> (last - first + 1) * hop >= sp.minNoteSec)
        {
            Note nt;
            nt.firstFrame = first; nt.lastFrame = last;
            nt.startSec = fr[first].timeSec - 0.5 * hop;
            nt.endSec = fr[last].timeSec + 0.5 * hop;
            nt.midiPitch = medianOf (vals);
            double r = 0; for (std::size_t k = first; k <= last; ++k) r += fr[k].rms;
            nt.meanRms = r / static_cast<double> (last - first + 1);
            out.push_back (nt);
        }
        open = false; vals.clear(); pend.clear(); idx.clear(); pendIdx.clear(); gap = 0;
    };

    for (std::size_t i = 0; i < N; ++i)
    {
        if (! fr[i].voiced)
        {
            if (open && ++gap > sp.maxGapFrames) closeNote();
            continue;
        }
        const double m = sm[i];
        if (! open)
        {
            open = true; first = last = i; vals = { m }; gap = 0; pend.clear(); pendIdx.clear();
            continue;
        }
        gap = 0;
        std::vector<double> tail (vals.end() - static_cast<std::ptrdiff_t> (std::min<std::size_t> (vals.size(), 12)), vals.end());
        const double ref = medianOf (tail);
        if (std::abs (m - ref) > sp.splitSemitones)
        {
            if (pend.empty()) pendingStart = i;
            pend.push_back (m);
            if (static_cast<int> (pend.size()) >= sp.splitConfirmFrames)
            {
                const std::size_t newFirst = pendingStart;
                std::vector<double> newVals = pend;
                closeNote();
                open = true; first = newFirst; last = i; vals = newVals; gap = 0;
            }
        }
        else
        {
            for (double v : pend) vals.push_back (v);
            pend.clear();
            vals.push_back (m);
            last = i;
        }
    }
    if (open) closeNote();
    return out;
}
} // namespace cooltone
