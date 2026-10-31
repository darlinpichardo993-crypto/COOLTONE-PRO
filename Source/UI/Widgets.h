#pragma once
// COOLTONE by ORYMY - YMY RECORDS - small reusable UI pieces (every control here is bound to a real parameter or action)
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "Look.h"
#include "Params.h"

class KnobCell : public juce::Component
{
public:
    KnobCell (juce::AudioProcessorValueTreeState& apvts, const cooltone::ParamSpec& sp) : label (juce::String::fromUTF8 (sp.label))
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 14);
        if (sp.unit != nullptr && sp.unit[0] != 0) slider.setTextValueSuffix (juce::String (" ") + sp.unit);
        addAndMakeVisible (slider);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, sp.id, slider);
        slider.setNumDecimalPlacesToDisplay (sp.step >= 1.0 ? 0 : 1);
        slider.setDoubleClickReturnValue (true, sp.defV);
    }
    void paint (juce::Graphics& g) override { g.setColour (look::dim); g.setFont (11.0f); g.drawFittedText (label, 0, 0, getWidth(), 14, juce::Justification::centred, 1); }
    void resized() override { slider.setBounds (0, 14, getWidth(), getHeight() - 14); }
private:
    juce::String label;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class ComboCell : public juce::Component
{
public:
    ComboCell (juce::AudioProcessorValueTreeState& apvts, const cooltone::ParamSpec& sp) : label (juce::String::fromUTF8 (sp.label))
    {
        juce::StringArray items;
        for (const auto& c : sp.choices) items.add (juce::String::fromUTF8 (c.c_str()));
        combo.addItemList (items, 1);
        addAndMakeVisible (combo);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, sp.id, combo);
    }
    void paint (juce::Graphics& g) override { g.setColour (look::dim); g.setFont (11.0f); g.drawFittedText (label, 0, 0, getWidth(), 14, juce::Justification::centredLeft, 1); }
    void resized() override { combo.setBounds (0, 16, getWidth(), getHeight() - 16); }
    juce::ComboBox combo;
private:
    juce::String label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

class ToggleCell : public juce::Component
{
public:
    ToggleCell (juce::AudioProcessorValueTreeState& apvts, const cooltone::ParamSpec& sp)
    {
        button.setButtonText (juce::String::fromUTF8 (sp.label));
        addAndMakeVisible (button);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, sp.id, button);
    }
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::ToggleButton button;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

// Titled panel that flows its children left-to-right and wraps: nothing overlaps, nothing is clipped.
class Section : public juce::Component
{
public:
    explicit Section (juce::String t, int preferredWidth = 0) : title (std::move (t)), prefW (preferredWidth) {}
    void add (juce::Component* c, int w, int h) { items.push_back ({ c, w, h }); addAndMakeVisible (c); }
    int preferredWidth() const { return prefW; }
    int layoutFor (int width)
    {
        const int pad = 8, titleH = 24, gap = 4;
        int x = pad, y = titleH, rowH = 0;
        for (auto& it : items)
        {
            if (x + it.w > width - pad && x > pad) { x = pad; y += rowH + gap; rowH = 0; }
            it.c->setBounds (x, y, it.w, it.h);
            x += it.w + gap; rowH = std::max (rowH, it.h);
        }
        return y + rowH + pad;
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (look::panel); g.fillRoundedRectangle (b, 6.0f);
        g.setColour (look::edge); g.drawRoundedRectangle (b, 6.0f, 1.0f);
        g.setColour (look::silver); g.setFont (12.0f);
        g.drawText (title, 10, 4, getWidth() - 20, 16, juce::Justification::centredLeft);
    }
    void resized() override { layoutFor (getWidth()); }
private:
    struct Item { juce::Component* c; int w, h; };
    std::vector<Item> items;
    juce::String title;
    int prefW;
};

// Vertical stack (right panel) or wrapping row flow (bottom panel) of sections, to be placed in a Viewport.
class StackPanel : public juce::Component
{
public:
    explicit StackPanel (bool flowHorizontally) : flow (flowHorizontally) {}
    void addSection (Section* s) { sections.push_back (s); addAndMakeVisible (s); }
    int layoutFor (int width)
    {
        const int gap = 6;
        int y = 0;
        if (! flow)
            for (auto* s : sections) { const int h = s->layoutFor (width); s->setBounds (0, y, width, h); y += h + gap; }
        else
        {
            int x = 0, rowH = 0;
            for (auto* s : sections)
            {
                const int w = std::min (width, s->preferredWidth() > 0 ? s->preferredWidth() : width);
                if (x + w > width && x > 0) { x = 0; y += rowH + gap; rowH = 0; }
                const int h = s->layoutFor (w);
                s->setBounds (x, y, w, h);
                x += w + gap; rowH = std::max (rowH, h);
            }
            y += rowH;
        }
        return y;
    }
private:
    bool flow;
    std::vector<Section*> sections;
};

class MeterBar : public juce::Component
{
public:
    MeterBar (std::atomic<float>& level, std::atomic<bool>* clip = nullptr) : lv (level), clipFlag (clip) {}
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (look::panel2); g.fillRoundedRectangle (b, 3.0f);
        const float db = juce::Decibels::gainToDecibels (lv.load(), -60.0f);
        const float f = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        g.setColour (db > -3.0f ? look::red : look::blue);
        g.fillRoundedRectangle (b.withWidth (b.getWidth() * f), 3.0f);
        if (clipFlag != nullptr && clipFlag->load()) { g.setColour (look::red); g.fillRoundedRectangle (b.removeFromRight (10.0f), 3.0f); }
    }
    void mouseDown (const juce::MouseEvent&) override { if (clipFlag != nullptr) clipFlag->store (false); }
private:
    std::atomic<float>& lv;
    std::atomic<bool>* clipFlag;
};

// Drag the exported WAV straight into the FL Studio playlist (native OS file drag).
class DragToPlaylist : public juce::Component
{
public:
    std::function<juce::File (juce::String&)> prepare;     // renders + validates the WAV, returns it
    std::function<void (const juce::String&)> report;
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (look::panel2); g.fillRoundedRectangle (b, 6.0f);
        g.setColour (look::violet); g.drawRoundedRectangle (b, 6.0f, 1.5f);
        g.setColour (look::text); g.setFont (12.0f);
        g.drawFittedText ("DRAG TO PLAYLIST", getLocalBounds().withTrimmedBottom (14), juce::Justification::centred, 1);
        g.setColour (look::dim); g.setFont (10.0f);
        g.drawText (ready.existsAsFile() ? ready.getFileName() : "drag the audio onto the FL Studio playlist", getLocalBounds().withTop (getHeight() - 16).reduced (4, 0), juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent&) override
    {
        juce::String err;
        ready = prepare ? prepare (err) : juce::File();
        if (err.isNotEmpty() && report) report (err);
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || ! ready.existsAsFile() || e.getDistanceFromDragStart() < 5) return;
        dragging = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ ready.getFullPathName() }, false, this, [this] { dragging = false; });
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    juce::File lastFile() const { return ready; }
private:
    juce::File ready;
    bool dragging = false;
};
