#pragma once
// COOLTONE by ORYMY - YMY RECORDS - plugin editor (resizable; panels scroll instead of overlapping or clipping)
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Look.h"
#include "PianoRoll.h"
#include "PluginProcessor.h"
#include "Widgets.h"

class CooltoneEditor : public juce::AudioProcessorEditor,
                       public juce::FileDragAndDropTarget,
                       private juce::Timer
{
public:
    explicit CooltoneEditor (CooltoneProcessor&);
    ~CooltoneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    struct NoteSlider;
    void timerCallback() override;
    void say (const juce::String&, bool error = false);
    void rebuildPresetBox();
    void showPresetMenu();
    void showSettingsMenu();
    void askName (const juce::String& title, const juce::String& def, std::function<void (juce::String)>);
    void chooseAndLoadFile();
    void doExportAudio();
    void doExportMidi();
    ct::ExportOptions makeOptions (bool& ok);
    juce::TextButton* makeButton (const juce::String& text, std::function<void()> onClick);
    template <typename Cell> Cell* make (ct::ParamId id);
    void addParam (Section*, ct::ParamId);
    void applySelectedNoteVibrato();
    void selectionChanged();
    void layoutPanels();

    CooltoneProcessor& proc;
    CooltoneLook look;
    PianoRoll roll;

    juce::ComboBox presetBox, keyBox, scaleBox, sourceBox, formatBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt;
    juce::ToggleButton bypassBtn { "Bypass" }, selOnlyBtn { "Selection only" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;
    juce::TextButton presetMenuBtn { "..." }, undoBtn { "Undo" }, redoBtn { "Redo" }, abBtn { "A/B" }, settingsBtn { "Settings" };
    juce::TextButton loadBtn { "Load WAV" }, recBtn { "REC" }, playBtn { "Play" }, stopBtn { "Stop" }, loopBtn { "Loop Sel." },
        detectBtn { "Detect Key" }, autoBtn { "AUTO TUNE" };
    juce::TextButton toolBtns[7];
    juce::Label bpmLabel, statusLabel, keyInfo;
    juce::TextEditor reportBox;

    juce::OwnedArray<juce::Component> owned;
    juce::OwnedArray<Section> sections;
    StackPanel rightPanel { false }, bottomPanel { true };
    juce::Viewport rightVp, bottomVp;
    NoteSlider* noteStart = nullptr; NoteSlider* noteEnd = nullptr; NoteSlider* noteSpeed = nullptr; NoteSlider* noteDepth = nullptr;
    DragToPlaylist dragArea;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::StringArray userPresetNames;
    juce::String transientMsg; juce::int64 transientUntil = 0;
    int reportTick = 0;
    juce::File lastExport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CooltoneEditor)
};
