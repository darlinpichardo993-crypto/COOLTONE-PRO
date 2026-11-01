#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <limits>

// ============================================================================================
// construction / parameters
// ============================================================================================
juce::AudioProcessorValueTreeState::ParameterLayout CooltoneProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& sp : ct::paramSpecs())
    {
        const juce::ParameterID pid { sp.id, 1 };
        if (sp.kind == ct::ParamKind::Float)
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, sp.label,
                juce::NormalisableRange<float> ((float) sp.minV, (float) sp.maxV, (float) sp.step), (float) sp.defV,
                juce::AudioParameterFloatAttributes().withLabel (sp.unit)));
        else if (sp.kind == ct::ParamKind::Choice)
        {
            juce::StringArray items;
            for (const auto& c : sp.choices) items.add (juce::String::fromUTF8 (c.c_str()));
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, sp.label, items, (int) sp.defV));
        }
        else
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, sp.label, sp.defV > 0.5));
    }
    return layout;
}

CooltoneProcessor::CooltoneProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "COOLTONE", createLayout()),
      worker (*this)
{
    const auto& sp = ct::paramSpecs();
    for (int i = 0; i < ct::pCount; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue (sp[(size_t) i].id);
        apvts.addParameterListener (sp[(size_t) i].id, this);
    }
    worker.startThread();
    startTimerHz (20);
}

CooltoneProcessor::~CooltoneProcessor()
{
    stopTimer();
    const auto& sp = ct::paramSpecs();
    for (int i = 0; i < ct::pCount; ++i) apvts.removeParameterListener (sp[(size_t) i].id, this);
    worker.signalThreadShouldExit();
    worker.wake.signal();
    worker.stopThread (4000);
}

ct::Settings CooltoneProcessor::readSettings() const
{
    ct::Settings s;
    for (int i = 0; i < ct::pCount; ++i) s.v[(size_t) i] = (double) raw[(size_t) i]->load();
    return s;
}

void CooltoneProcessor::setParam (int id, double value)
{
    if (auto* p = apvts.getParameter (ct::paramSpecs()[(size_t) id].id))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) value));
}

void CooltoneProcessor::loadSettings (const ct::Settings& s)
{
    suppressTicks = 3;
    pendingMode = -1; pendingVibStyle = -1; scaleChanged = false;
    for (int i = 0; i < ct::pCount; ++i) setParam (i, s.v[(size_t) i]);
    dirty = true;
}

void CooltoneProcessor::applyFactoryPreset (int index)
{
    auto s = readSettings();
    ct::applyStylePreset (s, index);
    s.set (ct::pCorrectionOn, 1.0);
    loadSettings (s);
}

void CooltoneProcessor::parameterChanged (const juce::String& id, float value)
{
    dirty = true;
    if (id == "mode") pendingMode = (int) value;
    else if (id == "vibStyle") pendingVibStyle = (int) value;
    else if (id == "keyRoot" || id == "scale") scaleChanged = true;
}

void CooltoneProcessor::timerCallback()
{
    if (suppressTicks.load() > 0)
    {
        --suppressTicks;
        pendingMode = -1; pendingVibStyle = -1; scaleChanged = false;
    }
    else
    {
        const int m = pendingMode.exchange (-1);
        if (m >= 0)
        {
            auto s = readSettings(); ct::applyCorrectionMode (s, m);
            for (int id : { (int) ct::pStrength, (int) ct::pRetune, (int) ct::pHumanize, (int) ct::pTransition, (int) ct::pDrift, (int) ct::pFormantKeep })
                setParam (id, s.v[(size_t) id]);
        }
        const int v = pendingVibStyle.exchange (-1);
        if (v >= 0)
        {
            auto s = readSettings(); ct::applyVibratoStyle (s, v);
            for (int id : { (int) ct::pVibRate, (int) ct::pVibDepth, (int) ct::pVibDelay, (int) ct::pVibAttack, (int) ct::pVibRelease,
                            (int) ct::pVibShape, (int) ct::pVibRandom, (int) ct::pVibHuman, (int) ct::pVibSmooth, (int) ct::pVibVariation })
                setParam (id, s.v[(size_t) id]);
        }
        if (scaleChanged.exchange (false))
        {
            auto s = readSettings(); ct::applyScaleToNoteParams (s);
            for (int i = 0; i < 12; ++i) setParam (ct::pNote0 + i, s.v[(size_t) (ct::pNote0 + i)]);
        }
    }
    if (dirty.exchange (false)) requestRender();
    if (captureFinished.exchange (false)) finalizeCapture();
}

