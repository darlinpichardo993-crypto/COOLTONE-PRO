#include "PluginEditor.h"

#include <algorithm>

// ------------------------------------------------------------------------------------------
// small labelled linear slider for the per-note vibrato controls (Start / End / Speed / Depth)
// ------------------------------------------------------------------------------------------
struct CooltoneEditor::NoteSlider : public juce::Component
{
    NoteSlider (const juce::String& n, double lo, double hi, double def, double step, const juce::String& suffix) : name (n)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 18);
        s.setRange (lo, hi, step); s.setValue (def, juce::dontSendNotification); s.setDoubleClickReturnValue (true, def);
        s.setTextValueSuffix (suffix);
        addAndMakeVisible (s);
    }
    void paint (juce::Graphics& g) override { g.setColour (look::dim); g.setFont (11.0f); g.drawText (name, 0, 0, 76, getHeight(), juce::Justification::centredLeft); }
    void resized() override { s.setBounds (76, 0, getWidth() - 76, getHeight()); }
    juce::Slider s;
    juce::String name;
};

template <typename Cell> Cell* CooltoneEditor::make (ct::ParamId id)
{
    auto* c = new Cell (proc.apvts, ct::paramSpecs()[(size_t) id]);
    owned.add (c);
    return c;
}

void CooltoneEditor::addParam (Section* s, ct::ParamId id)
{
    const auto& sp = ct::paramSpecs()[(size_t) id];
    if (sp.kind == ct::ParamKind::Float) s->add (make<KnobCell> (id), 84, 98);
    else if (sp.kind == ct::ParamKind::Choice) s->add (make<ComboCell> (id), 132, 46);
    else s->add (make<ToggleCell> (id), 190, 26);
}

juce::TextButton* CooltoneEditor::makeButton (const juce::String& text, std::function<void()> onClick)
{
    auto* b = new juce::TextButton (text);
    b->onClick = std::move (onClick);
    owned.add (b);
    return b;
}

