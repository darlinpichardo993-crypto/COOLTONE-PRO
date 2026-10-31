// COOLTONE by ORYMY - YMY RECORDS - engine tests: PSOLA, correction, vibrato styles, Magic Vibrato, melisma, expression, export
#include "TestHarness.h"
#include "AudioEngine.h"
#include "DSPUtilities.h"
#include "MelismaEngine.h"
#include "MidiExport.h"
#include "PresetManager.h"
#include "VibratoEngine.h"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <set>

using namespace cooltone;

namespace
{
std::vector<float> vowel (double sr, double dur, const std::function<double (double)>& f0, double amp = 0.4, double noise = 0.0, std::uint32_t seed = 1)
{
    const std::size_t n = static_cast<std::size_t> (sr * dur);
    std::vector<float> y (n);
    double ph = 0; std::uint32_t r = seed;
    for (std::size_t i = 0; i < n; ++i)
    {
        const double t = static_cast<double> (i) / sr, hz = f0 (t);
        ph += 2 * kPi * hz / sr;
        double s = 0, norm = 0;
        for (int h = 1; h * hz < 4500 && h < 40; ++h)
        {
            const double fh = h * hz;
            const double a = (1.0 / h) * (std::exp (-std::pow ((fh - 700) / 250, 2)) + 0.7 * std::exp (-std::pow ((fh - 1200) / 300, 2)) + 0.08);
            s += a * std::sin (h * ph); norm += a;
        }
        const double fade = std::min (1.0, std::min (t / 0.02, (dur - t) / 0.02));
        r = r * 1664525u + 1013904223u;
        const double nz = noise * (static_cast<double> (r >> 8) / 8388608.0 - 1.0);
        y[i] = static_cast<float> (amp * fade * (s / std::max (norm, 1e-9)) + nz);
    }
    return y;
}
struct Seg { double midi, dur, gap; };
AudioClip melody (double sr, const std::vector<Seg>& segs, double detuneCents = 0.0)
{
    AudioClip c; c.sampleRate = static_cast<int> (sr); c.ch.assign (1, {});
    for (const auto& s : segs)
    {
        auto v = vowel (sr, s.dur, [&] (double) { return midiToHz (s.midi + detuneCents / 100.0); });
        c.ch[0].insert (c.ch[0].end(), v.begin(), v.end());
        c.ch[0].insert (c.ch[0].end(), static_cast<std::size_t> (s.gap * sr), 0.f);
    }
    return c;
}
double medianPitchCents (const std::vector<float>& x, double sr, double refHz, double t0 = 0.2, double t1 = 1e9, double* stdOut = nullptr, double* meanOut = nullptr)
{
    YinParams p; p.sampleRate = sr;
    auto fr = PitchDetector::analyse (x.data(), x.size(), p);
    std::vector<double> v;
    for (const auto& f : fr) if (f.voiced && f.timeSec >= t0 && f.timeSec <= t1) v.push_back (centsBetween (f.f0Hz, refHz));
    if (v.empty()) return 1e9;
    double m = 0; for (double d : v) m += d; m /= static_cast<double> (v.size());
    double var = 0; for (double d : v) var += (d - m) * (d - m);
    if (stdOut) *stdOut = std::sqrt (var / static_cast<double> (v.size()));
    if (meanOut) *meanOut = m;
    std::sort (v.begin(), v.end());
    return v[v.size() / 2];
}
double centroid (const std::vector<float>& x, double sr, std::size_t start)
{
    const std::size_t N = 8192; double num = 0, den = 0;
    for (std::size_t k = 1; k < N / 2; ++k)
    {
        const double f = static_cast<double> (k) * sr / N; if (f > 5000) break;
        double re = 0, im = 0;
        for (std::size_t n = 0; n < N; ++n) { const double w = 0.5 - 0.5 * std::cos (2 * kPi * static_cast<double> (n) / N); const double a = 2 * kPi * static_cast<double> (k) * static_cast<double> (n) / N; re += w * x[start + n] * std::cos (a); im -= w * x[start + n] * std::sin (a); }
        const double mag = std::sqrt (re * re + im * im); num += f * mag; den += mag;
    }
    return num / den;
}
double rmsOf (const std::vector<double>& v, std::size_t a, std::size_t b) { double e = 0; for (std::size_t i = a; i < b; ++i) e += v[i] * v[i]; return std::sqrt (e / static_cast<double> (std::max<std::size_t> (1, b - a))); }
double peakOf (const std::vector<double>& v) { double m = 0; for (double x : v) m = std::max (m, std::abs (x)); return m; }
double zcRate (const std::vector<double>& v, double hop, std::size_t a) { int zc = 0; for (std::size_t i = a + 1; i < v.size(); ++i) if ((v[i - 1] < 0) != (v[i] < 0)) ++zc; return 0.5 * zc / (static_cast<double> (v.size() - a) * hop); }
}

