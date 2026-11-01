#include "PianoRoll.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
bool isBlackKey (int m) { const int p = ((m % 12) + 12) % 12; return p == 1 || p == 3 || p == 6 || p == 8 || p == 10; }
juce::String noteName (int m) { static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return juce::String (n[((m % 12) + 12) % 12]) + juce::String (m / 12 - 1); }
}

PianoRoll::PianoRoll (CooltoneProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    view = proc.getView();
    startTimerHz (30);
}

void PianoRoll::refresh()
{
    auto nextView = proc.getView();
    const bool newAnalysis = ! view || ! nextView || view->analysis != nextView->analysis;
    view = std::move (nextView);
    lastVersion = proc.getViewVersion();
    if (! view) { shown.clear(); sel.clear(); return; }
    shown.assign (view->notes.size(), 0.0);
    for (size_t i = 0; i < view->notes.size(); ++i)
    {
        const auto& e = view->notes[i];
        double v = e.edit.hasTarget ? e.edit.targetMidi : e.note.midiPitch;
        if (view->finalMidi && view->analysis)
        {
            std::vector<double> f;
            for (size_t k = e.note.firstFrame; k <= e.note.lastFrame && k < view->finalMidi->size(); ++k) if ((*view->finalMidi)[k] > 0) f.push_back ((*view->finalMidi)[k]);
            if (! f.empty()) { std::sort (f.begin(), f.end()); v = f[f.size() / 2]; }
        }
        shown[i] = v;
    }
    // Centre and scale the piano roll around the newly analysed take. Without this,
    // the default C6 view hides normal male/female vocal ranges (often C3-C5).
    if (newAnalysis && ! view->notes.empty())
    {
        double lo = 127.0, hi = 0.0;
        for (const auto& e : view->notes)
        {
            const double pitch = e.edit.hasTarget ? e.edit.targetMidi : e.note.midiPitch;
            lo = std::min (lo, pitch); hi = std::max (hi, pitch);
        }
        const double margin = 2.0;
        const double usableHeight = std::max (100.0, (double) plotBottom() - rulerH - 12.0);
        pxPerSemi = juce::jlimit (7.0, 30.0, usableHeight / std::max (8.0, hi - lo + 2.0 * margin));
        topMidi = hi + margin;
    }
    // drop selection entries that no longer exist
    for (auto it = sel.begin(); it != sel.end();) it = (*it >= (int) view->notes.size()) ? sel.erase (it) : std::next (it);
}

void PianoRoll::timerCallback()
{
    const double ph = proc.getPlayheadSeconds();
    if (proc.getViewVersion() != lastVersion) { refresh(); repaint(); }
    else if (std::abs (ph - lastPlayhead) > 1e-4) { repaint(); }
    lastPlayhead = ph;
}

bool PianoRoll::getSelectionSpan (double& a, double& b) const
{
    if (sel.empty() || ! view) return false;
    a = 1e18; b = -1e18;
    for (int i : sel) if (i < (int) view->notes.size()) { a = std::min (a, view->notes[(size_t) i].note.startSec); b = std::max (b, view->notes[(size_t) i].note.endSec); }
    return b > a;
}

int PianoRoll::hitNote (float x, float y) const
{
    if (! view) return -1;
    const double t = timeAt (x), m = midiAt (y);
    int best = -1; double bd = 1e9;
    for (size_t i = 0; i < view->notes.size(); ++i)
    {
        const auto& n = view->notes[i].note;
        if (t < n.startSec || t > n.endSec) continue;
        const double d = std::abs (m - (i < shown.size() ? shown[i] : n.midiPitch));
        if (d < 1.0 && d < bd) { bd = d; best = (int) i; }
    }
    return best;
}