// ------------------------------------------------------------------------------------------
CooltoneEditor::CooltoneEditor (CooltoneProcessor& p) : juce::AudioProcessorEditor (&p), proc (p), roll (p)
{
    setLookAndFeel (&look);

    // ---------------- header ----------------
    rebuildPresetBox();
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id >= 100 && id < 1000) proc.applyFactoryPreset (id - 100);
        else if (id >= 1000 && (id - 1000) < userPresetNames.size()) proc.loadUserPreset (userPresetNames[id - 1000]);
    };
    presetMenuBtn.onClick = [this] { showPresetMenu(); };
    for (const auto& c : ct::paramSpecs()[ct::pKeyRoot].choices) keyBox.addItem (juce::String::fromUTF8 (c.c_str()), keyBox.getNumItems() + 1);
    for (const auto& c : ct::paramSpecs()[ct::pScale].choices) scaleBox.addItem (juce::String::fromUTF8 (c.c_str()), scaleBox.getNumItems() + 1);
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "keyRoot", keyBox);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "scale", scaleBox);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", bypassBtn);
    undoBtn.onClick = [this] { proc.undo(); };
    redoBtn.onClick = [this] { proc.redo(); };
    abBtn.onClick = [this] { proc.toggleAB(); abBtn.setButtonText (proc.getABSlot() == 0 ? "A/B: A" : "A/B: B"); };
    abBtn.setButtonText ("A/B: A");
    settingsBtn.onClick = [this] { showSettingsMenu(); };
    bpmLabel.setJustificationType (juce::Justification::centred);
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &presetMenuBtn, &keyBox, &scaleBox, &bpmLabel, &undoBtn, &redoBtn, &abBtn, &bypassBtn, &settingsBtn })
        addAndMakeVisible (c);

    // ---------------- transport / actions ----------------
    loadBtn.onClick = [this] { chooseAndLoadFile(); };
    recBtn.onClick = [this] { if (proc.isCapturing()) proc.stopCapture(); else proc.armCapture(); };
    playBtn.onClick = [this] { proc.stopLoop(); proc.setInternalPlaying (true); };
    stopBtn.onClick = [this] { proc.setInternalPlaying (false); };
    loopBtn.onClick = [this]
    {
        double a = 0, b = 0;
        if (roll.getSelectionSpan (a, b)) proc.playSelectionLooped (a, b); else say ("Select one or more notes first", true);
    };
    detectBtn.onClick = [this]
    {
        const auto k = proc.detectKey (true);
        say (juce::String ("Key detected: ") + ct::ScaleEngine::keyName (k.root) + (k.minor ? " minor" : " major") + "  (confidence " + juce::String (juce::roundToInt (juce::jmax (0.0, k.correlation) * 100.0)) + "%)");
    };
    autoBtn.onClick = [this] { proc.autoTune(); say ("Auto Tune on - correcting to the selected key and scale"); };
    autoBtn.setColour (juce::TextButton::buttonColourId, look::violet.darker (0.3f));
    for (auto* b : std::initializer_list<juce::Component*> { &loadBtn, &recBtn, &playBtn, &stopBtn, &loopBtn, &detectBtn, &autoBtn }) addAndMakeVisible (b);

    static const char* toolNames[7] = { "Pointer", "Pitch", "Split", "Glue", "Vibrato", "Melisma", "Eraser" };
    for (int i = 0; i < 7; ++i)
    {
        auto& b = toolBtns[i];
        b.setButtonText (toolNames[i]); b.setClickingTogglesState (true); b.setRadioGroupId (101);
        b.onClick = [this, i] { roll.setTool ((PianoRoll::Tool) i); };
        addAndMakeVisible (b);
    }
    toolBtns[0].setToggleState (true, juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (statusLabel);

    addAndMakeVisible (roll);
    roll.onSelectionChanged = [this] { selectionChanged(); };

    // ---------------- right panel ----------------
    auto* vib = sections.add (new Section ("MAGIC VIBRATO"));
    for (auto id : { ct::pVibStyle, ct::pVibIntel }) addParam (vib, id);
    for (auto id : { ct::pVibAmount, ct::pVibRate, ct::pVibDepth, ct::pVibDelay, ct::pVibAttack, ct::pVibRelease, ct::pVibShape, ct::pVibRandom,
                     ct::pVibHuman, ct::pVibSpeed, ct::pVibSmooth, ct::pVibVariation, ct::pVibPhase, ct::pVibNoteSens, ct::pVibDurSens, ct::pVibStability })
        addParam (vib, id);
    addParam (vib, ct::pVibKeepOrig);
    vib->add (makeButton ("MAGIC VIBRATO", [this]
    {
        const int n = proc.editMagicVibrato (roll.getSelection(), false);
        say (n > 0 ? juce::String (n) + " notes carry vibrato" : "No note qualifies with these settings (try another intensity or style)");
    }), 190, 28);
    vib->add (makeButton ("AUTO VOICE", [this]
    {
        const auto key = proc.detectKey (true);
        proc.autoTune();
        const int n = proc.editMagicVibrato (roll.getSelection(), false);
        const juce::String detected = juce::String (ct::ScaleEngine::keyName (key.root))
            + juce::String (key.minor ? " minor" : " major");
        const juce::String message = juce::String ("AUTO VOICE · ") + detected
            + juce::String (" · ") + juce::String (n) + juce::String (" notes · selected vocal profile");
        say (message);
    }), 132, 28);
    vib->add (makeButton ("Regenerate", [this] { proc.editMagicVibrato (roll.getSelection(), true); }), 92, 26);
    vib->add (makeButton ("Remove", [this] { proc.editForceVibrato (roll.getSelection(), false); }), 92, 26);
    vib->add (makeButton ("Apply to Selected", [this]
    {
        if (roll.getSelection().empty()) say ("Select notes first", true); else proc.editForceVibrato (roll.getSelection(), true);
    }), 132, 26);
    vib->add (makeButton ("Apply to All", [this] { proc.editForceVibrato ({}, true); }), 132, 26);
    vib->add (makeButton ("Reset Curve", [this] { proc.editResetVibratoCurve (roll.getSelection()); }), 132, 26);
    vib->add (makeButton ("Randomize Humanization", [this] { proc.editRandomizeVibrato (roll.getSelection()); }), 132, 26);
    noteStart = new NoteSlider ("Vib. Start", 0, 95, 0, 1, " %");
    noteEnd = new NoteSlider ("Vib. End", 5, 100, 100, 1, " %");
    noteSpeed = new NoteSlider ("Note Speed", 0.5, 1.6, 1.0, 0.01, " x");
    noteDepth = new NoteSlider ("Note Depth", 0.0, 2.0, 1.0, 0.01, " x");
    for (auto* s : { noteStart, noteEnd, noteSpeed, noteDepth })
    {
        owned.add (s);
        s->s.onDragEnd = [this] { applySelectedNoteVibrato(); };
        vib->add (s, 270, 22);
    }
    rightPanel.addSection (vib);

    auto* mel = sections.add (new Section ("MELISMA ENGINE"));
    for (auto id : { ct::pMelMode, ct::pMelRhythm }) addParam (mel, id);
    for (auto id : { ct::pMelNotes, ct::pMelLength, ct::pMelStart, ct::pMelEnd, ct::pMelHuman }) addParam (mel, id);
    addParam (mel, ct::pMelLock);
    mel->add (makeButton ("Apply to Selected Note", [this]
    {
        if (roll.getSelection().empty()) say ("Select a note first", true); else proc.editApplyMelisma (roll.getSelection());
    }), 150, 28);
    mel->add (makeButton ("Clear Melisma", [this] { proc.editClearMelisma (roll.getSelection()); }), 110, 28);
    rightPanel.addSection (mel);

    auto* expr = sections.add (new Section ("VOICE EXPRESSION"));
    for (auto id : { ct::pBreath, ct::pAir, ct::pPresence, ct::pSoftness, ct::pDynamics, ct::pAttack, ct::pRelease }) addParam (expr, id);
    rightPanel.addSection (expr);

    auto* scl = sections.add (new Section ("SCALE LIBRARY"));
    static const char* kn[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i) { auto* t = make<ToggleCell> ((ct::ParamId) (ct::pNote0 + i)); (void) kn; scl->add (t, 62, 26); }
    keyInfo.setJustificationType (juce::Justification::centredLeft);
    scl->add (&keyInfo, 250, 22);
    scl->add (makeButton ("Detect Key / Scale", [this] { detectBtn.triggerClick(); }), 150, 26);
    for (auto id : { ct::pCenterHz }) addParam (scl, id);
    rightPanel.addSection (scl);

    auto* ana = sections.add (new Section ("ANALYZE VOICE"));
    reportBox.setMultiLine (true); reportBox.setReadOnly (true); reportBox.setScrollbarsShown (false); reportBox.setCaretVisible (false);
    ana->add (&reportBox, 250, 170);
    rightPanel.addSection (ana);

    rightVp.setViewedComponent (&rightPanel, false);
    rightVp.setScrollBarsShown (true, false);
    addAndMakeVisible (rightVp);

    // ---------------- bottom panel ----------------
    auto* pit = sections.add (new Section ("AUTO TUNE ENGINE", 600));
    addParam (pit, ct::pMode);
    for (auto id : { ct::pStrength, ct::pRetune, ct::pHumanize, ct::pTransition, ct::pDrift, ct::pFlex, ct::pRange,
                     ct::pFormantKeep, ct::pFormantShift, ct::pTranspose })
        addParam (pit, id);
    addParam (pit, ct::pCorrectionOn);
    bottomPanel.addSection (pit);

    auto* rec = sections.add (new Section ("RECORD VOICE", 300));
    addParam (rec, ct::pInGain);
    auto* inMeter = new MeterBar (proc.inPeak, &proc.inputClipped); owned.add (inMeter);
    rec->add (inMeter, 190, 16);
    auto* recInfo = new juce::Label ({}, "Input meter (click the red edge to clear clip).\nIn the Standalone app pick the input device and channel in Options. In a DAW, press REC and play the project.");
    recInfo->setFont (juce::Font (juce::FontOptions (11.0f))); recInfo->setColour (juce::Label::textColourId, look::dim); owned.add (recInfo);
    rec->add (recInfo, 270, 70);
    bottomPanel.addSection (rec);

    auto* outp = sections.add (new Section ("OUTPUT", 210));
    addParam (outp, ct::pOutGain); addParam (outp, ct::pMix);
    auto* outMeter = new MeterBar (proc.outPeak); owned.add (outMeter);
    outp->add (outMeter, 180, 16);
    bottomPanel.addSection (outp);

    auto* exp = sections.add (new Section ("EXPORT TO FL STUDIO", 440));
    sourceBox.addItemList ({ "Processed (all modules)", "Dry (original)", "Auto Tune only", "Magic Vibrato only" }, 1); sourceBox.setSelectedId (1, juce::dontSendNotification);
    formatBox.addItemList ({ "24-bit PCM", "32-bit float", "16-bit PCM" }, 1);
    formatBox.setSelectedId (proc.exportFormat == ct::WavFormat::Float32 ? 2 : proc.exportFormat == ct::WavFormat::PCM16 ? 3 : 1, juce::dontSendNotification);
    formatBox.onChange = [this] { const int id = formatBox.getSelectedId(); proc.exportFormat = id == 2 ? ct::WavFormat::Float32 : id == 3 ? ct::WavFormat::PCM16 : ct::WavFormat::PCM24; };
    exp->add (&sourceBox, 200, 26); exp->add (&formatBox, 120, 26);
    exp->add (&selOnlyBtn, 190, 26);
    exp->add (makeButton ("EXPORT AUDIO", [this] { doExportAudio(); }), 130, 30);
    exp->add (makeButton ("EXPORT MIDI", [this] { doExportMidi(); }), 110, 30);
    exp->add (makeButton ("Show file", [this]
    {
        const auto f = dragArea.lastFile().existsAsFile() ? dragArea.lastFile() : lastExport;
        if (f.existsAsFile()) f.revealToUser(); else say ("Nothing exported yet", true);
    }), 90, 30);
    dragArea.prepare = [this] (juce::String& err) -> juce::File
    {
        bool ok = true; const auto o = makeOptions (ok);
        if (! ok) { err = "Select notes first (Selection only is on)"; return {}; }
        return proc.prepareDragFile (o, (ExportSource) (sourceBox.getSelectedId() - 1), err);
    };
    dragArea.report = [this] (const juce::String& e) { say (e, true); };
    exp->add (&dragArea, 230, 56);
    bottomPanel.addSection (exp);

    bottomVp.setViewedComponent (&bottomPanel, false);
    bottomVp.setScrollBarsShown (true, false);
    addAndMakeVisible (bottomVp);

    setResizable (true, true);
    setResizeLimits (940, 560, 3200, 1900);
    setSize (juce::jlimit (940, 3200, proc.uiWidth), juce::jlimit (560, 1900, proc.uiHeight));
    startTimerHz (20);
}

