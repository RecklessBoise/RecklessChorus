#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "dsp/ChorusCore.h"

class RecklessChorusProcessor final : public juce::AudioProcessor,
                                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    RecklessChorusProcessor();
    ~RecklessChorusProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    float getLfoPhase() const noexcept { return core.getLfoPhase(); }

    bool isPresetModified() const noexcept { return presetModified.load(); }

    /** A/B comparison: swaps the live settings with the other slot. */
    void toggleAB();
    int getActiveSlot() const noexcept { return activeSlot; }

    /** Last editor width, persisted with the session. */
    int getEditorWidth() const noexcept { return editorWidth; }
    void setEditorWidth (int w) noexcept { editorWidth = w; }

    static constexpr int kDefaultEditorWidth = 760;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void applyPreset (int index);

    juce::AudioProcessorValueTreeState state;
    Parameters::Snapshot snapshot;
    reckless::ChorusCore core;

    int currentPreset = 0;
    std::atomic<bool> presetModified { false };
    std::atomic<bool> applyingPreset { false };

    std::array<juce::ValueTree, 2> abSlots;
    int activeSlot = 0;

    int editorWidth = kDefaultEditorWidth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecklessChorusProcessor)
};
