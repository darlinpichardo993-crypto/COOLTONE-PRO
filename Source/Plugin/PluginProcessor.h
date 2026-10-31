#pragma once
// COOLTONE by ORYMY - YMY RECORDS - JUCE plugin processor (VST3 + Standalone)
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <memory>

#include "AudioEngine.h"
#include "MelismaEngine.h"
#include "MidiExport.h"
#include "PresetManager.h"

namespace ct = cooltone;

struct ViewData
{
    std::shared_ptr<const ct::Analysis> analysis;
    std::vector<ct::EditNote> notes;
    std::shared_ptr<const std::vector<double>> finalMidi;
    double sampleRate = 44100.0;
    double durationSec = 0.0;
};

enum class ExportSource { Processed = 0, Dry, AutoTuneOnly, VibratoOnly };

class CooltoneProcessor : public juce::AudioProcessor,
                          private juce::AudioProcessorValueTreeState::Listener,
                          private juce::Timer
{
public:
    CooltoneProcessor();
    ~CooltoneProcessor() override;

    // ---- juce::AudioProcessor ----
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "COOLTONE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- parameters ----
    juce::AudioProcessorValueTreeState apvts;
    ct::Settings readSettings() const;
    void setParam (int id, double value);
    void loadSettings (const ct::Settings&);        // writes every parameter (used by presets / A-B)
    void applyFactoryPreset (int index);
    // user presets (text files)
    static juce::File userPresetDir();
    juce::StringArray listUserPresets() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::String& name);
    bool deleteUserPreset (const juce::String& name);
    bool renameUserPreset (const juce::String& oldName, const juce::String& newName);
    void toggleAB();
    int getABSlot() const { return abCurrent; }

    // ---- audio source: file import / recording ----
    void requestLoadFile (const juce::File&);
    void armCapture();
    void stopCapture();
    bool isCapturing() const { return capturing.load(); }
    bool hasClip() const;
    juce::String getStatus() const;

    // ---- transport (internal; the host transport takes over when it is playing) ----
    void setInternalPlaying (bool);
    bool isInternalPlaying() const { return internalPlaying.load(); }
    void seekSeconds (double);
    void playSelectionLooped (double startSec, double endSec);
    void stopLoop() { loopEnabled = false; }
    double getPlayheadSeconds() const;
    double getHostBpm() const { return lastBpm.load(); }

    // ---- view / report ----
    std::shared_ptr<ViewData> getView() const;
    std::uint64_t getViewVersion() const { return viewVersion.load(); }
    ct::VoiceReport getReport() const;
    std::atomic<float> inPeak { 0.f }, outPeak { 0.f };
    std::atomic<bool> inputClipped { false };

    // ---- editing (message thread) ----
    bool editSplit (double timeSec);
    bool editGlue (int index);
    bool editDelete (const std::vector<int>& sel);
    bool editMoveNotes (const std::vector<int>& sel, double deltaSemis, bool snap);
    bool editToggleVibrato (int index);
    int editMagicVibrato (const std::vector<int>& sel, bool regenerate);      // intelligent: only where it belongs
    bool editForceVibrato (const std::vector<int>& sel, bool on);             // Apply to Selected / All / Remove
    bool editNoteVibrato (const std::vector<int>& sel, double start, double end, double speedScale, double depthScale);
    bool editResetVibratoCurve (const std::vector<int>& sel);
    bool editRandomizeVibrato (const std::vector<int>& sel);
    bool editApplyMelisma (const std::vector<int>& sel);
    bool editClearMelisma (const std::vector<int>& sel);
    void autoTune();                                                           // enables correction and re-renders
    ct::KeyEstimate detectKey (bool apply);
    void undo();
    void redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }

    // ---- export ----
    juce::String exportWav (const juce::File& dest, const ct::ExportOptions&, ExportSource);
    juce::File prepareDragFile (const ct::ExportOptions&, ExportSource, juce::String& error);
    juce::String exportMidi (const juce::File& dest);
    ct::WavFormat exportFormat = ct::WavFormat::PCM24;
    int uiWidth = 1280, uiHeight = 760;

    // worker thread entry (public for the Worker helper class)
    void workerRun (juce::Thread&);

private:
    class Worker : public juce::Thread
    {
    public:
        explicit Worker (CooltoneProcessor& o) : juce::Thread ("COOLTONE worker"), owner (o) {}
        void run() override { owner.workerRun (*this); }
        juce::WaitableEvent wake;
        CooltoneProcessor& owner;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String& id, float) override;
    void timerCallback() override;
    void requestRender();
    void setClip (std::shared_ptr<const ct::AudioClip>, const juce::String& path, juce::int64 hostOffset);
    void finalizeCapture();
    void pushUndo();
    template <typename Fn> bool mutateNotes (Fn&&);
    std::shared_ptr<const ct::RenderOutput> currentRender() const;
    void doRender();
    void doLoadFile (const juce::File&);
    void doAnalyse (std::shared_ptr<const ct::AudioClip>);
    void setStatus (const juce::String&);

    std::array<std::atomic<float>*, ct::pCount> raw {};
    Worker worker;

    // model (guarded by modelLock)
    mutable juce::CriticalSection modelLock;
    std::shared_ptr<const ct::AudioClip> clip;
    std::shared_ptr<const ct::Analysis> analysis;
    std::vector<ct::EditNote> notes;
    std::shared_ptr<const std::vector<double>> finalMidiView;
    std::vector<std::vector<ct::EditNote>> undoStack, redoStack;
    juce::String clipPath;
    std::atomic<juce::int64> clipOffsetSamples { 0 };
    std::uint32_t seedCounter = 100;
    juce::String pendingEdits;

    // worker jobs (guarded by jobLock)
    juce::CriticalSection jobLock;
    juce::File pendingFile;
    bool hasPendingFile = false;
    std::shared_ptr<const ct::AudioClip> pendingClip;
    juce::String pendingClipPath;
    juce::int64 pendingClipOffset = 0;
    std::atomic<bool> renderRequested { false };
    mutable juce::CriticalSection statusLock;
    juce::String status;

    // rendered audio read by the audio thread (try-lock only there)
    juce::SpinLock playbackLock;
    std::shared_ptr<const ct::RenderOutput> playback;

    // parameter side effects
    std::atomic<int> pendingMode { -1 }, pendingVibStyle { -1 };
    std::atomic<bool> scaleChanged { false }, dirty { false };
    std::atomic<int> suppressTicks { 0 };
    std::atomic<std::uint64_t> viewVersion { 1 };
    std::array<ct::Settings, 2> abSlot;
    std::array<bool, 2> abInit { false, false };
    int abCurrent = 0;

    // audio thread state
    double currentSR = 44100.0;
    std::atomic<bool> internalPlaying { false }, loopEnabled { false };
    std::atomic<juce::int64> internalPos { 0 }, lastPlayPos { 0 }, loopStart { 0 }, loopEnd { 0 };
    std::atomic<double> lastBpm { 120.0 };
    std::atomic<bool> bypassFlag { false };

    // capture
    juce::AudioBuffer<float> captureBuffer;
    std::atomic<bool> capturing { false }, captureStopRequested { false }, captureFinished { false };
    std::atomic<int> capturePos { 0 };
    std::atomic<juce::int64> captureStartHost { 0 };
    int captureChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CooltoneProcessor)
};