// ---- user presets ----
juce::File CooltoneProcessor::userPresetDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("YMY RECORDS").getChildFile ("COOLTONE").getChildFile ("Presets");
    d.createDirectory();
    return d;
}
juce::StringArray CooltoneProcessor::listUserPresets() const
{
    juce::StringArray names;
    for (const auto& f : userPresetDir().findChildFiles (juce::File::findFiles, false, "*.ctpreset")) names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}
bool CooltoneProcessor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    return userPresetDir().getChildFile (clean + ".ctpreset").replaceWithText (juce::String (ct::serializeSettings (readSettings())));
}
bool CooltoneProcessor::loadUserPreset (const juce::String& name)
{
    const auto f = userPresetDir().getChildFile (name + ".ctpreset");
    if (! f.existsAsFile()) return false;
    auto s = readSettings();
    if (! ct::parseSettings (f.loadFileAsString().toStdString(), s)) return false;
    loadSettings (s);
    return true;
}
bool CooltoneProcessor::deleteUserPreset (const juce::String& name) { return userPresetDir().getChildFile (name + ".ctpreset").deleteFile(); }
bool CooltoneProcessor::renameUserPreset (const juce::String& a, const juce::String& b)
{
    const auto clean = juce::File::createLegalFileName (b.trim());
    if (clean.isEmpty()) return false;
    return userPresetDir().getChildFile (a + ".ctpreset").moveFileTo (userPresetDir().getChildFile (clean + ".ctpreset"));
}
void CooltoneProcessor::toggleAB()
{
    abSlot[(size_t) abCurrent] = readSettings(); abInit[(size_t) abCurrent] = true;
    abCurrent ^= 1;
    if (! abInit[(size_t) abCurrent]) abSlot[(size_t) abCurrent] = abSlot[(size_t) (abCurrent ^ 1)];
    loadSettings (abSlot[(size_t) abCurrent]);
}

// ============================================================================================
// audio thread
// ============================================================================================
void CooltoneProcessor::prepareToPlay (double sr, int)
{
    const bool changed = std::abs (sr - currentSR) > 0.5;
    currentSR = sr;
    setLatencySamples (0);   // everything is rendered offline: no latency compensation needed
    if (changed)
    {
        juce::String path; { juce::ScopedLock sl (modelLock); path = clipPath; }
        if (path.isNotEmpty()) requestLoadFile (juce::File (path));
    }
}

bool CooltoneProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return l.getMainInputChannelSet() == out;
}

void CooltoneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(), nch = buffer.getNumChannels();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n <= 0 || nch <= 0 || raw[ct::pBypass]->load() > 0.5f) return;

    const float gDb = raw[ct::pInGain]->load();
    if (std::abs (gDb) > 0.01f) buffer.applyGain (juce::Decibels::decibelsToGain (gDb));
    {
        float pk = 0.f;
        for (int c = 0; c < nch; ++c) pk = std::max (pk, buffer.getMagnitude (c, 0, n));
        inPeak.store (std::max (pk, inPeak.load() * 0.92f));
        if (pk >= 0.999f) inputClipped.store (true);
    }

    bool haveHost = false, hostPlaying = false;
    juce::int64 hostPos = 0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            haveHost = true; hostPlaying = pos->getIsPlaying();
            if (auto t = pos->getTimeInSamples()) hostPos = *t;
            if (auto b = pos->getBpm()) lastBpm.store (*b);
        }

    // ---- record voice (input is monitored: it stays in the buffer) ----
    if (capturing.load())
    {
        // Never block the audio thread. If the UI thread is preparing/finalizing a take,
        // skip this callback rather than touching a buffer being resized or released.
        juce::SpinLock::ScopedTryLockType captureTry (captureLock);
        if (captureTry.isLocked() && capturing.load())
        {
            if (captureStopRequested.exchange (false)) { capturing = false; captureFinished = true; }
            else if (haveHost && ! hostPlaying) { if (capturePos.load() > 0) { capturing = false; captureFinished = true; } }
            else
            {
                const int pos = capturePos.load(), cap = captureBuffer.getNumSamples();
                if (pos == 0) captureStartHost = haveHost ? hostPos : 0;
                const int ncopy = std::max (0, std::min (n, cap - pos));
                for (int c = 0; c < captureBuffer.getNumChannels(); ++c)
                    captureBuffer.copyFrom (c, pos, buffer, std::min (c, nch - 1), 0, ncopy);
                capturePos.store (pos + ncopy);
                if (ncopy < n || pos + ncopy >= cap) { capturing = false; captureFinished = true; }
            }
        }
    }

    // ---- playback of the processed take (follows the host transport, or the internal one) ----
    bool active = false;
    juce::int64 pos = 0;
    const bool hostDrives = haveHost && hostPlaying;
    if (hostDrives) { active = true; pos = hostPos - clipOffsetSamples.load(); lastPlayPos.store (pos); }
    else if (internalPlaying.load()) { active = true; pos = internalPos.load(); lastPlayPos.store (pos); }

    if (active && ! capturing.load())
    {
        juce::SpinLock::ScopedTryLockType tl (playbackLock);
        if (tl.isLocked() && playback && ! playback->ch.empty())
        {
            const auto& r = *playback;
            const juce::int64 len = (juce::int64) r.ch[0].size();
            const bool loop = loopEnabled.load() && ! hostDrives;
            const juce::int64 ls = loopStart.load(), le = loopEnd.load();
            for (int c = 0; c < nch; ++c)
            {
                const auto& src = r.ch[(size_t) std::min (c, (int) r.ch.size() - 1)];
                float* dst = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                {
                    juce::int64 idx = pos + i;
                    if (loop && le > ls && idx >= le) idx = ls + (idx - ls) % (le - ls);
                    if (idx >= 0 && idx < len) dst[i] = src[(size_t) idx];
                }
            }
            if (! hostDrives)
            {
                juce::int64 np = pos + n;
                if (loop && le > ls && np >= le) np = ls + (np - ls) % (le - ls);
                internalPos.store (np);
                if (! loop && np >= len) internalPlaying = false;
            }
        }
        else if (! hostDrives) internalPos.store (pos + n);
    }
    else if (active && ! hostDrives) internalPos.store (pos + n);

    float pk = 0.f;
    for (int c = 0; c < nch; ++c) pk = std::max (pk, buffer.getMagnitude (c, 0, n));
    outPeak.store (std::max (pk, outPeak.load() * 0.92f));
}

// ============================================================================================
// worker thread: file import, analysis, offline render
// ============================================================================================
void CooltoneProcessor::requestRender() { renderRequested = true; worker.wake.signal(); }

void CooltoneProcessor::setStatus (const juce::String& s) { const juce::ScopedLock sl (statusLock); status = s; }
juce::String CooltoneProcessor::getStatus() const { const juce::ScopedLock sl (statusLock); return status; }

void CooltoneProcessor::requestLoadFile (const juce::File& f)
{
    { const juce::ScopedLock sl (jobLock); pendingFile = f; hasPendingFile = true; }
    setStatus ("Loading " + f.getFileName() + " ...");
    worker.wake.signal();
}

void CooltoneProcessor::workerRun (juce::Thread& t)
{
    while (! t.threadShouldExit())
    {
        worker.wake.wait (200);
        if (t.threadShouldExit()) break;
        juce::File f; bool hasF = false;
        std::shared_ptr<const ct::AudioClip> pc; juce::String pcPath; juce::int64 pcOff = 0;
        {
            const juce::ScopedLock sl (jobLock);
            if (hasPendingFile) { f = pendingFile; hasF = true; hasPendingFile = false; }
            if (pendingClip) { pc = std::move (pendingClip); pendingClip = nullptr; pcPath = pendingClipPath; pcOff = pendingClipOffset; }
        }
        // A malformed file, allocation failure, or unexpected DSP exception must not
        // unwind out of the worker thread and terminate the host process.
        try { if (hasF) doLoadFile (f); }
        catch (const std::exception& e) { setStatus ("Load failed safely: " + juce::String (e.what())); }
        catch (...) { setStatus ("Load failed safely: unexpected error"); }
        try
        {
            if (pc) { { const juce::ScopedLock sl (modelLock); clipPath = pcPath; } clipOffsetSamples = pcOff; doAnalyse (pc); }
        }
        catch (const std::exception& e) { setStatus ("Analysis failed safely: " + juce::String (e.what())); }
        catch (...) { setStatus ("Analysis failed safely: unexpected error"); }
        if (renderRequested.exchange (false))
        {
            try { doRender(); }
            catch (const std::exception& e) { setStatus ("Render failed safely: " + juce::String (e.what())); }
            catch (...) { setStatus ("Render failed safely: unexpected error"); }
        }
    }
}

