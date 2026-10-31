#pragma once
// COOLTONE by ORYMY - YMY RECORDS - dark, minimal look (deep black, graphite, silver, electric violet, cold blue)
#include <juce_gui_basics/juce_gui_basics.h>

namespace look
{
const juce::Colour bg (0xff08080b), panel (0xff131319), panel2 (0xff1c1c25), edge (0xff2b2b38);
const juce::Colour text (0xffdcdce4), dim (0xff8c8c9c), violet (0xff8b5cf6), blue (0xff5aa0ff), silver (0xffbfc3cf), red (0xffe5484d);
}

class CooltoneLook : public juce::LookAndFeel_V4
{
public:
    CooltoneLook()
    {
        using namespace look;
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::thumbColourId, violet);
        setColour (juce::TextButton::buttonColourId, panel2);
        setColour (juce::TextButton::buttonOnColourId, violet);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::ComboBox::backgroundColourId, panel2);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::outlineColourId, edge);
        setColour (juce::ComboBox::arrowColourId, silver);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, violet);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, violet);
        setColour (juce::ToggleButton::tickDisabledColourId, dim);
        setColour (juce::Label::textColourId, text);
        setColour (juce::TextEditor::backgroundColourId, panel);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::outlineColourId, edge);
        setColour (juce::ScrollBar::thumbColourId, edge);
        setColour (juce::AlertWindow::backgroundColourId, panel);
        setColour (juce::AlertWindow::textColourId, text);
    }

    juce::Font getLabelFont (juce::Label&) override { return juce::Font (juce::FontOptions (11.0f)); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (12.0f)); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (12.0f)); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override
    {
        using namespace look;
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = start + pos * (end - start);
        juce::Path track; track.addCentredArc (c.x, c.y, r, r, 0.0f, start, end, true);
        g.setColour (edge); g.strokePath (track, juce::PathStrokeType (3.0f));
        juce::Path val; val.addCentredArc (c.x, c.y, r, r, 0.0f, start, ang, true);
        g.setColour (violet); g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (panel2); g.fillEllipse (c.x - r + 5.0f, c.y - r + 5.0f, (r - 5.0f) * 2.0f, (r - 5.0f) * 2.0f);
        juce::Path p; p.addRectangle (-1.0f, -(r - 6.0f), 2.0f, (r - 6.0f) * 0.55f);
        g.setColour (silver); g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }
};
