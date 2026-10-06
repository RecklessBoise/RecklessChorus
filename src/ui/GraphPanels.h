#pragma once

#include "Widgets.h"
#include "dsp/ChorusCore.h"

#include <array>

/** Bottom-row panel: title, a small live graph and two value fields. */
class GraphPanel : public juce::Component
{
public:
    struct Field
    {
        const char* paramId;
        const char* label;
    };

    GraphPanel (juce::String title, juce::AudioProcessorValueTreeState& state, Field first, Field second);

    /** Called from the editor timer; repaints only when something visible changed. */
    void refresh (const reckless::ChorusParams& p);

    void paint (juce::Graphics&) override;
    void resized() override;

protected:
    virtual void drawGraph (juce::Graphics&, juce::Rectangle<float> area) = 0;
    virtual bool isAnimated() const { return false; }

    static constexpr double kDisplayRate = 48000.0;

    reckless::ChorusParams params;
    double time = 0.0;

private:
    static std::array<float, 11> keyOf (const reckless::ChorusParams&);

    juce::String title;
    ValueField first, second;
    std::array<float, 11> lastKey {};
};

//==============================================================================
class AliasingPanel final : public GraphPanel
{
public:
    explicit AliasingPanel (juce::AudioProcessorValueTreeState&);

private:
    void drawGraph (juce::Graphics&, juce::Rectangle<float>) override;
};

class StabilityPanel final : public GraphPanel
{
public:
    explicit StabilityPanel (juce::AudioProcessorValueTreeState&);

private:
    void drawGraph (juce::Graphics&, juce::Rectangle<float>) override;
    bool isAnimated() const override { return params.noise > 0.0f || params.variance > 0.0f; }
};

class CompanderPanel final : public GraphPanel
{
public:
    explicit CompanderPanel (juce::AudioProcessorValueTreeState&);

private:
    void drawGraph (juce::Graphics&, juce::Rectangle<float>) override;
};

class InputFilterPanel final : public GraphPanel
{
public:
    explicit InputFilterPanel (juce::AudioProcessorValueTreeState&);

private:
    void drawGraph (juce::Graphics&, juce::Rectangle<float>) override;
};
