#include "Widgets.h"
#include "RecklessLookAndFeel.h"

namespace
{
    juce::RangedAudioParameter& lookup (juce::AudioProcessorValueTreeState& state, const juce::String& id)
    {
        auto* p = state.getParameter (id);
        jassert (p != nullptr);
        return *p;
    }

    juce::Colour accentOf (const juce::Component& c)
    {
        if (auto* lnf = dynamic_cast<RecklessLookAndFeel*> (&c.getLookAndFeel()))
            return lnf->getAccent();
        return Palette::analog;
    }
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& label, bool accent)
    : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox),
      attachment (state, paramId, *this)
{
    setComponentID (paramId);
    getProperties().set ("label", label);
    getProperties().set ("accent", accent);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setMouseDragSensitivity (220);
    setVelocityBasedMode (false);

    auto& param = lookup (state, paramId);
    setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));
}

//==============================================================================
Fader::Fader (juce::AudioProcessorValueTreeState& state, const juce::String& paramId)
    : juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox),
      attachment (state, paramId, *this)
{
    setComponentID (paramId);
    auto& param = lookup (state, paramId);
    setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));
    setPopupDisplayEnabled (true, true, nullptr);
}

//==============================================================================
ValueField::ValueField (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text)
    : param (lookup (state, paramId)),
      attachment (param, [this] (float) { repaint(); }, nullptr),
      label (text)
{
    setComponentID (paramId);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    attachment.sendInitialUpdate();
}

void ValueField::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto hot = isMouseOverOrDragging();

    g.setFont (Fonts::bold (11.0f));
    g.setColour (hot ? Palette::text : Palette::textDim);
    g.drawText (label.toUpperCase(), b, juce::Justification::centredLeft);

    g.setColour (Palette::text);
    g.drawText (param.getCurrentValueAsText(), b, juce::Justification::centredRight);
}

void ValueField::mouseDown (const juce::MouseEvent&)
{
    dragStartValue = param.getValue();
    attachment.beginGesture();
}

void ValueField::mouseDrag (const juce::MouseEvent& e)
{
    const auto sensitivity = e.mods.isShiftDown() ? 1000.0f : 200.0f;
    const auto normalised = juce::jlimit (0.0f, 1.0f, dragStartValue - static_cast<float> (e.getDistanceFromDragStartY()) / sensitivity);
    attachment.setValueAsPartOfGesture (param.convertFrom0to1 (normalised));
}

void ValueField::mouseUp (const juce::MouseEvent&)
{
    attachment.endGesture();
}

void ValueField::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

//==============================================================================
EngineSwitch::EngineSwitch (juce::AudioProcessorValueTreeState& state)
    : param (lookup (state, "engine")),
      attachment (param, [this] (float v) { digital = v > 0.5f; repaint(); }, nullptr)
{
    setComponentID ("engine");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

juce::Rectangle<float> EngineSwitch::half (bool right) const
{
    auto b = getLocalBounds().toFloat();
    return right ? b.removeFromRight (b.getWidth() * 0.5f) : b.removeFromLeft (b.getWidth() * 0.5f);
}

void EngineSwitch::paint (juce::Graphics& g)
{
    const auto mouse = getMouseXYRelative().toFloat();
    const auto hover = isMouseOver();

    for (const auto right : { false, true })
    {
        const auto area = half (right).reduced (6.0f, 0.0f);
        const auto active = right == digital;
        const auto colour = active ? Palette::accentFor (digital)
                                   : (hover && area.contains (mouse) ? Palette::text : Palette::textDim);

        const auto iconSize = 18.0f;
        const auto textWidth = 64.0f;
        const auto totalWidth = iconSize + 8.0f + textWidth;
        auto row = area.withSizeKeepingCentre (totalWidth, area.getHeight());
        // Analog: icon then text; Digital: text then icon (mirrors the reference layout)
        auto icon = right ? row.removeFromRight (iconSize) : row.removeFromLeft (iconSize);
        icon = icon.withSizeKeepingCentre (iconSize, iconSize);
        (right ? row.removeFromRight (8.0f) : row.removeFromLeft (8.0f));

        g.setColour (colour);
        if (! right)
        {
            // Chip icon
            const auto body = icon.reduced (4.0f);
            g.drawRect (body, 1.5f);
            g.drawRect (body.reduced (3.0f), 1.0f);
            for (int i = 0; i < 3; ++i)
            {
                const auto t = body.getX() + body.getWidth() * (0.2f + 0.3f * (float) i);
                const auto s = body.getY() + body.getHeight() * (0.2f + 0.3f * (float) i);
                g.drawLine (t, icon.getY(), t, body.getY(), 1.2f);
                g.drawLine (t, body.getBottom(), t, icon.getBottom(), 1.2f);
                g.drawLine (icon.getX(), s, body.getX(), s, 1.2f);
                g.drawLine (body.getRight(), s, icon.getRight(), s, 1.2f);
            }
        }
        else
        {
            // Dot-matrix icon
            for (int ix = 0; ix < 5; ++ix)
                for (int iy = 0; iy < 5; ++iy)
                {
                    const auto px = icon.getX() + 2.0f + (float) ix * 3.5f;
                    const auto py = icon.getY() + 2.0f + (float) iy * 3.5f;
                    const auto d = std::abs (ix - 2) + std::abs (iy - 2);
                    g.setColour (colour.withAlpha (d > 3 ? 0.35f : 1.0f));
                    g.fillEllipse (px - 1.0f, py - 1.0f, 2.0f, 2.0f);
                }
            g.setColour (colour);
        }

        g.setFont (Fonts::bold (13.0f));
        g.drawText (right ? "DIGITAL" : "ANALOG", row, right ? juce::Justification::centredRight
                                                            : juce::Justification::centredLeft);
    }
}

void EngineSwitch::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()))
        return;

    const auto wantDigital = half (true).contains (e.position);
    attachment.setValueAsCompleteGesture (wantDigital ? 1.0f : 0.0f);
}