void PianoRoll::paint (juce::Graphics& g)
{
    using namespace look;
    g.fillAll (bg);
    if (! view) return;
    const int top = rulerH, bottom = plotBottom();
    const auto settings = proc.readSettings();
    const unsigned mask = settings.scaleMask();
    const double tL = scrollSec, tR = timeAt ((float) getWidth());

    {   // ---- grid + piano keys ----
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (0, top, getWidth(), bottom - top);
        for (int m = (int) std::floor (midiAt ((float) bottom)) - 1; m <= (int) std::ceil (midiAt ((float) top)) + 1; ++m)
        {
            const float y0 = yOf (m + 0.5), y1 = yOf (m - 0.5);
            const bool allowed = (mask >> (((m % 12) + 12) % 12)) & 1u;
            g.setColour (allowed ? juce::Colour (0xff17131f) : (isBlackKey (m) ? juce::Colour (0xff0b0b0f) : juce::Colour (0xff0f0f14)));
            g.fillRect ((float) keyW, y0, (float) (getWidth() - keyW), y1 - y0);
            g.setColour (edge.withAlpha (0.5f)); g.drawHorizontalLine ((int) y1, (float) keyW, (float) getWidth());
            g.setColour (isBlackKey (m) ? juce::Colour (0xff15151b) : juce::Colour (0xffcfd1da));
            g.fillRect (0.0f, y0, (float) keyW - 2.0f, y1 - y0 - 1.0f);
            if (m % 12 == 0 || pxPerSemi > 15) { g.setColour (isBlackKey (m) ? dim : juce::Colour (0xff22222c)); g.setFont (10.0f); if (m % 12 == 0 || ! isBlackKey (m)) g.drawText (noteName (m), 2, (int) y0, keyW - 6, (int) (y1 - y0), juce::Justification::centredRight); }
        }
        double bpm = proc.getHostBpm(); if (bpm < 20 || bpm > 400) bpm = 120;
        const double beat = 60.0 / bpm;
        for (long b = (long) std::floor (tL / beat); b * beat <= tR; ++b)
        {
            const float x = xOf ((double) b * beat);
            g.setColour (b % 4 == 0 ? edge : edge.withAlpha (0.35f)); g.drawVerticalLine ((int) x, (float) top, (float) bottom);
        }
    }

    {   // ---- ruler ----
        g.setColour (panel); g.fillRect (0, 0, getWidth(), rulerH);
        double bpm = proc.getHostBpm(); if (bpm < 20 || bpm > 400) bpm = 120;
        const double beat = 60.0 / bpm;
        g.setColour (dim); g.setFont (10.0f);
        for (long b = (long) std::floor (tL / beat); b * beat <= tR; ++b)
            if (b >= 0 && b % 4 == 0) g.drawText (juce::String (b / 4 + 1), (int) xOf ((double) b * beat) + 3, 0, 40, rulerH, juce::Justification::centredLeft);
        g.setColour (dim); g.drawText ("Ctrl+wheel: zoom  |  Shift+wheel: time  |  arrows: navigate  |  right/middle drag: pan", getWidth() - 440, 0, 436, rulerH, juce::Justification::centredRight);
    }

    {   // ---- notes and curves ----
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (keyW, top, getWidth() - keyW, bottom - top);
        const auto& notes = view->notes;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            const auto& e = notes[i];
            const float x0 = xOf (e.note.startSec), x1 = xOf (e.note.endSec);
            if (x1 < keyW || x0 > getWidth()) continue;
            const double heard = shown[i] + ((dragging && std::find (dragNotes.begin(), dragNotes.end(), (int) i) != dragNotes.end()) ? dragDelta : 0.0);
            const float h = (float) pxPerSemi * 0.8f;
            const auto orig = juce::Rectangle<float> (x0, yOf (e.note.midiPitch) - h * 0.5f, x1 - x0, h);
            const auto now = juce::Rectangle<float> (x0, yOf (heard) - h * 0.5f, x1 - x0, h);
            const bool selected = sel.count ((int) i) > 0;
            g.setColour (silver.withAlpha (e.edit.deleted ? 0.12f : 0.35f)); g.drawRoundedRectangle (orig, 3.0f, 1.0f);       // detected
            g.setColour ((e.edit.deleted ? dim : violet).withAlpha (e.edit.deleted ? 0.25f : 0.75f)); g.fillRoundedRectangle (now, 3.0f);   // heard
            if (selected) { g.setColour (blue); g.drawRoundedRectangle (now.expanded (1.5f), 3.0f, 2.0f); }
            if (e.edit.vibrato) { g.setColour (juce::Colours::white); g.setFont (11.0f); g.drawText ("~ vib", now.toNearestInt().reduced (3, 0), juce::Justification::centredLeft); }
            if (! e.edit.melisma.empty())
                for (const auto& m : e.edit.melisma) { g.setColour (blue.withAlpha (0.8f)); g.fillRect (xOf (m.startSec), yOf (m.midi) - 2.0f, std::max (2.0f, xOf (m.endSec) - xOf (m.startSec) - 1.0f), 4.0f); }
        }
        if (view->analysis)
        {
            const auto& fr = view->analysis->frames;
            const double hop = view->analysis->hop;
            if (! fr.empty())
            {
                const long i0 = std::max<long> (0, (long) ((tL - fr[0].timeSec) / hop) - 1), i1 = std::min<long> ((long) fr.size() - 1, (long) ((tR - fr[0].timeSec) / hop) + 1);
                juce::Path po, pf; bool openO = false, openF = false;
                for (long i = i0; i <= i1; ++i)
                {
                    const auto& f = fr[(size_t) i];
                    const float x = xOf (f.timeSec);
                    if (f.voiced) { const float y = yOf (f.midi); if (openO) po.lineTo (x, y); else { po.startNewSubPath (x, y); openO = true; } } else openO = false;
                    const double fm = (view->finalMidi && (size_t) i < view->finalMidi->size()) ? (*view->finalMidi)[(size_t) i] : 0.0;
                    if (fm > 0) { const float y = yOf (fm); if (openF) pf.lineTo (x, y); else { pf.startNewSubPath (x, y); openF = true; } } else openF = false;
                }
                g.setColour (silver.withAlpha (0.55f)); g.strokePath (po, juce::PathStrokeType (1.2f));   // original pitch
                g.setColour (blue); g.strokePath (pf, juce::PathStrokeType (1.6f));                       // corrected pitch + vibrato
            }
        }
        if (marquee) { g.setColour (blue.withAlpha (0.2f)); g.fillRect (marqueeRect); g.setColour (blue); g.drawRect (marqueeRect, 1.0f); }
        const float px = xOf (proc.getPlayheadSeconds());
        g.setColour (juce::Colour (0xffff6b6b)); g.drawVerticalLine ((int) px, (float) top, (float) bottom);
    }

    {   // ---- waveform ----
        g.setColour (panel); g.fillRect (0, bottom, getWidth(), waveH);
        if (view->analysis && ! view->analysis->peakMax.empty())
        {
            const auto& a = *view->analysis;
            const float mid = (float) bottom + waveH * 0.5f;
            g.setColour (silver.withAlpha (0.7f));
            for (int x = keyW; x < getWidth(); ++x)
            {
                const long blk = (long) (timeAt ((float) x) * view->sampleRate / a.peakBlock);
                if (blk < 0 || blk >= (long) a.peakMax.size()) continue;
                g.drawVerticalLine (x, mid - a.peakMax[(size_t) blk] * waveH * 0.5f * 1.8f, mid - a.peakMin[(size_t) blk] * waveH * 0.5f * 1.8f);
            }
        }
        g.setColour (dim); g.setFont (10.0f); g.drawText ("WAVEFORM", 4, bottom, keyW, 14, juce::Justification::centredLeft);
        if (view->notes.empty()) { g.setColour (dim); g.setFont (14.0f); g.drawFittedText ("Load a WAV (or drop it here), or press REC and play your project, to see the detected notes", getLocalBounds().withTrimmedLeft (keyW), juce::Justification::centred, 2); }
    }
}

