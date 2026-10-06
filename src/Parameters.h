#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/ChorusCore.h"

namespace ParamIDs
{
    inline constexpr auto engine     = "engine";
    inline constexpr auto speed      = "speed";
    inline constexpr auto depth      = "depth";
    inline constexpr auto voices     = "voices";
    inline constexpr auto quality    = "quality";
    inline constexpr auto edge       = "edge";
    inline constexpr auto shape      = "shape";
    inline constexpr auto wide       = "wide";
    inline constexpr auto mix        = "mix";
    inline constexpr auto output     = "output";
    inline constexpr auto lowCut     = "lowCut";
    inline constexpr auto highCut    = "highCut";
    inline constexpr auto rejection  = "rejection";
    inline constexpr auto emphasis   = "emphasis";
    inline constexpr auto noise      = "noise";
    inline constexpr auto variance   = "variance";
    inline constexpr auto reactivity = "reactivity";
    inline constexpr auto ratio      = "ratio";

    /** Every parameter, in declaration order. */
    const juce::StringArray& all();
}

namespace Parameters
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    /** Display name and one-line help text shown in the editor's status bar. */
    struct Info
    {
        juce::String name, description;
    };

    Info infoFor (const juce::String& paramId);

    /** Cached raw-value pointers for lock-free reads on the audio thread. */
    class Snapshot
    {
    public:
        explicit Snapshot (juce::AudioProcessorValueTreeState& state);
        reckless::ChorusParams read() const noexcept;

    private:
        std::atomic<float>* get (juce::AudioProcessorValueTreeState&, const char* id);

        std::atomic<float> *engine, *speed, *depth, *voices, *quality, *edge, *shape, *wide,
                           *mix, *output, *lowCut, *highCut, *rejection, *emphasis, *noise,
                           *variance, *reactivity, *ratio;
    };
}