// ---------------- PSOLA / formants ----------------
static void shiftTest (double semis, double f0, double sr)
{
    auto x = vowel (sr, 1.5, [&] (double) { return f0; });
    YinParams p; p.sampleRate = sr;
    auto fr = PitchDetector::analyse (x.data(), x.size(), p);
    const double hop = static_cast<double> (PitchDetector::hopSamples (p)) / sr;
    std::vector<double> ratio (fr.size(), std::pow (2.0, semis / 12.0)), formant (fr.size(), 1.0);
    std::vector<std::vector<float>> out;
    psolaProcess ({ x }, sr, fr, hop, ratio, formant, out);
    CHECK (out.size() == 1 && out[0].size() == x.size());
    CHECK_NEAR (medianPitchCents (out[0], sr, f0 * std::pow (2.0, semis / 12.0)), 0.0, 12.0);
}
TEST (psola_shift_up_3st_44k)   { shiftTest (3.0, 200.0, 44100); }
TEST (psola_shift_down_4st_48k) { shiftTest (-4.0, 180.0, 48000); }
TEST (psola_shift_up_5st_96k)   { shiftTest (5.0, 160.0, 96000); }
TEST (psola_identity_is_bit_exact)
{
    const double sr = 44100;
    auto x = vowel (sr, 1.0, [] (double) { return 200.0; });
    YinParams p; p.sampleRate = sr;
    auto fr = PitchDetector::analyse (x.data(), x.size(), p);
    std::vector<double> one (fr.size(), 1.0);
    std::vector<std::vector<float>> out;
    psolaProcess ({ x }, sr, fr, 0.01, one, one, out);
    CHECK (out[0] == x);
}
TEST (psola_formants_are_preserved_when_asked)
{
    const double sr = 44100;
    auto x = vowel (sr, 1.5, [] (double) { return 150.0; });
    YinParams p; p.sampleRate = sr;
    auto fr = PitchDetector::analyse (x.data(), x.size(), p);
    const double r = std::pow (2.0, 4.0 / 12.0);
    std::vector<double> ratio (fr.size(), r), keep (fr.size(), 1.0), follow (fr.size(), r);
    std::vector<std::vector<float>> a, b;
    psolaProcess ({ x }, sr, fr, 0.01, ratio, keep, a);
    psolaProcess ({ x }, sr, fr, 0.01, ratio, follow, b);
    const double c0 = centroid (x, sr, 10000), ca = centroid (a[0], sr, 10000), cb = centroid (b[0], sr, 10000);
    CHECK (ca / c0 < 1.12);          // formants stay put
    CHECK (cb / c0 > 1.15);          // formants follow the pitch (like a naive resampler)
    CHECK (ca < cb);
}