CooltoneEditor::~CooltoneEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ------------------------------------------------------------------------------------------
void CooltoneEditor::paint (juce::Graphics& g)
{
    g.fillAll (look::bg);
    const bool narrow = getWidth() < 1100;
    g.setColour (look::text);
    g.setFont (juce::Font (juce::FontOptions (narrow ? 22.0f : 26.0f)).boldened());
    g.drawText ("COOLTONE", 12, 6, narrow ? 140 : 170, 28, juce::Justification::centredLeft);
    g.setColour (look::violet);
    g.setFont (11.0f);
    g.drawText ("BY ORYMY", 14, 30, 90, 14, juce::Justification::centredLeft);
    g.setColour (look::dim);
    g.drawText ("YMY RECORDS", 80, 30, 110, 14, juce::Justification::centredLeft);
    g.setColour (look::edge);
    g.drawHorizontalLine (50, 8.0f, (float) getWidth() - 8.0f);
}

void CooltoneEditor::layoutPanels()
{
    for (auto* vp : { &rightVp, &bottomVp })
    {
        auto* panel = (vp == &rightVp) ? static_cast<StackPanel*> (&rightPanel) : &bottomPanel;
        const int w = std::max (100, vp->getWidth() - vp->getScrollBarThickness() - 2);
        panel->setSize (w, panel->layoutFor (w));
    }
}

