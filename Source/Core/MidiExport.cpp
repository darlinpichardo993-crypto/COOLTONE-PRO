#include "MidiExport.h"
#include "PitchCorrection.h"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace cooltone
{
namespace
{
void putVar (std::vector<unsigned char>& b, unsigned v)
{
    unsigned char tmp[5]; int n = 0; tmp[n++] = v & 0x7F;
    while ((v >>= 7) != 0) tmp[n++] = static_cast<unsigned char> ((v & 0x7F) | 0x80);
    while (n--) b.push_back (tmp[n]);
}
void put32 (std::vector<unsigned char>& b, unsigned v) { for (int i = 3; i >= 0; --i) b.push_back (static_cast<unsigned char> ((v >> (8 * i)) & 0xFF)); }
void put16 (std::vector<unsigned char>& b, unsigned v) { b.push_back (static_cast<unsigned char> ((v >> 8) & 0xFF)); b.push_back (static_cast<unsigned char> (v & 0xFF)); }
struct Ev { long tick; int order; unsigned char status, d1, d2; };
}

std::vector<unsigned char> buildMidiFile (const std::vector<EditNote>& notes, const Settings& s, double bpm)
{
    const int ppq = 480;
    bpm = std::max (20.0, std::min (400.0, bpm));
    const double tickPerSec = ppq * bpm / 60.0;
    std::vector<double> rmsv; for (const auto& n : notes) rmsv.push_back (n.note.meanRms);
    std::sort (rmsv.begin(), rmsv.end());
    const double medRms = rmsv.empty() ? 1.0 : std::max (1e-6, rmsv[rmsv.size() / 2]);

    std::vector<Ev> evs;
    auto addNote = [&] (double t0, double t1, double midi, double rms)
    {
        const int pitch = std::max (0, std::min (127, static_cast<int> (std::lround (midi + s.get (pTranspose) * 0.0))));
        const long a = std::lround (t0 * tickPerSec), b = std::max (a + 1, std::lround (t1 * tickPerSec));
        const int vel = std::max (30, std::min (120, static_cast<int> (std::lround (80.0 * std::pow (std::max (rms, 1e-6) / medRms, 0.5)))));
        evs.push_back ({ a, 1, 0x90, static_cast<unsigned char> (pitch), static_cast<unsigned char> (vel) });
        evs.push_back ({ b, 0, 0x80, static_cast<unsigned char> (pitch), 0 });
    };
    for (const auto& en : notes)
    {
        if (en.edit.deleted) continue;
        if (! en.edit.melisma.empty())
            for (const auto& m : en.edit.melisma) addNote (m.startSec, m.endSec, m.midi + s.get (pTranspose), en.note.meanRms);
        else
        {
            double p = noteBaseTarget (en, s) + s.get (pTranspose);
            const double c0 = 12.0 * std::log2 (s.get (pCenterHz) / 440.0);
            addNote (en.note.startSec, en.note.endSec, p - c0, en.note.meanRms);   // report in standard 440 tuning
        }
    }
    std::stable_sort (evs.begin(), evs.end(), [] (const Ev& x, const Ev& y) { return x.tick != y.tick ? x.tick < y.tick : x.order < y.order; });

    std::vector<unsigned char> trk;
    putVar (trk, 0); trk.push_back (0xFF); trk.push_back (0x51); trk.push_back (3);
    const unsigned uspq = static_cast<unsigned> (std::lround (60000000.0 / bpm));
    trk.push_back (static_cast<unsigned char> ((uspq >> 16) & 0xFF)); trk.push_back (static_cast<unsigned char> ((uspq >> 8) & 0xFF)); trk.push_back (static_cast<unsigned char> (uspq & 0xFF));
    long last = 0;
    for (const auto& e : evs) { putVar (trk, static_cast<unsigned> (e.tick - last)); last = e.tick; trk.push_back (e.status); trk.push_back (e.d1); trk.push_back (e.d2); }
    putVar (trk, 0); trk.push_back (0xFF); trk.push_back (0x2F); trk.push_back (0);

    std::vector<unsigned char> f;
    f.insert (f.end(), { 'M', 'T', 'h', 'd' }); put32 (f, 6); put16 (f, 0); put16 (f, 1); put16 (f, static_cast<unsigned> (ppq));
    f.insert (f.end(), { 'M', 'T', 'r', 'k' }); put32 (f, static_cast<unsigned> (trk.size()));
    f.insert (f.end(), trk.begin(), trk.end());
    return f;
}

bool writeMidiFile (const std::string& path, const std::vector<EditNote>& notes, const Settings& s, double bpm, std::string* err)
{
    const auto b = buildMidiFile (notes, s, bpm);
    std::ofstream f (path, std::ios::binary | std::ios::trunc);
    if (! f) { if (err) *err = "cannot open file"; return false; }
    f.write (reinterpret_cast<const char*> (b.data()), static_cast<std::streamsize> (b.size()));
    f.flush();
    if (! f) { if (err) *err = "write failed"; return false; }
    return true;
}
} // namespace cooltone