//==============================================================================
IconToggle::IconToggle (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, Kind k)
    : param (lookup (state, paramId)),
      attachment (param, [this] (float v) { on = v > 0.5f; repaint(); }, nullptr),
      kind (k)
{
    setComponentID (paramId);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

void IconToggle::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const auto hover = isMouseOver();
    auto icon = b.removeFromLeft (22.0f).withSizeKeepingCentre (20.0f, 12.0f);
    b.removeFromLeft (6.0f);

    const auto lit = kind == Kind::Shape ? true : on;
    const auto colour = lit ? Palette::text : (hover ? Palette::text.withAlpha (0.8f) : Palette::textDim);
    g.setColour (colour);

    juce::Path p;
    if (kind == Kind::Shape)
    {
        if (on) // triangle
        {
            p.startNewSubPath (icon.getX(), icon.getCentreY());
            p.lineTo (icon.getX() + icon.getWidth() * 0.25f, icon.getY());
            p.lineTo (icon.getX() + icon.getWidth() * 0.75f, icon.getBottom());
            p.lineTo (icon.getRight(), icon.getCentreY());
        }
        else // sine
        {
            for (int i = 0; i <= 24; ++i)
            {
                const auto t = (float) i / 24.0f;
                const auto pt = juce::Point<float> (icon.getX() + t * icon.getWidth(),
                                                    icon.getCentreY() - std::sin (t * juce::MathConstants<float>::twoPi) * icon.getHeight() * 0.5f);
                if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
        }
    }
    else
    {
        // Diverging arrows
        const auto cy = icon.getCentreY();
        p.startNewSubPath (icon.getX(), cy);
        p.lineTo (icon.getRight(), cy);
        for (const auto x : { icon.getX(), icon.getRight() })
        {
            const auto dir = x < icon.getCentreX() ? 1.0f : -1.0f;
            p.startNewSubPath (x + dir * 5.0f, cy - 4.5f);
            p.lineTo (x, cy);
            p.lineTo (x + dir * 5.0f, cy + 4.5f);
        }
        p.startNewSubPath (icon.getCentreX(), icon.getY());
        p.lineTo (icon.getCentreX(), icon.getBottom());
    }
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (Fonts::bold (12.5f));
    g.drawText (kind == Kind::Shape ? "SHAPE" : "WIDE", b, juce::Justification::centredLeft);
}

void IconToggle::mouseUp (const juce::MouseEvent& e)
{
    if (getLocalBounds().contains (e.getPosition()))
        attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

//==============================================================================
FlatButton::FlatButton (juce::String t, float height) : text (std::move (t)), fontHeight (height)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void FlatButton::setSelected (bool s)
{
    if (selected != s)
    {
        selected = s;
        repaint();
    }
}

void FlatButton::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto colour = selected ? Palette::text : (isMouseOver() ? Palette::text.withAlpha (0.85f) : Palette::textDim);

    if (boxed)
    {
        g.setColour (selected ? accentOf (*this) : Palette::panelLine);
        g.drawRoundedRectangle (b.reduced (1.0f), 3.0f, 1.2f);
    }

    g.setColour (boxed && selected ? accentOf (*this) : colour);
    g.setFont (Fonts::bold (fontHeight));
    g.drawText (text, b, boxed ? juce::Justification::centred : juce::Justification::centredLeft);
}

void FlatButton::mouseUp (const juce::MouseEvent& e)
{
    if (getLocalBounds().contains (e.getPosition()) && onClick != nullptr)
        onClick();
}

//==============================================================================
void ArrowButton::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().withSizeKeepingCentre (7.0f, 9.0f);
    juce::Path p;
    if (right)
        p.addTriangle (b.getX(), b.getY(), b.getRight(), b.getCentreY(), b.getX(), b.getBottom());
    else
        p.addTriangle (b.getRight(), b.getY(), b.getX(), b.getCentreY(), b.getRight(), b.getBottom());

    g.setColour (isMouseOver() ? Palette::text : Palette::textDim);
    g.fillPath (p);
}

//==============================================================================
void SaveButton::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().withSizeKeepingCentre (14.0f, 14.0f);
    g.setColour (isMouseOver() ? accentOf (*this) : Palette::textDim);

    juce::Path tray;
    tray.startNewSubPath (b.getX(), b.getY() + b.getHeight() * 0.6f);
    tray.lineTo (b.getX(), b.getBottom());
    tray.lineTo (b.getRight(), b.getBottom());
    tray.lineTo (b.getRight(), b.getY() + b.getHeight() * 0.6f);

    juce::Path arrow;
    arrow.startNewSubPath (b.getCentreX(), b.getY());
    arrow.lineTo (b.getCentreX(), b.getY() + b.getHeight() * 0.7f);
    arrow.startNewSubPath (b.getCentreX() - 4.0f, b.getY() + b.getHeight() * 0.7f - 4.0f);
    arrow.lineTo (b.getCentreX(), b.getY() + b.getHeight() * 0.7f);
    arrow.lineTo (b.getCentreX() + 4.0f, b.getY() + b.getHeight() * 0.7f - 4.0f);

    const juce::PathStrokeType stroke (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.strokePath (tray, stroke);
    g.strokePath (arrow, stroke);
}