void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (e.mods.isMiddleButtonDown() || e.mods.isRightButtonDown())
    {
        panning = true; panStart = e.position; panStartSec = scrollSec; panStartMidi = topMidi;
        return;
    }
    if (e.y < rulerH && e.x > keyW) { proc.seekSeconds (std::max (0.0, timeAt ((float) e.x))); repaint(); return; }
    if (e.x < keyW || ! view) return;
    const int hit = hitNote ((float) e.x, (float) e.y);
    dragStart = e.position; dragDelta = 0.0; dragging = false; marquee = false;
    switch (tool)
    {
        case Tool::Pointer:
        case Tool::Pitch:
            if (hit >= 0)
            {
                if (e.mods.isShiftDown() && tool == Tool::Pointer)
                {
                    if (! sel.erase (hit)) sel.insert (hit);
                }
                else if (! sel.count (hit)) { sel.clear(); sel.insert (hit); }
                // Dragging a note in either Pointer or Pitch mode moves its pitch.
                // Alt-drag preserves the unsnapped target, matching the dedicated Pitch tool.
                dragNotes = getSelection();
                dragging = ! dragNotes.empty();
                notifySelection();
            }
            else if (tool == Tool::Pointer)
            {
                if (! e.mods.isShiftDown()) sel.clear();
                marquee = true; marqueeRect = { e.position, e.position };
                notifySelection();
            }
            break;
        case Tool::Split:   if (hit >= 0) { proc.editSplit (timeAt ((float) e.x)); sel.clear(); notifySelection(); } break;
        case Tool::Glue:    if (hit >= 0) { proc.editGlue (hit); sel.clear(); notifySelection(); } break;
        case Tool::Vibrato: if (hit >= 0) proc.editToggleVibrato (hit); break;
        case Tool::Melisma: if (hit >= 0) proc.editApplyMelisma (sel.count (hit) ? getSelection() : std::vector<int> { hit }); break;
        case Tool::Eraser:  if (hit >= 0) { proc.editDelete (sel.count (hit) ? getSelection() : std::vector<int> { hit }); sel.clear(); notifySelection(); } break;
    }
    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    if (panning)
    {
        scrollSec = std::max (0.0, panStartSec - (e.position.x - panStart.x) / pxPerSec);
        topMidi = juce::jlimit (0.0, 127.0, panStartMidi + (e.position.y - panStart.y) / pxPerSemi);
    }
    else if (dragging) dragDelta = (dragStart.y - e.position.y) / pxPerSemi;
    else if (marquee) marqueeRect = juce::Rectangle<float> (dragStart, e.position);
    repaint();
}

