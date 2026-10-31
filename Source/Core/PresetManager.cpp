#include "PresetManager.h"
#include "ScaleEngine.h"
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace cooltone
{
void applyCorrectionMode (Settings& s, int mode)
{
    //                 strength retune human trans drift keep
    static const double t[6][6] = {
        { 85, 60, 55, 40, 50, 100 },   // Natural
        { 100, 20, 25, 25, 20, 100 },  // Modern Pop
        { 100, 0, 0, 0, 0, 100 },      // Hard Tune
        { 100, 0, 0, 0, 0, 30 },       // Robotic (formants follow the pitch jump)
        { 70, 90, 70, 60, 70, 80 },    // Vintage
        { 60, 120, 80, 50, 70, 100 }   // Transparent
    };
    if (mode < 0 || mode > 5) return;
    const double* r = t[mode];
    s.set (pStrength, r[0]); s.set (pRetune, r[1]); s.set (pHumanize, r[2]);
    s.set (pTransition, r[3]); s.set (pDrift, r[4]); s.set (pFormantKeep, r[5]);
}

void applyVibratoStyle (Settings& s, int style)
{
    //                 rate depth delay attack release shape random human smooth var
    static const double t[8][10] = {
        { 5.0, 40, 300, 450, 200, 0, 25, 35, 40, 30 },    // Bachata
        { 5.8, 45, 0, 80, 150, 0, 20, 25, 25, 20 },       // Run
        { 7.5, 22, 150, 150, 100, 25, 15, 20, 20, 15 },   // Speed
        { 4.0, 55, 350, 500, 220, 0, 20, 30, 50, 25 },    // Slow
        { 5.5, 30, 250, 300, 120, 0, 60, 60, 35, 60 },    // Natural
        { 5.0, 80, 200, 300, 120, 0, 25, 30, 30, 25 },    // Wide
        { 6.0, 14, 150, 200, 100, 0, 5, 10, 55, 5 },      // Tight
        { 5.4, 55, 300, 700, 200, 0, 30, 40, 40, 35 }     // Emotional
    };
    if (style < 0 || style > 7) return;
    const double* r = t[style];
    s.set (pVibRate, r[0]); s.set (pVibDepth, r[1]); s.set (pVibDelay, r[2]); s.set (pVibAttack, r[3]);
    s.set (pVibRelease, r[4]); s.set (pVibShape, r[5]); s.set (pVibRandom, r[6]); s.set (pVibHuman, r[7]);
    s.set (pVibSmooth, r[8]); s.set (pVibVariation, r[9]);
}

void applyScaleToNoteParams (Settings& s)
{
    const auto type = static_cast<ScaleType> (static_cast<int> (s.get (pScale)));
    if (type == ScaleType::Custom) return;
    ScaleEngine e; e.setKey (static_cast<int> (s.get (pKeyRoot))); e.setScale (type);
    for (int i = 0; i < 12; ++i) s.set (static_cast<ParamId> (pNote0 + i), ((e.mask() >> i) & 1u) ? 1.0 : 0.0);
}

void resetToDefaults (Settings& s, bool keep)
{
    Settings d;
    for (int i = 0; i < pCount; ++i)
    {
        const auto id = static_cast<ParamId> (i);
        const bool isKeyScale = (id == pKeyRoot || id == pScale || (id >= pNote0 && id <= pNote11));
        const bool isOut = (id == pBypass || id == pMix || id == pOutGain || id == pInGain || id == pCorrectionOn);
        if (keep && (isKeyScale || isOut)) continue;
        s.v[static_cast<std::size_t> (i)] = d.v[static_cast<std::size_t> (i)];
    }
}

namespace
{
struct StyleRow
{
    const char* name; int category;
    double strength, retune, human, trans, drift;
    int vStyle, vInt; double vAmt, vDepth, vRate, vDelay;
    double breath, air, pres, soft, dyn;
    int mMode; double mNotes, mLen;
};
const char* kCategories[] = { "Artist Styles", "Bachata", "R&B", "Pop", "Emotional", "Natural", "Experimental" };
// Original processing configurations inspired by general performance traits - not voice clones.
const StyleRow kStyles[] = {
    // ---- Artist Styles (0) ----
    { "ROMEO SANTOS INSPIRED",     0, 85, 45, 45, 40, 35,  0, 2, 70, 38, 5.0, 300,  135, 3, 2, 10, 15,  4, 4, 50 },
    { "LUIS MIGUEL INSPIRED",      0, 90, 55, 40, 45, 30,  3, 3, 65, 42, 4.6, 350,  100, 1, 2, 15, 25,  2, 3, 45 },
    { "MICHAEL JACKSON INSPIRED",  0, 90, 35, 35, 25, 20,  6, 3, 50, 18, 6.0, 150,  135, 3, 2,  5, 30,  3, 5, 60 },
    { "CRISTIAN CASTRO INSPIRED",  0, 85, 65, 55, 55, 45,  5, 4, 85, 70, 5.0, 250,  115, 1, 1, 20, 35,  2, 4, 50 },
    { "RAULÍN RODRÍGUEZ INSPIRED", 0, 75, 80, 70, 60, 60,  0, 2, 75, 45, 5.2, 250,  120, 1, 1, 25, 20,  5, 5, 60 },
    { "CHRIS BROWN INSPIRED",      0, 95, 25, 30, 20, 15,  1, 2, 45, 28, 5.8, 120,  140, 3, 2,  5, 15,  4, 6, 65 },
    // ---- Genres ----
    { "BACHATA CLÁSICA",           1, 70, 90, 70, 70, 65,  0, 2, 65, 38, 4.9, 300,  120, 0, 0, 30, 10,  5, 3, 50 },
    { "BACHATA MODERNA",           1, 90, 35, 40, 30, 25,  0, 2, 50, 30, 5.6, 220,  125, 2, 1, 10, 15,  5, 4, 50 },
    { "ROMANTIC BACHATA",          1, 80, 70, 60, 60, 50,  0, 2, 70, 40, 5.0, 300,  130, 2, 1, 20, 10,  5, 4, 55 },
    { "R&B",                       2, 90, 30, 35, 25, 20,  1, 2, 50, 30, 5.6, 150,  135, 2, 1, 10, 10,  4, 5, 60 },
    { "SOUL",                      2, 75, 70, 65, 60, 55,  5, 3, 80, 55, 5.2, 150,  125, 2, 1, 15, 30,  3, 6, 65 },
    { "POP",                       3, 92, 35, 35, 30, 20,  6, 2, 40, 24, 5.8, 220,  110, 2, 2,  5, 10,  0, 3, 40 },
    { "POP BALLAD",                3, 85, 60, 45, 50, 35,  7, 3, 65, 38, 5.4, 350,  115, 1, 1, 15, 25,  2, 3, 45 },
    { "REGGAETÓN",                 3, 100, 0, 15, 10, 5,   6, 1, 25, 15, 6.2, 250,  110, 2, 2,  0,  5,  0, 3, 30 },
    { "LATIN POP",                 3, 90, 40, 40, 30, 25,  0, 2, 55, 33, 5.6, 200,  110, 2, 2, 10, 20,  5, 4, 50 },
    { "PROFESSIONAL VOCAL",        3, 85, 50, 45, 40, 30,  4, 3, 60, 33, 5.5, 280,  110, 1, 1, 10, 20,  0, 4, 50 },
    { "GOSPEL",                    4, 80, 60, 55, 50, 40,  5, 4, 85, 60, 5.6, 200,  120, 2, 2,  5, 40,  3, 6, 70 },
    { "ÓPERA",                     4, 85, 70, 30, 40, 25,  3, 3, 90, 70, 6.0, 400,   90, 0, 1, 25, 45,  2, 4, 40 },
    { "EMOTIONAL MALE VOCAL",      4, 80, 70, 60, 60, 50,  7, 3, 75, 50, 5.3, 250,  125, 1, 1, 20, 25,  1, 4, 55 },
    { "JAZZ",                      5, 65, 100, 80, 70, 70, 4, 2, 45, 35, 5.0, 300,  120, 1, 0, 20, 15,  3, 5, 60 },
    { "NATURAL MALE VOCAL",        5, 70, 90, 70, 70, 60,  4, 1, 45, 25, 5.2, 350,  105, 0, 0, 25, 10,  0, 3, 40 },
    { "NATURAL FEMALE VOCAL",      5, 70, 90, 70, 70, 60,  4, 1, 50, 30, 5.8, 300,  105, 1, 1, 25, 10,  0, 3, 40 },
    { "HIGH TENOR",                5, 85, 45, 40, 35, 25,  3, 2, 55, 32, 6.0, 250,  110, 3, 2,  5, 15,  2, 4, 45 },
    { "DANCEHALL",                 6, 100, 5, 10, 10, 5,   2, 2, 35, 20, 6.8, 120,  110, 2, 2,  0, 10,  0, 3, 30 },
    { "CUSTOM ORYMY",              6, 88, 40, 40, 30, 25,  0, 3, 60, 34, 5.6, 220,  120, 2, 1, 10, 20,  4, 5, 55 },
};
constexpr int kNumStyles = static_cast<int> (sizeof (kStyles) / sizeof (kStyles[0]));
} // namespace

int stylePresetCount() { return kNumStyles; }
const char* stylePresetName (int i) { return (i >= 0 && i < kNumStyles) ? kStyles[i].name : ""; }
int stylePresetCategory (int i) { return (i >= 0 && i < kNumStyles) ? kStyles[i].category : 0; }
int presetCategoryCount() { return static_cast<int> (sizeof (kCategories) / sizeof (kCategories[0])); }
const char* presetCategoryName (int c) { return (c >= 0 && c < presetCategoryCount()) ? kCategories[c] : ""; }

void applyStylePreset (Settings& s, int i)
{
    if (i < 0 || i >= kNumStyles) return;
    const StyleRow& r = kStyles[i];
    resetToDefaults (s, true);
    applyVibratoStyle (s, r.vStyle);           // the style's own character first, then the preset's values on top
    s.set (pStrength, r.strength); s.set (pRetune, r.retune); s.set (pHumanize, r.human);
    s.set (pTransition, r.trans); s.set (pDrift, r.drift);
    s.set (pVibStyle, r.vStyle); s.set (pVibIntel, r.vInt); s.set (pVibAmount, r.vAmt);
    s.set (pVibDepth, r.vDepth); s.set (pVibRate, r.vRate); s.set (pVibDelay, r.vDelay);
    s.set (pBreath, r.breath); s.set (pAir, r.air); s.set (pPresence, r.pres);
    s.set (pSoftness, r.soft); s.set (pDynamics, r.dyn);
    s.set (pMelMode, r.mMode); s.set (pMelNotes, r.mNotes); s.set (pMelLength, r.mLen);
}

std::string serializeSettings (const Settings& s)
{
    std::string out = "COOLTONE_PRESET 1\n";
    const auto& sp = paramSpecs();
    char buf[96];
    for (std::size_t i = 0; i < sp.size(); ++i)
    {
        std::snprintf (buf, sizeof buf, "%s=%.6g\n", sp[i].id, s.v[i]);
        out += buf;
    }
    return out;
}

bool parseSettings (const std::string& text, Settings& s)
{
    std::istringstream in (text);
    std::string line;
    if (! std::getline (in, line) || line.rfind ("COOLTONE_PRESET", 0) != 0) return false;
    const auto& sp = paramSpecs();
    while (std::getline (in, line))
    {
        const auto eq = line.find ('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr (0, eq);
        const double val = std::atof (line.c_str() + eq + 1);
        for (std::size_t i = 0; i < sp.size(); ++i)
            if (key == sp[i].id) { s.set (static_cast<ParamId> (i), val); break; }
    }
    return true;
}
} // namespace cooltone