// ---------------- vibrato engine ----------------
static std::vector<double> vib (int style, std::uint32_t seed = 5, double dur = 3.0)
{
    VibratoParams p; p.style = style; p.rateHz = 5.5; p.depthCents = 40; p.delaySec = 0.2; p.attackSec = 0.2; p.releaseSec = 0.2;
    return generateVibratoCents (dur, 0.01, p, seed);
}
TEST (vibrato_eight_styles_are_really_different)
{
    std::vector<std::vector<double>> c; for (int s = 0; s < 8; ++s) c.push_back (vib (s));
    for (int a = 0; a < 8; ++a) for (int b = a + 1; b < 8; ++b)
    {
        double d = 0; for (std::size_t i = 0; i < c[a].size(); ++i) d += (c[a][i] - c[b][i]) * (c[a][i] - c[b][i]);
        CHECK (std::sqrt (d / static_cast<double> (c[a].size())) > 1.0);   // > 1 cent rms apart
    }
    CHECK (zcRate (c[VibSpeed], 0.01, 60) > 1.4 * zcRate (c[VibSlow], 0.01, 60));
    CHECK (peakOf (c[VibWide]) > 2.0 * peakOf (c[VibTight]));
    CHECK (rmsOf (c[VibEmotional], 200, 300) > 1.8 * rmsOf (c[VibEmotional], 30, 100));   // swells over the note
    // natural: cycle peaks vary more than tight
    auto cyc = [] (const std::vector<double>& v) { std::vector<double> pk; double m = 0; for (std::size_t i = 40; i + 1 < v.size() - 40; ++i) { m = std::max (m, std::abs (v[i])); if (v[i] * v[i + 1] < 0) { if (m > 0) pk.push_back (m); m = 0; } }
        double mean = 0; for (double x : pk) mean += x; mean /= std::max<std::size_t> (1, pk.size()); double var = 0; for (double x : pk) var += (x - mean) * (x - mean);
        return std::sqrt (var / std::max<std::size_t> (1, pk.size())) / std::max (mean, 1e-9); };
    CHECK (cyc (c[VibNatural]) > 1.5 * cyc (c[VibTight]));
}
TEST (vibrato_is_centred_deterministic_and_respects_delay)
{
    for (int s = 0; s < 8; ++s)
    {
        auto c = vib (s);
        double m = 0; for (double x : c) m += x; m /= static_cast<double> (c.size());
        CHECK (std::abs (m) < 0.25 * peakOf (c));      // oscillates AROUND the note
        CHECK (peakOf (c) <= 130.0);
    }
    CHECK (vib (0, 5) == vib (0, 5));
    CHECK (vib (0, 5) != vib (0, 6));
    auto c = vib (3);
    CHECK (rmsOf (c, 0, 15) < 1.0);                     // silent during the delay
    VibratoParams p; p.rateHz = 6.0; p.depthCents = 50; p.delaySec = 0; p.attackSec = 0; p.releaseSec = 0; p.randomness = 0; p.humanize = 0; p.variation = 0; p.smoothness = 0;
    auto f = generateVibratoCents (3.0, 0.01, p, 1);
    CHECK_NEAR (zcRate (f, 0.01, 0), 6.0, 0.4);
    CHECK_NEAR (peakOf (f), 50.0, 3.0);
}

