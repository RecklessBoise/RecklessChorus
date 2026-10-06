#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "dsp/ChorusCore.h"

/** Animated ring of voices: each chorus voice is a family of orbiting circles. */
class VoiceVisualizer final : public juce::Component
{
public:
    VoiceVisualizer();

    /** Called from the editor's timer with the latest LFO phase and parameters. */
    void update (float lfoPhase, const reckless::ChorusParams& params, double elapsedSeconds);

    void paint (juce::Graphics&) override;

private:
    float phase = 0.0f;
    float rotation = 0.0f;
    reckless::ChorusParams params;
};