void CooltoneEditor::resized()
{
    proc.uiWidth = getWidth(); proc.uiHeight = getHeight();
    auto r = getLocalBounds().reduced (8);
    auto header = r.removeFromTop (44);
    const bool narrow = getWidth() < 1100;
    header.removeFromLeft (narrow ? 190 : 230);
    settingsBtn.setBounds (header.removeFromRight (74).reduced (2, 8));
    bypassBtn.setBounds (header.removeFromRight (74).reduced (2, 8));
    abBtn.setBounds (header.removeFromRight (62).reduced (2, 8));
    redoBtn.setBounds (header.removeFromRight (54).reduced (2, 8));
    undoBtn.setBounds (header.removeFromRight (54).reduced (2, 8));
    bpmLabel.setBounds (header.removeFromRight (62).reduced (2, 8));
    scaleBox.setBounds (header.removeFromRight (140).reduced (2, 8));
    keyBox.setBounds (header.removeFromRight (62).reduced (2, 8));
    presetMenuBtn.setBounds (header.removeFromRight (32).reduced (2, 8));
    presetBox.setBounds (header.reduced (2, 8));

    r.removeFromTop (4);
    auto t1 = r.removeFromTop (30);
    int x = t1.getX();
    for (auto* b : std::initializer_list<juce::Component*> { &loadBtn, &recBtn, &playBtn, &stopBtn, &loopBtn, &detectBtn })
    { const int w = (b == &loopBtn || b == &detectBtn || b == &loadBtn) ? 96 : 64; b->setBounds (x, t1.getY(), w, 30); x += w + 4; }
    autoBtn.setBounds (x + 6, t1.getY(), 120, 30);
    r.removeFromTop (4);
    auto t2 = r.removeFromTop (28);
    x = t2.getX();
    for (auto& b : toolBtns) { b.setBounds (x, t2.getY(), 72, 28); x += 76; }
    statusLabel.setBounds (x + 8, t2.getY(), std::max (50, t2.getRight() - x - 8), 28);
    r.removeFromTop (6);

    const int rightW = getWidth() < 1100 ? 292 : getWidth() < 1500 ? 332 : getWidth() < 1900 ? 372 : 420;
    rightVp.setBounds (r.removeFromRight (rightW));
    r.removeFromRight (6);
    const int bottomH = juce::jlimit (176, 340, (int) (getHeight() * 0.30));
    bottomVp.setBounds (r.removeFromBottom (bottomH));
    r.removeFromBottom (6);
    roll.setBounds (r);
    layoutPanels();
}

