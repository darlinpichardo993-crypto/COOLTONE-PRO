#include "NoteEditor.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace cooltone
{
static double medianOf (std::vector<double> v)
{
    if (v.empty()) return 0.0;
    std::sort (v.begin(), v.end());
    const std::size_t n = v.size();
    return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

void refreshNote (EditNote& en, const std::vector<PitchFrame>& fr, double hop)
{
    Note& n = en.note;
    n.startSec = fr[n.firstFrame].timeSec - 0.5 * hop;
    n.endSec = fr[n.lastFrame].timeSec + 0.5 * hop;
    std::vector<double> v; double r = 0;
    for (std::size_t k = n.firstFrame; k <= n.lastFrame; ++k) { if (fr[k].voiced) v.push_back (fr[k].midi); r += fr[k].rms; }
    if (! v.empty()) n.midiPitch = medianOf (v);
    n.meanRms = r / static_cast<double> (n.lastFrame - n.firstFrame + 1);
}

std::vector<EditNote> makeEditNotes (const std::vector<Note>& notes)
{
    std::vector<EditNote> out;
    for (const auto& n : notes) { EditNote e; e.note = n; out.push_back (e); }
    return out;
}

bool splitNote (std::vector<EditNote>& notes, const std::vector<PitchFrame>& fr, double hop, std::size_t idx, double t)
{
    if (idx >= notes.size() || fr.empty()) return false;
    const long k = std::lround ((t - fr[0].timeSec) / hop);
    EditNote a = notes[idx];
    if (k <= static_cast<long> (a.note.firstFrame) + 1 || k >= static_cast<long> (a.note.lastFrame) - 1) return false;
    EditNote b = a;
    b.note.firstFrame = static_cast<std::size_t> (k);
    a.note.lastFrame = static_cast<std::size_t> (k - 1);
    a.edit.melisma.clear(); b.edit.melisma.clear();
    b.edit.vibSeed = a.edit.vibSeed * 1664525u + 1013904223u;
    refreshNote (a, fr, hop); refreshNote (b, fr, hop);
    notes[idx] = a;
    notes.insert (notes.begin() + static_cast<std::ptrdiff_t> (idx) + 1, b);
    return true;
}

bool glueNotes (std::vector<EditNote>& notes, const std::vector<PitchFrame>& fr, double hop, std::size_t idx)
{
    if (idx + 1 >= notes.size()) return false;
    EditNote a = notes[idx];
    const EditNote& b = notes[idx + 1];
    a.note.lastFrame = b.note.lastFrame;
    a.edit.vibrato = a.edit.vibrato || b.edit.vibrato;
    a.edit.deleted = a.edit.deleted && b.edit.deleted;
    a.edit.hasTarget = false; a.edit.melisma.clear();
    refreshNote (a, fr, hop);
    notes[idx] = a;
    notes.erase (notes.begin() + static_cast<std::ptrdiff_t> (idx) + 1);
    return true;
}

int findNoteAt (const std::vector<EditNote>& notes, double t)
{
    for (std::size_t i = 0; i < notes.size(); ++i)
        if (t >= notes[i].note.startSec && t <= notes[i].note.endSec) return static_cast<int> (i);
    return -1;
}

std::string serializeEdits (const std::vector<EditNote>& notes)
{
    std::string out = "COOLTONE_EDITS 1\n";
    char buf[256];
    for (const auto& e : notes)
    {
        std::snprintf (buf, sizeof buf, "N %zu %zu %d %.6f %d %d %u %.4f %.4f %.4f %.4f %.4f %.4f %zu", e.note.firstFrame, e.note.lastFrame,
                       e.edit.hasTarget ? 1 : 0, e.edit.targetMidi, e.edit.deleted ? 1 : 0, e.edit.vibrato ? 1 : 0,
                       e.edit.vibSeed, e.edit.vibAmountScale, e.edit.vibDelayScale, e.edit.vibStart, e.edit.vibEnd,
                       e.edit.vibSpeedScale, e.edit.vibDepthScale, e.edit.melisma.size());
        out += buf;
        for (const auto& m : e.edit.melisma)
        {
            std::snprintf (buf, sizeof buf, " %.6f %.6f %.6f", m.startSec, m.endSec, m.midi);
            out += buf;
        }
        out += "\n";
    }
    return out;
}

bool parseEdits (const std::string& text, std::vector<EditNote>& notes, std::size_t numFrames, const std::vector<PitchFrame>& fr, double hop)
{
    std::istringstream in (text);
    std::string line;
    if (! std::getline (in, line) || line.rfind ("COOLTONE_EDITS", 0) != 0) return false;
    std::vector<EditNote> out;
    while (std::getline (in, line))
    {
        std::istringstream ls (line);
        std::string tag; ls >> tag;
        if (tag != "N") continue;
        EditNote e; std::size_t f = 0, l = 0, nm = 0; int ht = 0, del = 0, vib = 0; unsigned seed = 1;
        ls >> f >> l >> ht >> e.edit.targetMidi >> del >> vib >> seed >> e.edit.vibAmountScale >> e.edit.vibDelayScale
           >> e.edit.vibStart >> e.edit.vibEnd >> e.edit.vibSpeedScale >> e.edit.vibDepthScale >> nm;
        if (! ls || f > l || l >= numFrames) return false;
        e.edit.hasTarget = ht != 0; e.edit.deleted = del != 0; e.edit.vibrato = vib != 0; e.edit.vibSeed = seed;
        for (std::size_t i = 0; i < nm; ++i) { MelismaStep m; ls >> m.startSec >> m.endSec >> m.midi; e.edit.melisma.push_back (m); }
        e.note.firstFrame = f; e.note.lastFrame = l;
        refreshNote (e, fr, hop);
        out.push_back (e);
    }
    if (out.empty()) return false;
    notes = out;
    return true;
}
} // namespace cooltone