void CooltoneProcessor::doLoadFile (const juce::File& f)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr) { setStatus ("Cannot read " + f.getFileName()); return; }
    const double targetSR = currentSR.load();
    if (! std::isfinite (reader->sampleRate) || reader->sampleRate < 8000.0 || reader->sampleRate > 384000.0
        || ! std::isfinite (targetSR) || targetSR < 8000.0 || targetSR > 384000.0
        || reader->numChannels == 0 || reader->lengthInSamples < 4096
        || reader->lengthInSamples > 600.0 * reader->sampleRate
        || reader->lengthInSamples > std::numeric_limits<int>::max())
    { setStatus ("Unsupported or invalid audio file"); return; }
    const int nch = (int) std::min<unsigned int> (2u, reader->numChannels);
    const int len = (int) reader->lengthInSamples;
    // Keep source and resampled buffers within a conservative memory budget.
    constexpr std::uint64_t maxAudioSamples = 50000000ull;
    if (static_cast<std::uint64_t> (len) * static_cast<std::uint64_t> (nch) > maxAudioSamples)
    { setStatus ("Audio file exceeds the safe memory limit"); return; }
    juce::AudioBuffer<float> buf (nch, len + 16);
    buf.clear();
    if (! reader->read (&buf, 0, len, 0, true, nch > 1))
    { setStatus ("Could not decode the complete audio file"); return; }

    auto c = std::make_shared<ct::AudioClip>();
    c->sampleRate = (int) std::lround (targetSR);
    c->ch.resize ((size_t) nch);
    const double ratio = reader->sampleRate / targetSR;
    for (int ch = 0; ch < nch; ++ch)
    {
        if (std::abs (ratio - 1.0) < 1e-9) c->ch[(size_t) ch].assign (buf.getReadPointer (ch), buf.getReadPointer (ch) + len);
        else
        {
            const double outLength = std::ceil (static_cast<double> (len) / ratio);
            if (! std::isfinite (outLength) || outLength < 1.0
                || outLength * static_cast<double> (nch) > static_cast<double> (maxAudioSamples)
                || outLength > static_cast<double> (std::numeric_limits<int>::max()))
            { setStatus ("Resampled audio exceeds the safe memory limit"); return; }
            const int outLen = static_cast<int> (outLength);
            c->ch[(size_t) ch].assign ((size_t) outLen, 0.f);
            juce::LagrangeInterpolator li;
            li.process (ratio, buf.getReadPointer (ch), c->ch[(size_t) ch].data(), outLen);
        }
    }
    { const juce::ScopedLock sl (modelLock); clipPath = f.getFullPathName(); }
    doAnalyse (c);
}

void CooltoneProcessor::doAnalyse (std::shared_ptr<const ct::AudioClip> c)
{
    setStatus ("Analyzing voice ...");
    auto an = std::make_shared<ct::Analysis> (ct::analyseClip (*c));
    juce::String edits;
    { const juce::ScopedLock sl (modelLock); edits = pendingEdits; pendingEdits = {}; }
    std::shared_ptr<const ct::RenderOutput> old;
    {
        const juce::ScopedLock sl (modelLock);
        clip = c; analysis = an; notes = an->notes;
        if (edits.isNotEmpty()) { std::vector<ct::EditNote> parsed; if (ct::parseEdits (edits.toStdString(), parsed, an->frames.size(), an->frames, an->hop)) notes = parsed; }
        undoStack.clear(); redoStack.clear(); finalMidiView.reset();
    }
    { juce::SpinLock::ScopedLockType sl (playbackLock); old = playback; playback.reset(); }
    ++viewVersion;
    renderRequested = true;
    setStatus ("Ready - " + juce::String ((int) an->notes.size()) + " notes detected");
}

void CooltoneProcessor::doRender()
{
    std::shared_ptr<const ct::AudioClip> c; std::shared_ptr<const ct::Analysis> a; std::vector<ct::EditNote> n;
    { const juce::ScopedLock sl (modelLock); c = clip; a = analysis; n = notes; }
    if (! c || ! a) return;
    const ct::Settings s = readSettings();
    auto out = std::make_shared<ct::RenderOutput> (ct::renderProject (*c, *a, n, s));
    auto fm = std::make_shared<std::vector<double>> (out->finalMidi);
    std::shared_ptr<const ct::RenderOutput> old;
    { juce::SpinLock::ScopedLockType sl (playbackLock); old = playback; playback = out; }
    { const juce::ScopedLock sl (modelLock); finalMidiView = fm; }
    ++viewVersion;
    setStatus ("Ready - " + juce::String ((int) n.size()) + " notes");
}