// ------------------------------------------------------------------------------------------
void CooltoneEditor::timerCallback()
{
    const bool rec = proc.isCapturing();
    recBtn.setButtonText (rec ? "STOP REC" : "REC");
    recBtn.setColour (juce::TextButton::buttonColourId, rec ? look::red : look::panel2);
    playBtn.setToggleState (proc.isInternalPlaying(), juce::dontSendNotification);
    undoBtn.setEnabled (proc.canUndo()); redoBtn.setEnabled (proc.canRedo());
    const double bpm = proc.getHostBpm();
    bpmLabel.setText ("BPM " + juce::String (juce::roundToInt (bpm)), juce::dontSendNotification);

    if (transientUntil > juce::Time::currentTimeMillis()) { statusLabel.setText (transientMsg, juce::dontSendNotification); }
    else { statusLabel.setText (proc.getStatus(), juce::dontSendNotification); statusLabel.setColour (juce::Label::textColourId, look::dim); }

    if (++reportTick % 10 == 0 && proc.hasClip())
    {
        const auto rep = proc.getReport();
        juce::String t;
        t << "Duration: " << juce::String (rep.durationSec, 1) << " s    Voiced: " << juce::roundToInt (rep.voicedFraction * 100.0) << "%\n";
        t << "F0: " << juce::roundToInt (rep.f0MinHz) << " / " << juce::roundToInt (rep.f0MedianHz) << " / " << juce::roundToInt (rep.f0MaxHz) << " Hz (min/med/max)\n";
        t << "Likely key: " << rep.keyName << (rep.keyMinor ? " minor" : " major") << "  (confidence " << juce::roundToInt (juce::jmax (0.0, rep.key.correlation) * 100.0) << "%)\n";
        t << "Notes: " << rep.numNotes << "   sustained (>1 s): " << rep.numSustained << "\n";
        t << "Avg. tuning error: " << juce::String (rep.avgAbsDeviationCents, 1) << " cents\n";
        t << "Natural vibrato: " << rep.notesWithVibrato << " notes";
        if (rep.notesWithVibrato > 0) t << "  (" << juce::String (rep.avgVibratoRateHz, 1) << " Hz, " << juce::String (rep.avgVibratoDepthCents, 0) << " cents)";
        t << "\nRuns / melismas: " << rep.numRuns << "   breaths: " << rep.numBreaths;
        if (t != reportBox.getText()) reportBox.setText (t, false);
        keyInfo.setText (juce::String ("Detected: ") + rep.keyName + (rep.keyMinor ? " minor" : " major") + " (" + juce::String (juce::roundToInt (juce::jmax (0.0, rep.key.correlation) * 100.0)) + "%) - you can change it above", juce::dontSendNotification);
    }
    else if (! proc.hasClip()) reportBox.setText ("No audio yet.\nLoad a WAV, drop a file on this window,\nor press REC and play your project.", false);
}

