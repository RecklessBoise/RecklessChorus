#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/** Rotary knob bound to a parameter; shows its label, or its value while hovered. */
class Knob final : public juce::Slider
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& label,
          bool accent = false);

private:
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

//==============================================================================
/** Horizontal fader bound to a parameter. */
class Fader final : public juce::Slider
{
public:
    Fader (juce::AudioProcessorValueTreeState& state, const juce::String& paramId);

private:
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

//==============================================================================
/** "LABEL   value" field; drag vertically to edit, double-click to reset. */
class ValueField final : public juce::Component
{
public:
    ValueField (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& label);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::String label;
    float dragStartValue = 0.0f;
};

//==============================================================================
/** ANALOG / DIGITAL engine selector with drawn icons. */
class EngineSwitch final : public juce::Component
{
public:
    explicit EngineSwitch (juce::AudioProcessorValueTreeState& state);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    bool isDigital() const noexcept { return digital; }

private:
    juce::Rectangle<float> half (bool right) const;

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    bool digital = false;
};

//==============================================================================
/** Small icon + caption toggle (SHAPE / WIDE). */
class IconToggle final : public juce::Component
{
public:
    enum class Kind { Shape, Wide };

    IconToggle (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, Kind kind);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    Kind kind;
    bool on = false;
};

//==============================================================================
/** Flat text tab/button. */
class FlatButton final : public juce::Component
{
public:
    explicit FlatButton (juce::String text, float fontHeight = 12.5f);

    void setSelected (bool shouldBeSelected);
    bool isSelected() const noexcept { return selected; }
    void setText (const juce::String& t) { text = t; repaint(); }

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    std::function<void()> onClick;
    bool boxed = false;

private:
    juce::String text;
    float fontHeight;
    bool selected = false;
};

//==============================================================================
/** Triangle arrow button used by the preset browser. */
class ArrowButton final : public juce::Component
{
public:
    explicit ArrowButton (bool pointsRight) : right (pointsRight) {}

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (contains (e.getPosition()) && onClick != nullptr)
            onClick();
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    std::function<void()> onClick;

private:
    bool right;
};

//==============================================================================
/** "Save" icon button (arrow into a tray). */
class SaveButton final : public juce::Component
{
public:
    SaveButton() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (contains (e.getPosition()) && onClick != nullptr)
            onClick();
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    std::function<void()> onClick;
};