std::shared_ptr<const ct::RenderOutput> CooltoneProcessor::currentRender() const
{
    juce::SpinLock::ScopedLockType sl (const_cast<juce::SpinLock&> (playbackLock));
    return playback;
}

// ============================================================================================
// recording
// ============================================================================================
void CooltoneProcessor::armCapture()
{
    // Finalize a just-stopped take before reusing the capture buffer.
    if (captureFinished.exchange (false)) finalizeCapture();
    if (capturing.load()) return;
    {
        const juce::SpinLock::ScopedLockType captureGuard (captureLock);
        captureChannels = std::max (1, std::min (2, getTotalNumInputChannels()));
        captureBuffer.setSize (captureChannels, (int) (currentSR * 180.0), false, true, false);
        capturePos = 0; captureStopRequested = false; captureFinished = false;
        internalPlaying = false;
        capturing = true;
    }
    setStatus ("Recording (3 min max) - press stop when finished");
}
void CooltoneProcessor::stopCapture() { if (capturing.load()) captureStopRequested = true; }

void CooltoneProcessor::finalizeCapture()
{
    auto c = std::make_shared<ct::AudioClip>();
    int n = 0;
    {
        // The callback may have set captureFinished just before returning. Waiting here is
        // safe (message thread only) and guarantees it no longer references captureBuffer.
        const juce::SpinLock::ScopedLockType captureGuard (captureLock);
        n = capturePos.load();
        if ((double) n >= currentSR * 0.2)
        {
            c->sampleRate = (int) std::lround (currentSR);
            for (int ch = 0; ch < captureBuffer.getNumChannels(); ++ch)
                c->ch.emplace_back (captureBuffer.getReadPointer (ch), captureBuffer.getReadPointer (ch) + n);
        }
        captureBuffer.setSize (0, 0);
    }
    if ((double) n < currentSR * 0.2) { setStatus ("Recording too short"); return; }
    { const juce::ScopedLock sl (jobLock); pendingClip = c; pendingClipPath = {}; pendingClipOffset = captureStartHost.load(); }
    setStatus ("Analyzing recording ...");
    worker.wake.signal();
}

bool CooltoneProcessor::hasClip() const { const juce::ScopedLock sl (modelLock); return clip != nullptr; }

// ============================================================================================
// transport
// ============================================================================================
void CooltoneProcessor::setInternalPlaying (bool p)
{
    if (p && hasClip()) { if (! loopEnabled.load()) {} internalPlaying = true; }
    else { internalPlaying = false; loopEnabled = false; }
}
void CooltoneProcessor::seekSeconds (double t) { const auto s = (juce::int64) std::max (0.0, t * currentSR); internalPos = s; lastPlayPos = s; }
void CooltoneProcessor::playSelectionLooped (double a, double b)
{
    if (! hasClip() || b <= a) return;
    loopStart = (juce::int64) (a * currentSR); loopEnd = (juce::int64) (b * currentSR);
    internalPos = loopStart.load(); loopEnabled = true; internalPlaying = true;
}
double CooltoneProcessor::getPlayheadSeconds() const { return (double) lastPlayPos.load() / currentSR; }

// ============================================================================================
// view / report
// ============================================================================================
std::shared_ptr<ViewData> CooltoneProcessor::getView() const
{
    auto v = std::make_shared<ViewData>();
    const juce::ScopedLock sl (modelLock);
    v->analysis = analysis; v->notes = notes; v->finalMidi = finalMidiView;
    v->sampleRate = clip ? (double) clip->sampleRate : currentSR;
    v->durationSec = clip ? clip->durationSec() : 0.0;
    return v;
}
ct::VoiceReport CooltoneProcessor::getReport() const
{
    const juce::ScopedLock sl (modelLock);
    if (! clip || ! analysis) return {};
    return ct::buildVoiceReport (*clip, *analysis, notes);
}

// ============================================================================================
// editing
// ============================================================================================
template <typename Fn> bool CooltoneProcessor::mutateNotes (Fn&& fn)
{
    {
        const juce::ScopedLock sl (modelLock);
        if (! analysis) return false;
        auto before = notes;
        if (! fn (notes, *analysis, readSettings())) { notes = before; return false; }
        undoStack.push_back (std::move (before));
        if (undoStack.size() > 100) undoStack.erase (undoStack.begin());
        redoStack.clear();
    }
    ++viewVersion;
    requestRender();
    return true;
}