void CooltoneEditor::say (const juce::String& m, bool error)
{
    transientMsg = m; transientUntil = juce::Time::currentTimeMillis() + 5000;
    statusLabel.setColour (juce::Label::textColourId, error ? look::red : look::silver);
    statusLabel.setText (m, juce::dontSendNotification);
}

void CooltoneEditor::selectionChanged()
{
    const auto sel = roll.getSelection();
    auto v = roll.currentView();
    if (sel.empty() || ! v || sel[0] >= (int) v->notes.size()) return;
    const auto& e = v->notes[(size_t) sel[0]].edit;
    noteStart->s.setValue (e.vibStart * 100.0, juce::dontSendNotification);
    noteEnd->s.setValue (e.vibEnd * 100.0, juce::dontSendNotification);
    noteSpeed->s.setValue (e.vibSpeedScale, juce::dontSendNotification);
    noteDepth->s.setValue (e.vibDepthScale, juce::dontSendNotification);
}

void CooltoneEditor::applySelectedNoteVibrato()
{
    const auto sel = roll.getSelection();
    if (sel.empty()) { say ("Select notes first", true); return; }
    proc.editNoteVibrato (sel, noteStart->s.getValue() / 100.0, noteEnd->s.getValue() / 100.0, noteSpeed->s.getValue(), noteDepth->s.getValue());
}

// ------------------------------------------------------------------------------------------
// presets
// ------------------------------------------------------------------------------------------
void CooltoneEditor::rebuildPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    for (int c = 0; c < ct::presetCategoryCount(); ++c)
    {
        presetBox.addSectionHeading (ct::presetCategoryName (c));
        for (int i = 0; i < ct::stylePresetCount(); ++i)
            if (ct::stylePresetCategory (i) == c) presetBox.addItem (juce::String::fromUTF8 (ct::stylePresetName (i)), 100 + i);
    }
    userPresetNames = proc.listUserPresets();
    presetBox.addSeparator();
    presetBox.addSectionHeading ("User Presets");
    for (int i = 0; i < userPresetNames.size(); ++i) presetBox.addItem (userPresetNames[i], 1000 + i);
}

void CooltoneEditor::askName (const juce::String& title, const juce::String& def, std::function<void (juce::String)> cb)
{
    auto* w = new juce::AlertWindow (title, "Preset name:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", def);
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, cb] (int r) { if (r == 1) cb (w->getTextEditorContents ("name")); }), true);
}

void CooltoneEditor::showPresetMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Save preset as...");
    m.addItem (2, "Rename user preset...", userPresetNames.size() > 0);
    m.addItem (3, "Delete user preset", userPresetNames.size() > 0);
    m.addSeparator();
    m.addItem (4, "Import preset file...");
    m.addItem (5, "Export current settings...");
    m.addItem (6, "Open presets folder");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetMenuBtn), [this] (int r)
    {
        const int id = presetBox.getSelectedId();
        const bool isUser = id >= 1000 && (id - 1000) < userPresetNames.size();
        const juce::String cur = isUser ? userPresetNames[id - 1000] : juce::String();
        if (r == 1) askName ("Save preset", "My preset", [this] (juce::String n) { if (proc.saveUserPreset (n)) { rebuildPresetBox(); say ("Preset saved: " + n); } else say ("Could not save the preset", true); });
        else if (r == 2 && isUser) askName ("Rename preset", cur, [this, cur] (juce::String n) { if (proc.renameUserPreset (cur, n)) rebuildPresetBox(); });
        else if (r == 2 || r == 3) { if (isUser) { proc.deleteUserPreset (cur); rebuildPresetBox(); } else say ("Select a user preset first", true); }
        else if (r == 4)
        {
            chooser = std::make_unique<juce::FileChooser> ("Import preset", juce::File(), "*.ctpreset");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
            { const auto f = fc.getResult(); if (f.existsAsFile()) { f.copyFileTo (CooltoneProcessor::userPresetDir().getChildFile (f.getFileName())); rebuildPresetBox(); } });
        }
        else if (r == 5)
        {
            chooser = std::make_unique<juce::FileChooser> ("Export settings", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("COOLTONE preset.ctpreset"), "*.ctpreset");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
            { const auto f = fc.getResult(); if (f != juce::File()) f.replaceWithText (juce::String (ct::serializeSettings (proc.readSettings()))); });
        }
        else if (r == 6) CooltoneProcessor::userPresetDir().revealToUser();
    });
}

