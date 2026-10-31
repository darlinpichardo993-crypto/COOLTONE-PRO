#pragma once
// COOLTONE by ORYMY - YMY RECORDS
// Musical key / scale model: which pitch classes are allowed, snapping, key detection.
#include <array>
#include <cstdint>
#include <vector>

namespace cooltone
{
enum class ScaleType
{
    Major, NaturalMinor, HarmonicMinor, MelodicMinor, MajorPentatonic, MinorPentatonic,
    MajorBlues, MinorBlues, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian, Ionian,
    Jazz, Chromatic, WholeTone, Diminished, Custom
};

struct KeyEstimate
{
    int root = 0;          // 0 = C ... 11 = B
    bool minor = false;
    double correlation = 0.0; // Pearson correlation with the winning profile (-1..1)
};

class ScaleEngine
{
public:
    static const char* scaleName (ScaleType);
    static const char* keyName (int root);
    static int scaleCount() { return 20; }
    // Semitone offsets from the root. "Jazz" = bebop dominant (documented design decision).
    // Custom returns an empty list (its notes come from the custom mask).
    static std::vector<int> intervals (ScaleType);

    void setKey (int root);                 // clears manual overrides
    void setScale (ScaleType);              // clears manual overrides
    void setCustomMask (std::uint16_t absolutePitchClassMask); // bit i = pitch class i (C=0)
    void setNoteEnabled (int pitchClass, bool enabled);        // manual override on top of the scale
    void clearOverrides();

    int root() const { return root_; }
    ScaleType scale() const { return scale_; }
    std::uint16_t mask() const;             // effective mask incl. manual overrides
    bool isAllowed (int midiNote) const;

    // Nearest allowed integer MIDI note to a (fractional) MIDI pitch. Ties go to the lower note.
    double nearestAllowed (double midi) const;

    // Krumhansl-Schmuckler key finding from a 12-bin pitch-class histogram (C = bin 0).
    static KeyEstimate detectKey (const std::array<double, 12>& pitchClassHistogram);

private:
    void recompute();
    int root_ = 0;
    ScaleType scale_ = ScaleType::Major;
    std::uint16_t customMask_ = 0x0FFF;
    std::uint16_t base_ = 0;
    std::uint16_t forceOn_ = 0, forceOff_ = 0;
public:
    ScaleEngine() { recompute(); }
};
} // namespace cooltone