static bool inSel (const std::vector<int>& sel, int i) { return std::find (sel.begin(), sel.end(), i) != sel.end(); }

bool CooltoneProcessor::editSplit (double t)
{
    return mutateNotes ([&] (std::vector<ct::EditNote>& n, const ct::Analysis& a, const ct::Settings&)
    { const int i = ct::findNoteAt (n, t); return i >= 0 && ct::splitNote (n, a.frames, a.hop, (size_t) i, t); });
}
bool CooltoneProcessor::editGlue (int index)
{
    return mutateNotes ([&] (std::vector<ct::EditNote>& n, const ct::Analysis& a, const ct::Settings&)
    { return index >= 0 && ct::glueNotes (n, a.frames, a.hop, (size_t) index); });
}
bool CooltoneProcessor::editDelete (const std::vector<int>& sel)
{
    return mutateNotes ([&] (std::vector<ct::EditNote>& n, const ct::Analysis&, const ct::Settings&)
    { bool any = false; for (int i : sel) if (i >= 0 && i < (int) n.size()) { n[(size_t) i].edit.deleted = true; any = true; } return any; });
}
bool CooltoneProcessor::editMoveNotes (const std::vector<int>& sel, double delta, bool snap)
{
    if (std::abs (delta) < 0.01) return false;
    return mutateNotes ([&] (std::vector<ct::EditNote>& n, const ct::Analysis&, const ct::Settings& s)
    {
        bool any = false;
        for (int i : sel)
        {
            if (i < 0 || i >= (int) n.size()) continue;
            auto& e = n[(size_t) i];
            double t = ct::noteBaseTarget (e, s) + delta;
            if (snap) t = ct::snapToScale (t, s);
            e.edit.hasTarget = true; e.edit.targetMidi = t; e.edit.melisma.clear(); any = true;
        }
        return any;
    });
}
bool CooltoneProcessor::editToggleVibrato (int index)
{
    return mutateNotes ([&] (std::vector<ct::EditNote>& n, const ct::Analysis&, const ct::Settings&)
    {
        if (index < 0 || index >= (int) n.size()) return false;
        auto& e = n[(size_t) index].edit; e.vibrato = ! e.vibrato;
        if (e.vibSeed == 1) e.vibSeed = ++seedCounter * 2654435761u + 5u;
        return true;
    });
}
int CooltoneProcessor::editMagicVibrato (const std::vector<int>& sel, bool regenerate)
{
    int count = 0;
    mutateNotes ([&] (std::vector<ct::EditNote>& n, const ct::Analysis& a, const ct::Settings& s)
    { count = ct::applyMagicVibrato (n, a.frames, a.hop, ct::magicSettingsFromSettings (s), sel, regenerate, ++seedCounter); return true; });
    return count;
}
bool CooltoneProcessor::editForceVibrato (const std::vector<int>& sel, bool on)
{
    return mutateNotes ([&] (std::vector<ct::EditNote>& n, const ct::Analysis&, const ct::Settings&)
    {
        bool any = false;
        for (size_t i = 0; i < n.size(); ++i)
        {
            if (! sel.empty() && ! inSel (sel, (int) i)) continue;
            if (n[i].edit.deleted) continue;
            auto& e = n[i].edit;
            if (e.vibrato != on) any = true;
            e.vibrato = on;
            if (on) { if (e.vibSeed == 1) e.vibSeed = ++seedCounter * 2654435761u + (std::uint32_t) i; e.vibAmountScale = 1.0; e.vibDelayScale = 1.0; }
        }
        return any;
    });
}
bool CooltoneProcessor::editNoteVibrato (const std::vector<int>& sel, double start, double end, double spd, double dep)
{
    return mutateNotes ([&] (std::vector<ct::EditNote>& n, const ct::Analysis&, const ct::Settings&)
    {
        bool any = false;
        for (int i : sel)
        {
            if (i < 0 || i >= (int) n.size()) continue;
            auto& e = n[(size_t) i].edit;
            e.vibStart = std::max (0.0, std::min (0.95, start)); e.vibEnd = std::max (e.vibStart + 0.05, std::min (1.0, end));
            e.vibSpeedScale = spd; e.vibDepthScale = dep; e.vibrato = true;
            if (e.vibSeed == 1) e.vibSeed = ++seedCounter * 2654435761u + (std::uint32_t) i;
            any = true;
        }
        return any;
    });
}
bool CooltoneProcessor::editResetVibratoCurve (const std::vector<int>& sel)
{
    return mutateNotes ([&] (std::vector<ct::EditNote>& n, const ct::Analysis&, const ct::Settings&)
    {
        bool any = false;
        for (size_t i = 0; i < n.size(); ++i)
            if (sel.empty() || inSel (sel, (int) i))
            { auto& e = n[i].edit; e.vibStart = 0; e.vibEnd = 1; e.vibSpeedScale = 1; e.vibDepthScale = 1; e.vibAmountScale = 1; e.vibDelayScale = 1; any = true; }
        return any;
    });
}
bool CooltoneProcessor::editRandomizeVibrato (const std::vector<int>& sel)
{
    return mutateNotes ([&] (std::vector<ct::EditNote>& n, const ct::Analysis&, const ct::Settings&)
    {
        bool any = false;
        for (size_t i = 0; i < n.size(); ++i)
            if ((sel.empty() || inSel (sel, (int) i)) && n[i].edit.vibrato) { n[i].edit.vibSeed = ++seedCounter * 2654435761u + (std::uint32_t) i * 977u; any = true; }
        return any;
    });
}
bool CooltoneProcessor::editApplyMelisma (const std::vector<int>& sel)
{
    return mutateNotes ([&] (std::vector<ct::EditNote>& n, const ct::Analysis&, const ct::Settings& s)
    {
        bool any = false;
        const auto mp = ct::melismaParamsFromSettings (s);
        const double c0 = 12.0 * std::log2 (s.get (ct::pCenterHz) / 440.0);
        for (int i : sel)
        {
            if (i < 0 || i >= (int) n.size() || n[(size_t) i].edit.deleted) continue;
            auto& e = n[(size_t) i];
            auto steps = ct::generateMelisma (e.note.startSec, e.note.endSec, ct::noteBaseTarget (e, s) - c0, s.scaleMask(), mp, ++seedCounter);
            for (auto& st : steps) st.midi += c0;
            if (! steps.empty()) { e.edit.melisma = steps; e.edit.hasTarget = false; any = true; }
        }
        return any;
    });
}
bool CooltoneProcessor::editClearMelisma (const std::vector<int>& sel)
{
    return mutateNotes ([&] (std::vector<ct::EditNote>& n, const ct::Analysis&, const ct::Settings&)
    { bool any = false; for (int i : sel) if (i >= 0 && i < (int) n.size() && ! n[(size_t) i].edit.melisma.empty()) { n[(size_t) i].edit.melisma.clear(); any = true; } return any; });
}