void CooltoneEditor::showSettingsMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Window size");
    m.addItem (11, "Small (960 x 580)"); m.addItem (12, "Medium (1280 x 760)"); m.addItem (13, "Large (1680 x 940)"); m.addItem (14, "Full Editor (2200 x 1250)");
    m.addSeparator();
    m.addSectionHeader ("Export format");
    m.addItem (21, "24-bit PCM", true, proc.exportFormat == ct::WavFormat::PCM24);
    m.addItem (22, "32-bit float", true, proc.exportFormat == ct::WavFormat::Float32);
    m.addItem (23, "16-bit PCM", true, proc.exportFormat == ct::WavFormat::PCM16);
    m.addSeparator();
    m.addItem (31, "About COOLTONE");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsBtn), [this] (int r)
    {
        if (r >= 11 && r <= 14) { static const int w[4] = { 960, 1280, 1680, 2200 }, h[4] = { 580, 760, 940, 1250 }; setSize (w[r - 11], h[r - 11]); }
        else if (r >= 21 && r <= 23) { formatBox.setSelectedId (r - 20, juce::sendNotificationSync); }
        else if (r == 31) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "COOLTONE", "COOLTONE by ORYMY - YMY RECORDS\nVocal tuning, Magic Vibrato and expression.\nVersion 1.0.0");
    });
}

// ------------------------------------------------------------------------------------------
// files / export
// ------------------------------------------------------------------------------------------
void CooltoneEditor::chooseAndLoadFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Select a vocal recording", juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    { const auto f = fc.getResult(); if (f.existsAsFile()) proc.requestLoadFile (f); });
}

bool CooltoneEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}
void CooltoneEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& f : files) if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg")) { proc.requestLoadFile (juce::File (f)); return; }
}

ct::ExportOptions CooltoneEditor::makeOptions (bool& ok)
{
    ct::ExportOptions o; o.format = proc.exportFormat; o.selectionOnly = selOnlyBtn.getToggleState(); ok = true;
    if (o.selectionOnly)
    {
        double a = 0, b = 0;
        if (! roll.getSelectionSpan (a, b)) { ok = false; return o; }
        o.selStartSec = a; o.selEndSec = b;
    }
    return o;
}

void CooltoneEditor::doExportAudio()
{
    bool ok = true; const auto o = makeOptions (ok);
    if (! ok) { say ("Select notes first (Selection only is on)", true); return; }
    const auto src = (ExportSource) (sourceBox.getSelectedId() - 1);
    chooser = std::make_unique<juce::FileChooser> ("Export audio", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("COOLTONE vocal.wav"), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, o, src] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        f = f.withFileExtension ("wav");
        const auto err = proc.exportWav (f, o, src);
        if (err.isEmpty()) { lastExport = f; say ("Exported: " + f.getFileName()); }
        else { say (err, true); juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export", err); }
    });
}

void CooltoneEditor::doExportMidi()
{
    chooser = std::make_unique<juce::FileChooser> ("Export MIDI", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("COOLTONE notes.mid"), "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        f = f.withFileExtension ("mid");
        const auto err = proc.exportMidi (f);
        if (err.isEmpty()) say ("MIDI exported (notes and durations only - pitch curves and vibrato are not stored in MIDI)");
        else say (err, true);
    });
}
