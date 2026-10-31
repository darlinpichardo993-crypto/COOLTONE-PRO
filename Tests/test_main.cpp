// COOLTONE by ORYMY - YMY RECORDS - Core unit tests (no JUCE required)
#include "TestHarness.h"
#include "DSPUtilities.h"
#include "NoteSegmenter.h"
#include "PitchDetection.h"
#include "ScaleEngine.h"
#include "WavIO.h"
#include <algorithm>
#include <cstdint>
#include <filesystem>

using namespace cooltone;

// ---------- helpers ----------
static std::vector<float> synth (double sr, double dur, const std::function<double (double)>& f0,
                                 const std::vector<double>& harmonicGains = { 1.0 }, double amp = 0.5)
{
    const std::size_t n = static_cast<std::size_t> (sr * dur);
    std::vector<float> y (n);
    double phase = 0.0, norm = 0.0;
    for (double g : harmonicGains) norm += std::fabs (g);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double t = static_cast<double> (i) / sr;
        const double fade = std::min (1.0, std::min (t / 0.01, (dur - t) / 0.01));
        const double hz = f0 (t);
        phase += 2.0 * kPi * hz / sr;
        double s = 0.0;
        for (std::size_t h = 0; h < harmonicGains.size(); ++h) s += harmonicGains[h] * std::sin (phase * static_cast<double> (h + 1));
        y[i] = static_cast<float> (amp * fade * s / norm);
    }
    return y;
}
static double medianVoicedHz (const std::vector<PitchFrame>& fr, double& voicedFraction)
{
    std::vector<double> v;
    for (const auto& f : fr) if (f.voiced) v.push_back (f.f0Hz);
    voicedFraction = fr.empty() ? 0.0 : static_cast<double> (v.size()) / static_cast<double> (fr.size());
    if (v.empty()) return 0.0;
    std::sort (v.begin(), v.end());
    return v[v.size() / 2];
}

