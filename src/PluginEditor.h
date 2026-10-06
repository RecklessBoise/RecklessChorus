#pragma once

#include "PluginProcessor.h"
#include "ui/GraphPanels.h"
#include "ui/PresetDialog.h"
#include "ui/RecklessLookAndFeel.h"
#include "ui/VoiceVisualizer.h"
#include "ui/Widgets.h"

/**
    The whole interface, laid out at a fixed design size. The editor scales it
    with an affine transform, so every pixel is vector-drawn at any size.
*/
class MainView final : public juce::Component
{
public:
    static constexpr int kWidth = 760;
    static constexpr int kHeight = 670;

    MainView (RecklessChorusProcessor&, RecklessLookAndFeel&);
    ~MainView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    /** Animation + state polling, called by the editor's timer. */
    void tick (double elapsedSeconds, bool refreshPanels);

    std::function<void (float)> onScaleChosen;
    std::function<float()> getCurrentScale;

private:
    void showPresetMenu();
    void showSaveDialog();
    void confirmDeletePreset();
    void showMessage (const juce::String& name, const juce::String& text);
    void showSizeMenu();
    void selectTab (bool analogSettings);
    void setStatus (const juce::String& paramId);

    RecklessChorusProcessor& processor;
    RecklessLookAndFeel& lnf;
    Parameters::Snapshot snapshot;

    // Header
    ArrowButton prevPreset { false }, nextPreset { true };
    FlatButton presetName { "Default", 14.0f };
    SaveButton savePreset;
    FlatButton abButton { "A", 12.5f };
    Knob mix;
    Fader output;

    // Main
    EngineSwitch engine;
    VoiceVisualizer visualizer;
    Knob speed, depth, quality, edge, voices;

    // Tabs
    FlatButton inputTab { "INPUT FILTERING" }, analogTab { "ANALOG SETTINGS" };
    IconToggle shape, wide;

    // Panels
    InputFilterPanel inputPanel;
    AliasingPanel aliasingPanel;
    StabilityPanel stabilityPanel;
    CompanderPanel companderPanel;

    // Status bar
    FlatButton sizeButton { "100%", 11.5f };
    juce::String statusName, statusText;
    juce::uint32 messageUntil = 0;

    PresetDialog dialog;

    bool showingAnalogSettings = true;
    bool lastDigital = false;
    float lastScale = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};

//==============================================================================
class RecklessChorusEditor final : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit RecklessChorusEditor (RecklessChorusProcessor&);
    ~RecklessChorusEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setScale (float scale);

    RecklessChorusProcessor& chorusProcessor;
    RecklessLookAndFeel lnf;
    MainView view;
    double lastTick = 0.0;
    int frame = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecklessChorusEditor)
};