// ---------------- full render: Auto Tune, vibrato, independence ----------------
static Settings tuneSettings()
{
    Settings s; s.set (pCorrectionOn, 1); s.set (pRetune, 0); s.set (pHumanize, 0); s.set (pDrift, 0); s.set (pStrength, 100); s.set (pTransition, 0);
    return s;
}
TEST (render_with_defaults_is_bit_exact)
{
    auto clip = melody (44100, { { 57, 1.0, 0.1 }, { 60, 1.0, 0.0 } }, 30.0);
    auto an = analyseClip (clip);
    auto out = renderProject (clip, an, an.notes, Settings());
    CHECK (out.ch == clip.ch);
}
TEST (autotune_corrects_detuned_voice_to_scale)
{
    const double sr = 44100;
    auto clip = melody (sr, { { 57, 1.5, 0.1 }, { 60, 1.5, 0.0 } }, -39.0);    // A3 and C4, both 39 cents flat
    auto an = analyseClip (clip);
    CHECK (an.notes.size() == 2);
    CHECK_NEAR (centsBetween (an.notes[0].note.midiPitch >= 0 ? midiToHz (an.notes[0].note.midiPitch) : 0, 220.0), -39.0, 6.0);
    Settings s = tuneSettings();
    auto out = renderProject (clip, an, an.notes, s);
    CHECK_NEAR (medianPitchCents (out.ch[0], sr, 220.0, 0.3, 1.5), 0.0, 10.0);
    CHECK_NEAR (medianPitchCents (out.ch[0], sr, midiToHz (60), 1.9, 3.0), 0.0, 10.0);
    CHECK (out.ch[0].size() == clip.ch[0].size());            // duration / sync untouched
}
TEST (autotune_snaps_out_of_scale_notes_and_flex_range_work)
{
    const double sr = 44100;
    auto clip = melody (sr, { { 61.0, 1.5, 0.0 } });          // C#4 is not in C major
    auto an = analyseClip (clip);
    Settings s = tuneSettings();
    auto out = renderProject (clip, an, an.notes, s);
    const double c = medianPitchCents (out.ch[0], sr, midiToHz (60), 0.3, 1.4), d = medianPitchCents (out.ch[0], sr, midiToHz (62), 0.3, 1.4);
    CHECK (std::abs (c) < 15.0 || std::abs (d) < 15.0);       // landed on C4 or D4 (exact tie may go either way)
    Settings r = s; r.set (pRange, 50);                       // leap of 100 cents > range 50: treated as intentional
    auto keep = renderProject (clip, an, an.notes, r);
    CHECK_NEAR (medianPitchCents (keep.ch[0], sr, midiToHz (61), 0.3, 1.4), 0.0, 10.0);
    Settings f = s; f.set (pStrength, 100); f.set (pFlex, 50); // already within flex? 100 cents away -> still corrected
    auto flex = renderProject (clip, an, an.notes, f);
    CHECK (std::abs (medianPitchCents (flex.ch[0], sr, midiToHz (61), 0.3, 1.4)) > 40.0);
}
TEST (magic_vibrato_oscillates_around_the_note_without_autotune)
{
    const double sr = 44100;
    auto clip = melody (sr, { { 57, 2.5, 0.0 } });
    auto an = analyseClip (clip);
    auto notes = an.notes; CHECK (notes.size() == 1);
    notes[0].edit.vibrato = true; notes[0].edit.vibSeed = 9;
    Settings s; s.set (pVibStyle, VibWide); s.set (pVibDepth, 45); s.set (pVibDelay, 100); s.set (pVibAttack, 150);   // Auto Tune stays OFF
    auto out = renderProject (clip, an, notes, s);
    double sd = 0, mean = 0;
    medianPitchCents (out.ch[0], sr, 220.0, 0.8, 2.2, &sd, &mean);
    CHECK_NEAR (mean, 0.0, 8.0);        // centre unchanged
    CHECK (sd > 20.0);                  // clearly modulated
}
TEST (autotune_and_vibrato_stack_without_drift_or_double_modulation)
{
    const double sr = 44100;
    auto clip = melody (sr, { { 57, 2.5, 0.0 } }, -35.0);
    auto an = analyseClip (clip);
    auto notes = an.notes; notes[0].edit.vibrato = true; notes[0].edit.vibSeed = 3;
    Settings s = tuneSettings(); s.set (pVibStyle, VibBachata); s.set (pVibDepth, 40);
    auto out = renderProject (clip, an, notes, s);
    double sd = 0, mean = 0;
    medianPitchCents (out.ch[0], sr, 220.0, 0.9, 2.2, &sd, &mean);
    CHECK_NEAR (mean, 0.0, 10.0);       // vibrato is centred on the CORRECTED pitch (not on the -35 cents of the take)
    CHECK (sd > 12.0 && sd < 45.0);
}
TEST (original_vibrato_can_be_kept_or_replaced)
{
    const double sr = 44100;
    auto x = vowel (sr, 2.5, [] (double t) { return 220.0 * std::pow (2.0, (30.0 / 1200.0) * std::sin (2 * kPi * 5.5 * t)); });
    AudioClip clip; clip.sampleRate = 44100; clip.ch = { x };
    auto an = analyseClip (clip);
    auto notes = an.notes; CHECK (notes.size() == 1);
    double sdIn = 0; medianPitchCents (x, sr, 220.0, 0.5, 2.0, &sdIn);
    Settings s; s.set (pVibStyle, VibTight); s.set (pVibDepth, 20);
    notes[0].edit.vibrato = true; notes[0].edit.vibSeed = 4;
    s.set (pVibKeepOrig, 0);
    auto rep = renderProject (clip, an, notes, s);
    double sdRep = 0; medianPitchCents (rep.ch[0], sr, 220.0, 0.8, 2.0, &sdRep);
    CHECK (sdRep < 0.6 * sdIn);         // singer's vibrato flattened and replaced by a tight one
    s.set (pVibKeepOrig, 1);
    auto keep = renderProject (clip, an, notes, s);
    double sdKeep = 0; medianPitchCents (keep.ch[0], sr, 220.0, 0.8, 2.0, &sdKeep);
    CHECK (sdKeep > 0.9 * sdIn);
}
TEST (manual_target_and_transpose_and_delete)
{
    const double sr = 44100;
    auto clip = melody (sr, { { 57, 1.2, 0.1 }, { 60, 1.2, 0.0 } });
    auto an = analyseClip (clip);
    auto notes = an.notes; CHECK (notes.size() == 2);
    notes[0].edit.hasTarget = true; notes[0].edit.targetMidi = 59.0;        // drag A3 up to B3
    Settings s; s.set (pRetune, 0);
    auto out = renderProject (clip, an, notes, s);
    CHECK_NEAR (medianPitchCents (out.ch[0], sr, midiToHz (59), 0.3, 1.1), 0.0, 12.0);
    CHECK_NEAR (medianPitchCents (out.ch[0], sr, midiToHz (60), 1.6, 2.3), 0.0, 12.0);   // other note untouched
    Settings t; t.set (pTranspose, 2); t.set (pRetune, 0);
    auto tr = renderProject (clip, an, an.notes, t);
    CHECK_NEAR (medianPitchCents (tr.ch[0], sr, midiToHz (59), 0.3, 1.1), 0.0, 12.0);
    notes[0].edit.deleted = true;
    auto del = renderProject (clip, an, notes, Settings());
    double e = 0; for (std::size_t n = static_cast<std::size_t> (0.3 * sr); n < static_cast<std::size_t> (1.1 * sr); ++n) e += del.ch[0][n] * del.ch[0][n];
    CHECK (e < 1e-6);
}