void CooltoneProcessor::autoTune() { setParam (ct::pCorrectionOn, 1.0); dirty = true; requestRender(); }

ct::KeyEstimate CooltoneProcessor::detectKey (bool apply)
{
    ct::KeyEstimate est;
    { const juce::ScopedLock sl (modelLock); est = ct::detectKeyFromNotes (notes, (double) raw[ct::pCenterHz]->load()); }
    if (apply && est.correlation > -1.5)
    {
        setParam (ct::pKeyRoot, est.root);
        setParam (ct::pScale, est.minor ? (int) ct::ScaleType::NaturalMinor : (int) ct::ScaleType::Major);
    }
    return est;
}

void CooltoneProcessor::undo()
{
    { const juce::ScopedLock sl (modelLock); if (undoStack.empty()) return; redoStack.push_back (notes); notes = undoStack.back(); undoStack.pop_back(); }
    ++viewVersion; requestRender();
}
void CooltoneProcessor::redo()
{
    { const juce::ScopedLock sl (modelLock); if (redoStack.empty()) return; undoStack.push_back (notes); notes = redoStack.back(); redoStack.pop_back(); }
    ++viewVersion; requestRender();
}

// ============================================================================================
// export
// ============================================================================================
static juce::String sourceName (ExportSource s)
{
    switch (s) { case ExportSource::Dry: return "dry"; case ExportSource::AutoTuneOnly: return "autotune"; case ExportSource::VibratoOnly: return "vibrato"; default: return "processed"; }
}

