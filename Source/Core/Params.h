#pragma once
// COOLTONE by ORYMY - YMY RECORDS
// Single source of truth for every user parameter (used by the DSP core AND the JUCE plugin).
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace cooltone
{
enum ParamId : int
{
    pBypass, pMix, pOutGain, pInGain,
    pCorrectionOn, pMode, pStrength, pRetune, pHumanize, pTransition, pDrift, pCenterHz,
    pFormantKeep, pFormantShift, pTranspose, pFlex, pRange,
    pKeyRoot, pScale,
    pNote0, pNote1, pNote2, pNote3, pNote4, pNote5, pNote6, pNote7, pNote8, pNote9, pNote10, pNote11,
    pVibStyle, pVibIntel, pVibAmount, pVibRate, pVibDepth, pVibDelay, pVibAttack, pVibRelease,
    pVibShape, pVibRandom, pVibHuman, pVibSpeed, pVibSmooth, pVibVariation,
    pVibPhase, pVibNoteSens, pVibDurSens, pVibStability, pVibKeepOrig,
    pMelMode, pMelNotes, pMelLength, pMelStart, pMelEnd, pMelRhythm, pMelHuman, pMelLock,
    pBreath, pAir, pPresence, pSoftness, pDynamics, pAttack, pRelease,
    pCount
};

enum class ParamKind { Float, Choice, Bool };

struct ParamSpec
{
    const char* id; const char* label; ParamKind kind;
    double minV, maxV, defV, step;
    const char* unit;
    std::vector<std::string> choices;
    const char* group;
};

const std::vector<ParamSpec>& paramSpecs();   // size == pCount, indexed by ParamId

struct Settings
{
    std::array<double, pCount> v {};
    Settings();
    double get (ParamId p) const { return v[static_cast<std::size_t> (p)]; }
    void set (ParamId p, double x);               // clamps to the spec range
    std::uint16_t scaleMask() const;              // from pNote0..pNote11
};
} // namespace cooltone
