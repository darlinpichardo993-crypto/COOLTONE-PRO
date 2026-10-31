#include "Params.h"
#include "ScaleEngine.h"
#include <algorithm>

namespace cooltone
{
static std::vector<ParamSpec> build()
{
    std::vector<ParamSpec> s;
    auto F = [&] (const char* id, const char* label, double mn, double mx, double df, double st, const char* unit, const char* grp)
    { s.push_back ({ id, label, ParamKind::Float, mn, mx, df, st, unit, {}, grp }); };
    auto C = [&] (const char* id, const char* label, std::vector<std::string> ch, int df, const char* grp)
    { const double mx = static_cast<double> (ch.size()) - 1.0; s.push_back ({ id, label, ParamKind::Choice, 0, mx, static_cast<double> (df), 1, "", std::move (ch), grp }); };
    auto B = [&] (const char* id, const char* label, int df, const char* grp)
    { s.push_back ({ id, label, ParamKind::Bool, 0, 1, static_cast<double> (df), 1, "", {}, grp }); };

    B ("bypass", "Bypass", 0, "Output");
    F ("mix", "Mix", 0, 100, 100, 1, "%", "Output");
    F ("outGain", "Output", -24, 12, 0, 0.1, "dB", "Output");
    F ("inGain", "Input Gain", -24, 24, 0, 0.1, "dB", "Record");

    B ("corrOn", "Correction", 0, "Pitch");
    C ("mode", "Mode", { "Natural", "Modern Pop", "Hard Tune", "Robotic", "Vintage", "Transparent" }, 0, "Pitch");
    F ("strength", "Strength", 0, 100, 100, 1, "%", "Pitch");
    F ("retune", "Retune Speed", 0, 400, 40, 1, "ms", "Pitch");
    F ("humanize", "Humanize", 0, 100, 40, 1, "%", "Pitch");
    F ("transition", "Transition", 0, 200, 30, 1, "ms", "Pitch");
    F ("drift", "Pitch Drift", 0, 100, 30, 1, "%", "Pitch");
    F ("centerHz", "Pitch Center", 415, 466, 440, 0.1, "Hz", "Pitch");
    F ("formantKeep", "Formant Keep", 0, 100, 100, 1, "%", "Pitch");
    F ("formantShift", "Formant Shift", -12, 12, 0, 0.1, "st", "Pitch");
    F ("transpose", "Transpose", -12, 12, 0, 1, "st", "Pitch");
    F ("flex", "Flex Tune", 0, 50, 0, 1, "cents", "Pitch");
    F ("range", "Correction Range", 10, 600, 600, 5, "cents", "Pitch");

    std::vector<std::string> keys, scales;
    for (int i = 0; i < 12; ++i) keys.push_back (ScaleEngine::keyName (i));
    for (int i = 0; i < ScaleEngine::scaleCount(); ++i) scales.push_back (ScaleEngine::scaleName (static_cast<ScaleType> (i)));
    C ("keyRoot", "Key", keys, 0, "Scale");
    C ("scale", "Scale", scales, 0, "Scale");
    static const char* noteIds[12] = { "note0", "note1", "note2", "note3", "note4", "note5", "note6", "note7", "note8", "note9", "note10", "note11" };
    const int cMajor[12] = { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 };
    for (int i = 0; i < 12; ++i) B (noteIds[i], ScaleEngine::keyName (i), cMajor[i], "Scale");

    C ("vibStyle", "Vibrato Style", { "Bachata", "Run", "Speed", "Slow", "Natural", "Wide", "Tight", "Emotional" }, 0, "Vibrato");
    C ("vibIntel", "Intensity", { "Minimal", "Natural", "Expressive", "Professional", "Dramatic" }, 1, "Vibrato");
    F ("vibAmount", "Mix", 0, 100, 100, 1, "%", "Vibrato");
    F ("vibRate", "Speed", 3, 9, 5.5, 0.05, "Hz", "Vibrato");
    F ("vibDepth", "Depth", 0, 100, 35, 1, "cents", "Vibrato");
    F ("vibDelay", "Delay", 0, 800, 250, 1, "ms", "Vibrato");
    F ("vibAttack", "Attack/Fade In", 0, 800, 300, 1, "ms", "Vibrato");
    F ("vibRelease", "Release/Fade Out", 0, 500, 120, 1, "ms", "Vibrato");
    F ("vibShape", "Shape", 0, 100, 0, 1, "%", "Vibrato");
    F ("vibRandom", "Randomness", 0, 100, 25, 1, "%", "Vibrato");
    F ("vibHuman", "Humanize", 0, 100, 30, 1, "%", "Vibrato");
    F ("vibSpeed", "Speed", -50, 50, 0, 1, "%", "Vibrato");
    F ("vibSmooth", "Smoothness", 0, 100, 30, 1, "%", "Vibrato");
    F ("vibVar", "Natural Variation", 0, 100, 25, 1, "%", "Vibrato");
    F ("vibPhase", "Phase", 0, 360, 0, 1, "deg", "Vibrato");
    F ("vibNoteSens", "Note Sensitivity", 0, 100, 50, 1, "%", "Vibrato");
    F ("vibDurSens", "Duration Sensitivity", 0, 100, 50, 1, "%", "Vibrato");
    F ("vibStab", "Pitch Stability", 0, 100, 50, 1, "%", "Vibrato");
    B ("vibKeep", "Keep Original Vibrato", 1, "Vibrato");

    C ("melMode", "Melisma Mode", { "Ascending", "Descending", "Wave", "Soul Run", "R&B Run", "Latin Run", "Custom" }, 0, "Melisma");
    F ("melNotes", "Number of Notes", 2, 12, 4, 1, "", "Melisma");
    F ("melLength", "Melisma Length", 10, 100, 60, 1, "%", "Melisma");
    F ("melStart", "Starting Note", -12, 12, 0, 1, "deg", "Melisma");
    F ("melEnd", "Ending Note", -12, 12, 3, 1, "deg", "Melisma");
    C ("melRhythm", "Rhythm", { "Even", "Swing", "Accelerating", "Decelerating" }, 0, "Melisma");
    F ("melHuman", "Humanization", 0, 100, 20, 1, "%", "Melisma");
    B ("melLock", "Scale Lock", 1, "Melisma");

    F ("breath", "Breath Amount", 0, 200, 100, 1, "%", "Expression");
    F ("air", "Airiness", -6, 12, 0, 0.1, "dB", "Expression");
    F ("presence", "Presence", -6, 6, 0, 0.1, "dB", "Expression");
    F ("softness", "Softness", 0, 100, 0, 1, "%", "Expression");
    F ("dynamics", "Dynamics", -50, 50, 0, 1, "%", "Expression");
    F ("attack", "Note Attack", 0, 100, 0, 1, "ms", "Expression");
    F ("release", "Note Release", 0, 300, 0, 1, "ms", "Expression");
    return s;
}

const std::vector<ParamSpec>& paramSpecs()
{
    static const std::vector<ParamSpec> specs = build();
    return specs;
}

Settings::Settings()
{
    const auto& sp = paramSpecs();
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = sp[i].defV;
}

void Settings::set (ParamId p, double x)
{
    const auto& sp = paramSpecs()[static_cast<std::size_t> (p)];
    v[static_cast<std::size_t> (p)] = std::min (sp.maxV, std::max (sp.minV, x));
}

std::uint16_t Settings::scaleMask() const
{
    unsigned m = 0;
    for (int i = 0; i < 12; ++i) if (v[static_cast<std::size_t> (pNote0 + i)] > 0.5) m |= 1u << i;
    return static_cast<std::uint16_t> (m);
}
} // namespace cooltone