juce::String CooltoneProcessor::exportWav (const juce::File& dest, const ct::ExportOptions& o, ExportSource src)
{
    std::shared_ptr<const ct::AudioClip> c; std::shared_ptr<const ct::Analysis> a; std::vector<ct::EditNote> n;
    { const juce::ScopedLock sl (modelLock); c = clip; a = analysis; n = notes; }
    if (! c || ! a) return "No audio loaded";

    std::shared_ptr<const ct::RenderOutput> out;
    if (src == ExportSource::Processed) out = currentRender();
    else if (src == ExportSource::Dry) { auto d = std::make_shared<ct::RenderOutput>(); d->sampleRate = c->sampleRate; d->ch = c->ch; out = d; }
    else
    {
        ct::Settings s = readSettings();
        if (src == ExportSource::AutoTuneOnly) { s.set (ct::pCorrectionOn, 1.0); for (auto& e : n) e.edit.vibrato = false; }
        else { s.set (ct::pCorrectionOn, 0.0); for (auto& e : n) { e.edit.hasTarget = false; e.edit.melisma.clear(); } }
        out = std::make_shared<ct::RenderOutput> (ct::renderProject (*c, *a, n, s));
    }
    if (! out || out->ch.empty()) return "Nothing rendered yet - wait for the analysis to finish";

    const auto w = ct::buildExport (out->ch, out->sampleRate, o);
    std::string err;
    if (! ct::writeWav (dest.getFullPathName().toStdString(), w, &err)) return "Could not write the file: " + juce::String (err);
    ct::WavData check;
    if (! ct::readWav (dest.getFullPathName().toStdString(), check, &err) || check.frames() != w.frames() || check.sampleRate != w.sampleRate)
        return "The exported file failed validation: " + juce::String (err);
    return {};
}

juce::File CooltoneProcessor::prepareDragFile (const ct::ExportOptions& o, ExportSource src, juce::String& error)
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("COOLTONE_export");
    dir.createDirectory();
    for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.wav"))   // tidy files older than a day
        if (f.getCreationTime() < juce::Time::getCurrentTime() - juce::RelativeTime::days (1)) f.deleteFile();
    const auto f = dir.getChildFile ("COOLTONE_" + sourceName (src) + "_" + juce::String (juce::Time::currentTimeMillis()) + ".wav");
    error = exportWav (f, o, src);
    if (error.isNotEmpty()) return {};
    if (! f.existsAsFile() || f.getSize() < 44) { error = "The exported file is not accessible"; return {}; }
    return f;
}

juce::String CooltoneProcessor::exportMidi (const juce::File& dest)
{
    std::vector<ct::EditNote> n;
    { const juce::ScopedLock sl (modelLock); n = notes; }
    if (n.empty()) return "No notes to export";
    double bpm = lastBpm.load(); if (bpm < 20.0 || bpm > 400.0) bpm = 120.0;
    std::string err;
    if (! ct::writeMidiFile (dest.getFullPathName().toStdString(), n, readSettings(), bpm, &err)) return juce::String (err);
    return {};
}

// ============================================================================================
// state
// ============================================================================================
void CooltoneProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml == nullptr) return;
    {
        const juce::ScopedLock sl (modelLock);
        xml->setAttribute ("clipPath", clipPath);
        xml->setAttribute ("clipOffset", juce::String ((juce::int64) clipOffsetSamples.load()));
        xml->setAttribute ("edits", juce::String (ct::serializeEdits (notes)));
    }
    xml->setAttribute ("uiW", uiWidth); xml->setAttribute ("uiH", uiHeight); xml->setAttribute ("exportFmt", (int) exportFormat);
    copyXmlToBinary (*xml, dest);
}

void CooltoneProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr) return;
    if (xml->hasTagName (apvts.state.getType().toString()))
    {
        suppressTicks = 3;
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        pendingMode = -1; pendingVibStyle = -1; scaleChanged = false;
    }
    uiWidth = xml->getIntAttribute ("uiW", 1280); uiHeight = xml->getIntAttribute ("uiH", 760);
    exportFormat = (ct::WavFormat) juce::jlimit (0, 2, xml->getIntAttribute ("exportFmt", (int) ct::WavFormat::PCM24));
    const auto path = xml->getStringAttribute ("clipPath");
    if (path.isNotEmpty() && juce::File (path).existsAsFile())
    {
        { const juce::ScopedLock sl (modelLock); pendingEdits = xml->getStringAttribute ("edits"); }
        clipOffsetSamples = xml->getStringAttribute ("clipOffset", "0").getLargeIntValue();
        requestLoadFile (juce::File (path));
    }
    dirty = true;
}

juce::AudioProcessorEditor* CooltoneProcessor::createEditor() { return new CooltoneEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CooltoneProcessor(); }