// ---------------- Magic Vibrato intelligence ----------------
TEST (magic_vibrato_picks_the_right_notes)
{
    const double sr = 44100;
    auto clip = melody (sr, { { 57, 2.0, 0.3 }, { 60, 0.2, 0.3 }, { 62, 1.4, 0.4 } });
    auto an = analyseClip (clip);
    CHECK (an.notes.size() == 3);
    MagicSettings ms; ms.level = Intelligence::Natural;
    auto d = analyseForVibrato (an.notes, an.frames, an.hop, ms);
    CHECK (d[0].apply);       // long note at a phrase end
    CHECK (! d[1].apply);     // 200 ms note: no vibrato
    CHECK (d[2].apply);
    int counts[5];
    for (int l = 0; l < 5; ++l) { MagicSettings m2; m2.level = static_cast<Intelligence> (l); int c = 0; for (const auto& x : analyseForVibrato (an.notes, an.frames, an.hop, m2)) c += x.apply; counts[l] = c; }
    CHECK (counts[0] <= counts[1] && counts[1] <= counts[2] && counts[2] <= counts[4]);
    CHECK (counts[4] >= 2);
    // an existing natural vibrato is never doubled
    auto x = vowel (sr, 2.0, [] (double t) { return 220.0 * std::pow (2.0, (30.0 / 1200.0) * std::sin (2 * kPi * 5.5 * t)); });
    AudioClip c2; c2.sampleRate = 44100; c2.ch = { x };
    auto a2 = analyseClip (c2);
    CHECK (a2.notes.size() == 1 && noteHasNaturalVibrato (a2.notes[0], a2.frames, a2.hop));
    CHECK (! analyseForVibrato (a2.notes, a2.frames, a2.hop, ms)[0].apply);
    double rate = 0, depth = 0; CHECK (measureNaturalVibrato (a2.notes[0], a2.frames, a2.hop, &rate, &depth));
    CHECK_NEAR (rate, 5.5, 0.8); CHECK_NEAR (depth, 30.0, 12.0);
}
TEST (run_vibrato_only_on_run_endings)
{
    const double sr = 44100;
    std::vector<Seg> segs;
    for (double m : { 57.0, 59.0, 60.0, 62.0, 60.0 }) segs.push_back ({ m, 0.14, 0.0 });   // a run of five short notes
    segs.push_back ({ 64.0, 1.0, 0.5 });                                                    // it lands on a long note
    segs.push_back ({ 57.0, 1.2, 0.0 });                                                    // an unrelated sustained note
    auto clip = melody (sr, segs);
    auto an = analyseClip (clip);
    CHECK (an.notes.size() >= 7);
    auto ends = findRunEndings (an.notes);
    CHECK (ends.size() == 1);
    MagicSettings ms; ms.style = VibRun;
    auto d = analyseForVibrato (an.notes, an.frames, an.hop, ms);
    int applied = 0; for (const auto& x : d) applied += x.apply;
    CHECK (applied == 1);
    if (! ends.empty()) { CHECK (d[static_cast<std::size_t> (ends[0])].apply); CHECK (d[static_cast<std::size_t> (ends[0])].start > 0.3); }
    auto notes = an.notes;
    CHECK (applyMagicVibrato (notes, an.frames, an.hop, ms, {}, false, 11) == 1);
}