void PianoRoll::mouseUp (const juce::MouseEvent& e)
{
    if (panning) panning = false;
    if (dragging)
    {
        const double d = dragDelta; const auto notes = dragNotes;
        dragging = false; dragDelta = 0.0;
        proc.editMoveNotes (notes, d, ! e.mods.isAltDown());
    }
    if (marquee && view)
    {
        marquee = false;
        for (size_t i = 0; i < view->notes.size(); ++i)
        {
            const auto& n = view->notes[i].note;
            const float h = (float) pxPerSemi * 0.8f;
            const juce::Rectangle<float> r (xOf (n.startSec), yOf (shown[i]) - h * 0.5f, xOf (n.endSec) - xOf (n.startSec), h);
            if (r.intersects (marqueeRect)) sel.insert ((int) i);
        }
        notifySelection();
    }
    repaint();
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCtrlDown())          // horizontal zoom around the pointer
    {
        const double tUnder = timeAt ((float) e.x);
        pxPerSec = juce::jlimit (15.0, 1500.0, pxPerSec * (w.deltaY > 0 ? 1.15 : 1.0 / 1.15));
        scrollSec = std::max (0.0, tUnder - (e.x - keyW) / pxPerSec);
    }
    else if (e.mods.isAltDown()) pxPerSemi = juce::jlimit (7.0, 48.0, pxPerSemi * (w.deltaY > 0 ? 1.1 : 1.0 / 1.1));
    else if (e.mods.isShiftDown()) scrollSec = std::max (0.0, scrollSec - w.deltaY * 600.0 / pxPerSec);
    else topMidi = juce::jlimit (0.0, 127.0, topMidi - w.deltaY * 6.0);
    repaint();
}

bool PianoRoll::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { if (! sel.empty()) { proc.editDelete (getSelection()); sel.clear(); notifySelection(); } return true; }
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)) { proc.undo(); return true; }
    if (k == juce::KeyPress ('y', juce::ModifierKeys::commandModifier, 0)) { proc.redo(); return true; }
    if (k == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0) && view) { sel.clear(); for (size_t i = 0; i < view->notes.size(); ++i) sel.insert ((int) i); notifySelection(); repaint(); return true; }
    // Keyboard navigation: arrows pan the visible range; PageUp/PageDown move an octave.
    if (k == juce::KeyPress::leftKey)  { scrollSec = std::max (0.0, scrollSec - (k.getModifiers().isShiftDown() ? 1.0 : 0.25) * (getWidth() - keyW) / pxPerSec); repaint(); return true; }
    if (k == juce::KeyPress::rightKey) { scrollSec += (k.getModifiers().isShiftDown() ? 1.0 : 0.25) * (getWidth() - keyW) / pxPerSec; repaint(); return true; }
    if (k == juce::KeyPress::upKey)    { topMidi = juce::jlimit (0.0, 127.0, topMidi + (k.getModifiers().isShiftDown() ? 12.0 : 1.0)); repaint(); return true; }
    if (k == juce::KeyPress::downKey)  { topMidi = juce::jlimit (0.0, 127.0, topMidi - (k.getModifiers().isShiftDown() ? 12.0 : 1.0)); repaint(); return true; }
    if (k == juce::KeyPress::pageUpKey)   { topMidi = juce::jlimit (0.0, 127.0, topMidi + 12.0); repaint(); return true; }
    if (k == juce::KeyPress::pageDownKey) { topMidi = juce::jlimit (0.0, 127.0, topMidi - 12.0); repaint(); return true; }
    if (k == juce::KeyPress::homeKey) { scrollSec = 0.0; repaint(); return true; }
    if (k == juce::KeyPress::endKey && view) { scrollSec = std::max (0.0, view->durationSec - (getWidth() - keyW) / pxPerSec); repaint(); return true; }
    if (k == juce::KeyPress ('+', 0, 0) || k == juce::KeyPress ('=', 0, 0)) { pxPerSec = juce::jlimit (15.0, 1500.0, pxPerSec * 1.15); repaint(); return true; }
    if (k == juce::KeyPress ('-', 0, 0)) { pxPerSec = juce::jlimit (15.0, 1500.0, pxPerSec / 1.15); repaint(); return true; }
    return false;
}
