#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Palette
{
    inline const juce::Colour background { 0xff2a2c30 };
    inline const juce::Colour header     { 0xff1b1c1f };
    inline const juce::Colour panel      { 0xff1f2023 };
    inline const juce::Colour panelLine  { 0xff35373c };
    inline const juce::Colour knobTop    { 0xff3b3d42 };
    inline const juce::Colour knobBottom { 0xff222327 };
    inline const juce::Colour knobRim    { 0xff141517 };
    inline const juce::Colour track      { 0xff46484e };
    inline const juce::Colour text       { 0xffeaeaeb };
    inline const juce::Colour textDim    { 0xff7e8188 };
    inline const juce::Colour analog     { 0xffff6a1f };
    inline const juce::Colour digital    { 0xff3cc8ff };
    inline const juce::Colour graph      { 0xff3b8cff };

    inline juce::Colour accentFor (bool digitalEngine) { return digitalEngine ? digital : analog; }
}

namespace Fonts
{
    juce::Font bold (float height);
    juce::Font regular (float height);
}

class RecklessLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RecklessLookAndFeel();

    /** Accent colour of the active engine, used for highlighted controls. */
    void setAccent (juce::Colour c) { accent = c; }
    juce::Colour getAccent() const { return accent; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;

    juce::Font getPopupMenuFont() override { return Fonts::regular (14.0f); }

private:
    juce::Colour accent = Palette::analog;
};