// ---------------- melisma ----------------
TEST (melisma_respects_scale_and_shape)
{
    const std::uint16_t cMajor = 0b101010110101;
    MelismaParams p; p.numNotes = 5; p.mode = 0; p.startDegree = 0; p.endDegree = 4; p.lengthFrac = 0.6; p.humanize = 0.0;
    auto a = generateMelisma (1.0, 3.0, 57.0, cMajor, p, 1);
    CHECK (a.size() == 5);
    for (const auto& s : a) CHECK ((cMajor >> (static_cast<int> (s.midi) % 12)) & 1);
    for (std::size_t i = 1; i < a.size(); ++i) CHECK (a[i].midi >= a[i - 1].midi);
    CHECK_NEAR (a.back().midi, 64.0, 1e-9);                       // A B C D E
    CHECK_NEAR (a.front().startSec, 1.0, 1e-9); CHECK_NEAR (a.back().endSec, 3.0, 1e-9);
    for (std::size_t i = 1; i < a.size(); ++i) CHECK_NEAR (a[i].startSec, a[i - 1].endSec, 1e-9);
    p.mode = 1; p.startDegree = 0; p.endDegree = 4;
    auto d = generateMelisma (1.0, 3.0, 57.0, cMajor, p, 1);
    for (std::size_t i = 2; i < d.size() - 1; ++i) CHECK (d[i].midi <= d[i - 1].midi);
    for (int mode = 0; mode < 7; ++mode) { p.mode = mode; for (const auto& s : generateMelisma (0, 2, 60.0, cMajor, p, 3)) CHECK ((cMajor >> (static_cast<int> (s.midi) % 12)) & 1); }
    CHECK (generateMelisma (0, 0.1, 60.0, cMajor, p, 1).empty());   // too short for a run
}
TEST (melisma_changes_the_rendered_audio)
{
    const double sr = 44100;
    auto clip = melody (sr, { { 57, 2.4, 0.0 } });
    auto an = analyseClip (clip);
    auto notes = an.notes;
    Settings s; s.set (pRetune, 0);
    MelismaParams p; p.numNotes = 4; p.mode = 0; p.endDegree = 3; p.lengthFrac = 0.6; p.humanize = 0;
    notes[0].edit.melisma = generateMelisma (notes[0].note.startSec, notes[0].note.endSec, 57.0, s.scaleMask(), p, 1);
    CHECK (notes[0].edit.melisma.size() == 4);
    auto out = renderProject (clip, an, notes, s);
    auto an2 = analyseClip ([&] { AudioClip c; c.sampleRate = 44100; c.ch = out.ch; return c; }());
    CHECK (an2.notes.size() >= 4);
}

// ---------------- voice expression ----------------
TEST (expression_breath_is_real_dsp)
{
    const double sr = 44100;
    AudioClip clip; clip.sampleRate = 44100; clip.ch = { vowel (sr, 2.0, [] (double) { return 200.0; }, 0.4, 0.04) };
    auto an = analyseClip (clip);
    auto base = renderProject (clip, an, an.notes, Settings());
    Settings lo; lo.set (pBreath, 0);
    Settings hi; hi.set (pBreath, 200);
    auto a = renderProject (clip, an, an.notes, lo), b = renderProject (clip, an, an.notes, hi);
    auto energy = [] (const std::vector<float>& x) { double e = 0; for (std::size_t n = 20000; n < 70000; ++n) e += static_cast<double> (x[n]) * x[n]; return e; };
    CHECK (energy (a.ch[0]) < 0.97 * energy (base.ch[0]));
    CHECK (energy (b.ch[0]) > 1.02 * energy (base.ch[0]));
    // the tonal body survives: pitch is unchanged
    CHECK_NEAR (medianPitchCents (a.ch[0], sr, 200.0), 0.0, 5.0);
    Settings pr; pr.set (pPresence, 6);
    CHECK (energy (renderProject (clip, an, an.notes, pr).ch[0]) > energy (base.ch[0]));
    Settings dyn; dyn.set (pDynamics, 40);
    CHECK (renderProject (clip, an, an.notes, dyn).ch[0] != base.ch[0]);
}

