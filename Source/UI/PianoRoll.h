#pragma once
// COOLTONE by ORYMY - YMY RECORDS - piano roll: detected notes, original / corrected pitch curves (incl. vibrato), waveform
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <set>

#include "Look.h"
#include "PluginProcessor.h"

class PianoRoll : public juce::Component, private juce::Timer
{
public:
    enum class Tool { Pointer, Pitch, Split, Glue, Vibrato, Melisma, Eraser };

    explicit PianoRoll (CooltoneProcessor&);
    ~PianoRoll() override { stopTimer(); }

    void setTool (Tool t) { tool = t; }
    std::vector<int> getSelection() const { return { sel.begin(), sel.end() }; }
    bool getSelectionSpan (double& a, double& b) const;
    void clearSelection() { sel.clear(); notifySelection(); repaint(); }
    std::function<void()> onSelectionChanged;
    std::shared_ptr<ViewData> currentView() const { return view; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void refresh();
    void notifySelection() { if (onSelectionChanged) onSelectionChanged(); }
    float xOf (double t) const { return (float) (keyW + (t - scrollSec) * pxPerSec); }
    double timeAt (float x) const { return scrollSec + (x - keyW) / pxPerSec; }
    float yOf (double midi) const { return (float) (rulerH + (topMidi - midi) * pxPerSemi); }
    double midiAt (float y) const { return topMidi - (y - rulerH) / pxPerSemi; }
    int hitNote (float x, float y) const;
    int plotBottom() const { return getHeight() - waveH; }

    CooltoneProcessor& proc;
    std::shared_ptr<ViewData> view;
    std::vector<double> shown;                 // pitch actually heard for each note
    std::uint64_t lastVersion = 0;
    double lastPlayhead = -1.0;
    Tool tool = Tool::Pointer;
    std::set<int> sel;

    static constexpr int keyW = 54, rulerH = 22, waveH = 54;
    double pxPerSec = 110.0, pxPerSemi = 16.0, scrollSec = 0.0, topMidi = 84.0;

    juce::Point<float> dragStart;
    std::vector<int> dragNotes;
    double dragDelta = 0.0;
    bool dragging = false, marquee = false, panning = false;
    juce::Point<float> panStart;
    double panStartSec = 0.0, panStartMidi = 84.0;
    juce::Rectangle<float> marqueeRect;
};
