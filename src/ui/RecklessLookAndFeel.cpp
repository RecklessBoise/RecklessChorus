#include "RecklessLookAndFeel.h"

namespace Fonts
{
    juce::Font bold (float height)    { return juce::Font (juce::FontOptions (height, juce::Font::bold)); }
    juce::Font regular (float height) { return juce::Font (juce::FontOptions (height, juce::Font::plain)); }
}

RecklessLookAndFeel::RecklessLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, Palette::header);
    setColour (juce::PopupMenu::textColourId, Palette::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::panelLine);
    setColour (juce::PopupMenu::highlightedTextColourId, Palette::text);
    setColour (juce::TooltipWindow::backgroundColourId, Palette::header);
    setColour (juce::TooltipWindow::textColourId, Palette::text);
}

void RecklessLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                            float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const auto radius = size * 0.5f - 2.0f;
    const auto isAccent = static_cast<bool> (slider.getProperties()["accent"]);
    const auto isSmall = size < 60.0f;

    // Drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre.translated (0.0f, radius * 0.08f)).expanded (radius * 0.06f));

    // Face
    const auto face = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (Palette::knobTop, centre.x, face.getY(),
                                             Palette::knobBottom, centre.x, face.getBottom(), false));
    g.fillEllipse (face);
    g.setColour (Palette::knobRim);
    g.drawEllipse (face.reduced (0.5f), 1.0f);

    // Value arc along the rim
    const auto arcRadius = radius - juce::jmax (2.5f, radius * 0.07f);
    const auto arcWidth = juce::jmax (2.0f, radius * 0.06f);
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (Palette::track.withAlpha (0.6f));
    g.strokePath (track, juce::PathStrokeType (arcWidth * 0.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
    g.setColour (isAccent ? accent : Palette::text);
    g.strokePath (value, juce::PathStrokeType (arcWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Label, or the value while interacting
    const auto showValue = slider.isMouseOverOrDragging() && slider.isEnabled();
    const auto label = slider.getProperties()["label"].toString();
    const auto text = showValue ? slider.getTextFromValue (slider.getValue()) : label;
    g.setColour (showValue ? (isAccent ? accent : Palette::text) : Palette::text);
    g.setFont (Fonts::bold (isSmall ? juce::jmax (9.0f, radius * 0.42f) : radius * 0.2f));
    g.drawFittedText (text, face.reduced (radius * 0.18f).toNearestInt(), juce::Justification::centred, 1);
}

void RecklessLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                            float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto trackY = bounds.getCentreY();

    g.setColour (Palette::track);
    g.fillRoundedRectangle (bounds.getX(), trackY - 1.5f, bounds.getWidth(), 3.0f, 1.5f);

    g.setColour (Palette::text.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds.getX(), trackY - 1.5f, sliderPos - bounds.getX(), 3.0f, 1.5f);

    const auto thumb = juce::Rectangle<float> (5.0f, juce::jmin (18.0f, bounds.getHeight())).withCentre ({ sliderPos, trackY });
    g.setColour (Palette::text);
    g.fillRoundedRectangle (thumb, 1.5f);
}