// ---------- ScaleEngine ----------
TEST (scale_note_counts)
{
    CHECK (ScaleEngine::intervals (ScaleType::Major).size() == 7);
    CHECK (ScaleEngine::intervals (ScaleType::MajorPentatonic).size() == 5);
    CHECK (ScaleEngine::intervals (ScaleType::MinorPentatonic).size() == 5);
    CHECK (ScaleEngine::intervals (ScaleType::MinorBlues).size() == 6);
    CHECK (ScaleEngine::intervals (ScaleType::Chromatic).size() == 12);
    CHECK (ScaleEngine::intervals (ScaleType::Custom).empty());
    for (int i = 0; i < ScaleEngine::scaleCount(); ++i) CHECK (ScaleEngine::scaleName (static_cast<ScaleType> (i)) != nullptr);
}
TEST (scale_c_major_mask_and_snap)
{
    ScaleEngine s; s.setKey (0); s.setScale (ScaleType::Major);
    CHECK (s.mask() == 0b101010110101); // C D E F G A B
    CHECK (s.isAllowed (60) && ! s.isAllowed (61) && s.isAllowed (62));
    CHECK_NEAR (s.nearestAllowed (61.2), 62.0, 1e-9);
    CHECK_NEAR (s.nearestAllowed (60.8), 60.0, 1e-9); // 60.8 is nearer C (0.8) than D (1.2)
    CHECK_NEAR (s.nearestAllowed (66.9), 67.0, 1e-9);
    CHECK_NEAR (s.nearestAllowed (61.0), 60.0, 1e-9); // exact tie -> lower note
}
TEST (scale_transposed_and_manual_toggle)
{
    ScaleEngine s; s.setKey (2); s.setScale (ScaleType::Major); // D major: D E F# G A B C#
    CHECK (s.isAllowed (66) && s.isAllowed (73) && ! s.isAllowed (60));
    s.setNoteEnabled (0, true);   // add C natural
    s.setNoteEnabled (6, false);  // remove F#
    CHECK (s.isAllowed (60) && ! s.isAllowed (66));
    s.setScale (ScaleType::Major); // clears overrides
    CHECK (! s.isAllowed (60) && s.isAllowed (66));
}
TEST (scale_custom_and_chromatic)
{
    ScaleEngine s; s.setScale (ScaleType::Custom); s.setCustomMask (0b000000010001); // C and E
    CHECK (s.isAllowed (60) && s.isAllowed (64) && ! s.isAllowed (62));
    CHECK_NEAR (s.nearestAllowed (62.2), 64.0, 1e-9);
    s.setScale (ScaleType::Chromatic);
    CHECK (s.mask() == 0x0FFF);
    CHECK_NEAR (s.nearestAllowed (62.4), 62.0, 1e-9);
}
TEST (scale_key_detection)
{
    static const double major[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double minor[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    for (int r = 0; r < 12; ++r)
    {
        std::array<double, 12> hM{}, hm{};
        for (int pc = 0; pc < 12; ++pc) { hM[pc] = major[(pc - r + 12) % 12]; hm[pc] = minor[(pc - r + 12) % 12]; }
        const auto a = ScaleEngine::detectKey (hM), b = ScaleEngine::detectKey (hm);
        CHECK (a.root == r && ! a.minor);
        CHECK (b.root == r && b.minor);
    }
}

// ---------- PitchDetection ----------
TEST (yin_sine_220_44k)
{
    YinParams p; p.sampleRate = 44100;
    auto x = synth (44100, 1.0, [] (double) { return 220.0; });
    auto fr = PitchDetector::analyse (x.data(), x.size(), p);
    double vf; const double hz = medianVoicedHz (fr, vf);
    CHECK (vf > 0.9);
    CHECK_NEAR (centsBetween (hz, 220.0), 0.0, 1.0);
}
TEST (yin_sine_440_96k)
{
    YinParams p; p.sampleRate = 96000;
    auto x = synth (96000, 0.6, [] (double) { return 440.0; });
    auto fr = PitchDetector::analyse (x.data(), x.size(), p);
    double vf; const double hz = medianVoicedHz (fr, vf);
    CHECK (vf > 0.85);
    CHECK_NEAR (centsBetween (hz, 440.0), 0.0, 1.0);
}
TEST (yin_weak_fundamental_no_octave_error)
{
    YinParams p; p.sampleRate = 48000;
    auto x = synth (48000, 1.0, [] (double) { return 150.0; }, { 0.3, 1.0, 0.8, 0.6, 0.4, 0.3 });
    auto fr = PitchDetector::analyse (x.data(), x.size(), p);
    double vf; const double hz = medianVoicedHz (fr, vf);
    CHECK (vf > 0.85);
    CHECK_NEAR (centsBetween (hz, 150.0), 0.0, 10.0);
}
TEST (yin_silence_and_noise_are_unvoiced)
{
    YinParams p; p.sampleRate = 44100;
    std::vector<float> sil (44100, 0.f);
    auto a = PitchDetector::analyse (sil.data(), sil.size(), p);
    double vf; medianVoicedHz (a, vf);
    CHECK (! a.empty() && vf == 0.0);

    std::vector<float> nz (44100); std::uint32_t s = 12345;
    for (auto& v : nz) { s = s * 1664525u + 1013904223u; v = 0.3f * (static_cast<float> (s >> 8) / 8388608.0f - 1.0f); }
    auto b = PitchDetector::analyse (nz.data(), nz.size(), p);
    medianVoicedHz (b, vf);
    CHECK (vf < 0.1);
}

// ---------- NoteSegmenter ----------
TEST (segmenter_melody_gap_and_legato_jump)
{
    const double sr = 44100;
    // A3 (0.5s) | silence 0.15s | C4 (0.5s) -> D4 (0.5s) legato, no gap
    std::vector<float> x = synth (sr, 0.5, [] (double) { return midiToHz (57); });
    auto sil = std::vector<float> (static_cast<std::size_t> (0.15 * sr), 0.f);
    auto c4 = synth (sr, 0.5, [] (double) { return midiToHz (60); });
    auto d4 = synth (sr, 0.5, [] (double) { return midiToHz (62); });
    x.insert (x.end(), sil.begin(), sil.end());
    x.insert (x.end(), c4.begin(), c4.end());
    x.insert (x.end(), d4.begin(), d4.end());

    YinParams p; p.sampleRate = sr;
    auto fr = PitchDetector::analyse (x.data(), x.size(), p);
    auto notes = segmentNotes (fr, p.hopSeconds);
    CHECK (notes.size() == 3);
    if (notes.size() == 3)
    {
        CHECK_NEAR (notes[0].midiPitch, 57.0, 0.1);
        CHECK_NEAR (notes[1].midiPitch, 60.0, 0.1);
        CHECK_NEAR (notes[2].midiPitch, 62.0, 0.1);
        CHECK_NEAR (notes[0].startSec, 0.0, 0.06);
        CHECK_NEAR (notes[1].startSec, 0.65, 0.06);
        CHECK_NEAR (notes[2].startSec, 1.15, 0.06);
        CHECK_NEAR (notes[2].endSec, 1.65, 0.06);
    }
}
TEST (segmenter_vibrato_stays_one_note)
{
    const double sr = 44100;
    auto x = synth (sr, 1.2, [] (double t) { return 220.0 * std::pow (2.0, (30.0 / 1200.0) * std::sin (2.0 * kPi * 5.5 * t)); });
    YinParams p; p.sampleRate = sr;
    auto fr = PitchDetector::analyse (x.data(), x.size(), p);
    auto notes = segmentNotes (fr, p.hopSeconds);
    CHECK (notes.size() == 1);
    if (! notes.empty()) CHECK_NEAR (notes[0].midiPitch, 57.0, 0.15);
}

// ---------- WavIO ----------
static std::string tmpPath (const char* n) { return (std::filesystem::temp_directory_path() / n).string(); }
static void roundTrip (WavFormat fmt, int ch, int sr, double tol)
{
    WavData w; w.sampleRate = sr; w.channels = ch; w.format = fmt;
    const std::size_t frames = 4801; // odd on purpose (exercises RIFF padding for 24-bit mono)
    for (std::size_t i = 0; i < frames; ++i)
        for (int c = 0; c < ch; ++c) w.samples.push_back (static_cast<float> (0.9 * std::sin (0.01 * static_cast<double> (i) * (c + 1))));
    w.samples[0] = 1.0f; w.samples[1 % w.samples.size()] = -1.0f; // full-scale extremes
    const std::string path = tmpPath ("cooltone_test.wav");
    std::string err;
    CHECK (writeWav (path, w, &err));
    WavData r;
    CHECK (readWav (path, r, &err));
    CHECK (r.sampleRate == sr && r.channels == ch && r.format == fmt && r.frames() == frames);
    double maxErr = 0;
    for (std::size_t i = 0; i < std::min (w.samples.size(), r.samples.size()); ++i) maxErr = std::max (maxErr, std::fabs (static_cast<double> (w.samples[i]) - r.samples[i]));
    CHECK (maxErr <= tol);
    std::filesystem::remove (path);
}
TEST (wav_roundtrip_float32_exact)   { roundTrip (WavFormat::Float32, 2, 48000, 0.0); }
TEST (wav_roundtrip_pcm24_mono)      { roundTrip (WavFormat::PCM24, 1, 44100, 1.3e-7); }
TEST (wav_roundtrip_pcm24_stereo96k) { roundTrip (WavFormat::PCM24, 2, 96000, 1.3e-7); }
TEST (wav_roundtrip_pcm16)           { roundTrip (WavFormat::PCM16, 2, 44100, 3.1e-5); }
TEST (wav_rejects_truncated_file)
{
    WavData w; w.samples.assign (1000, 0.25f);
    const std::string path = tmpPath ("cooltone_trunc.wav");
    CHECK (writeWav (path, w));
    std::filesystem::resize_file (path, 300);
    WavData r; std::string err;
    CHECK (! readWav (path, r, &err));
    std::filesystem::remove (path);
}

int main()
{
    int failedTests = 0;
    for (auto& t : TestRegistry::all())
    {
        const int before = TestRegistry::failures();
        t.fn();
        const bool ok = TestRegistry::failures() == before;
        std::printf ("[%s] %s\n", ok ? " OK " : "FAIL", t.name.c_str());
        if (! ok) ++failedTests;
    }
    std::printf ("\n%zu tests, %d checks, %d failed checks\n", TestRegistry::all().size(), TestRegistry::checks(), TestRegistry::failures());
    return failedTests == 0 ? 0 : 1;
}