// ---------------- model / presets / export ----------------
TEST (params_are_consistent)
{
    const auto& sp = paramSpecs();
    CHECK (sp.size() == static_cast<std::size_t> (pCount));
    std::set<std::string> ids;
    for (const auto& s : sp) { ids.insert (s.id); CHECK (s.defV >= s.minV && s.defV <= s.maxV); CHECK (s.maxV > s.minV); if (s.kind == ParamKind::Choice) CHECK (static_cast<std::size_t> (s.maxV) + 1 == s.choices.size()); }
    CHECK (ids.size() == sp.size());
    CHECK (ScaleEngine::scaleCount() == static_cast<int> (paramSpecs()[pScale].choices.size()));
}
TEST (presets_cover_all_categories_and_stay_in_range)
{
    CHECK (stylePresetCount() >= 24);
    std::set<int> cats;
    for (int i = 0; i < stylePresetCount(); ++i)
    {
        Settings s; applyStylePreset (s, i); cats.insert (stylePresetCategory (i));
        for (int p = 0; p < pCount; ++p) CHECK (s.v[static_cast<std::size_t> (p)] >= paramSpecs()[static_cast<std::size_t> (p)].minV && s.v[static_cast<std::size_t> (p)] <= paramSpecs()[static_cast<std::size_t> (p)].maxV);
    }
    CHECK (static_cast<int> (cats.size()) == presetCategoryCount());
    Settings a, b; applyStylePreset (a, 0); applyStylePreset (b, 8);
    CHECK (a.v != b.v);                       // presets are real parameter sets, not labels
    bool anyArtist = false; for (int i = 0; i < stylePresetCount(); ++i) anyArtist = anyArtist || std::string (stylePresetName (i)).find ("ROMEO") == 0;
    CHECK (anyArtist);
}
TEST (settings_serialisation_round_trip)
{
    Settings s; applyStylePreset (s, 3); s.set (pKeyRoot, 7); s.set (pScale, 8); applyScaleToNoteParams (s); s.set (pCenterHz, 432.5);
    Settings r; CHECK (parseSettings (serializeSettings (s), r));
    for (int i = 0; i < pCount; ++i) CHECK_NEAR (r.v[static_cast<std::size_t> (i)], s.v[static_cast<std::size_t> (i)], 1e-5);
    Settings junk; CHECK (! parseSettings ("garbage", junk));
    Settings g; g.set (pKeyRoot, 7); g.set (pScale, static_cast<int> (ScaleType::MajorBlues)); applyScaleToNoteParams (g);
    CHECK (g.scaleMask() == ((1u << 7) | (1u << 9) | (1u << 10) | (1u << 11) | (1u << 2) | (1u << 4)));   // G major blues: G A Bb B D E
}
TEST (edits_split_glue_and_serialise)
{
    const double sr = 44100;
    auto clip = melody (sr, { { 57, 2.0, 0.0 } });
    auto an = analyseClip (clip);
    auto notes = an.notes;
    CHECK (splitNote (notes, an.frames, an.hop, 0, 1.0) && notes.size() == 2);
    CHECK (! splitNote (notes, an.frames, an.hop, 0, 0.01));
    notes[0].edit.hasTarget = true; notes[0].edit.targetMidi = 58; notes[1].edit.vibrato = true; notes[1].edit.vibStart = 0.3; notes[1].edit.vibDepthScale = 1.4;
    notes[1].edit.melisma.push_back ({ 1.0, 1.5, 60.0 });
    std::vector<EditNote> back;
    CHECK (parseEdits (serializeEdits (notes), back, an.frames.size(), an.frames, an.hop));
    CHECK (back.size() == 2 && back[0].edit.hasTarget && back[1].edit.vibrato && back[1].edit.melisma.size() == 1);
    CHECK_NEAR (back[1].edit.vibStart, 0.3, 1e-3); CHECK_NEAR (back[1].edit.vibDepthScale, 1.4, 1e-3);
    CHECK (glueNotes (notes, an.frames, an.hop, 0) && notes.size() == 1);
    CHECK_NEAR (notes[0].note.startSec, an.notes[0].note.startSec, 1e-9);
    CHECK (findNoteAt (notes, 0.5) == 0 && findNoteAt (notes, 5.0) == -1);
}
TEST (voice_analyzer_report_and_key_detection)
{
    const double sr = 44100;
    std::vector<Seg> segs;
    for (double m : { 62.0, 64.0, 66.0, 67.0, 69.0, 71.0, 73.0, 74.0, 62.0, 66.0, 69.0, 62.0 }) segs.push_back ({ m, 0.45, 0.08 });   // D major
    auto clip = melody (sr, segs, 20.0);
    auto an = analyseClip (clip);
    auto rep = buildVoiceReport (clip, an, an.notes);
    CHECK (rep.numNotes == 12);
    CHECK (rep.key.root == 2 && ! rep.keyMinor);
    CHECK (rep.key.correlation > 0.6);
    CHECK_NEAR (rep.avgAbsDeviationCents, 20.0, 6.0);
    CHECK (rep.voicedFraction > 0.7 && rep.f0MinHz > 140 && rep.f0MaxHz < 600);
    CHECK_NEAR (noteDeviationCents (an.notes[0]), 20.0, 6.0);
    Settings s; s.set (pKeyRoot, rep.key.root); s.set (pScale, 0); applyScaleToNoteParams (s);
    CHECK (s.scaleMask() == ((1u << 2) | (1u << 4) | (1u << 6) | (1u << 7) | (1u << 9) | (1u << 11) | (1u << 1)));
}
TEST (export_wav_keeps_position_duration_and_content)
{
    const double sr = 48000;
    std::vector<std::vector<float>> ch (2, std::vector<float> (static_cast<std::size_t> (sr * 2.0), 0.f));
    for (std::size_t n = 0; n < ch[0].size(); ++n) { ch[0][n] = static_cast<float> (0.5 * std::sin (0.05 * static_cast<double> (n))); ch[1][n] = -ch[0][n]; }
    ExportOptions o; o.format = WavFormat::Float32;
    auto full = buildExport (ch, 48000, o);
    CHECK (full.channels == 2 && full.frames() == ch[0].size() && full.sampleRate == 48000);
    CHECK (full.samples[2 * 1000] == ch[0][1000] && full.samples[2 * 1000 + 1] == ch[1][1000]);   // exactly what was rendered
    o.selectionOnly = true; o.selStartSec = 0.5; o.selEndSec = 1.0;
    auto sel = buildExport (ch, 48000, o);
    CHECK (sel.frames() == ch[0].size());                      // same length -> same start position in the playlist
    CHECK (sel.samples[2 * 100] == 0.f && sel.samples[2 * 60000] == 0.f);
    CHECK (std::abs (sel.samples[2 * 36000]) > 0.f);           // inside the selection (0.75 s)
    CHECK (sel.samples[2 * 36000] == ch[0][36000]);
    const std::string path = (std::filesystem::temp_directory_path() / "ct_export.wav").string();
    CHECK (writeWav (path, full)); WavData r; CHECK (readWav (path, r)); CHECK (r.samples == full.samples);
    std::filesystem::remove (path);
}
TEST (midi_export_has_correct_notes_and_timing)
{
    const double sr = 44100;
    auto clip = melody (sr, { { 57, 1.0, 0.2 }, { 60, 0.5, 0.0 }, { 64, 1.0, 0.1 } });
    auto an = analyseClip (clip);
    Settings s; s.set (pCorrectionOn, 1);
    auto f = buildMidiFile (an.notes, s, 120.0);
    CHECK (f.size() > 30 && f[0] == 'M' && f[1] == 'T' && f[2] == 'h' && f[3] == 'd');
    CHECK (f[9] == 0 && f[11] == 1);                            // format 0, one track
    CHECK ((f[12] << 8 | f[13]) == 480);
    std::size_t pos = 14 + 8 + 0; // skip MTrk header
    std::vector<int> ons; long tick = 0, firstOn = -1, firstOff = -1;
    pos = 22;
    while (pos + 3 < f.size())
    {
        unsigned d = 0; unsigned char b; do { b = f[pos++]; d = (d << 7) | (b & 0x7F); } while (b & 0x80);
        tick += d;
        const unsigned char st = f[pos++];
        if (st == 0xFF) { const unsigned char type = f[pos++]; const unsigned len = f[pos++]; pos += len; if (type == 0x2F) break; continue; }
        const unsigned char p = f[pos++], v = f[pos++];
        if ((st & 0xF0) == 0x90 && v > 0) { ons.push_back (p); if (firstOn < 0) firstOn = tick; }
        if ((st & 0xF0) == 0x80 && firstOff < 0) firstOff = tick;
    }
    CHECK (ons == std::vector<int> ({ 57, 60, 64 }));
    CHECK_NEAR (static_cast<double> (firstOff - firstOn), 1.0 * 480 * 120.0 / 60.0, 40.0);    // ~1 s at 120 bpm
    std::string err; const std::string path = (std::filesystem::temp_directory_path() / "ct_notes.mid").string();
    CHECK (writeMidiFile (path, an.notes, s, 120.0, &err)); CHECK (std::filesystem::file_size (path) == f.size());
    std::filesystem::remove (path);
}
