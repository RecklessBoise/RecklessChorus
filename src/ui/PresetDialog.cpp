#include "PresetDialog.h"
#include "RecklessLookAndFeel.h"

PresetDialog::PresetDialog()
{
    setVisible (false);
    setWantsKeyboardFocus (true);

    nameEditor.setFont (Fonts::regular (15.0f));
    nameEditor.setJustification (juce::Justification::centredLeft);
    nameEditor.setIndents (10, 0);
    nameEditor.setInputRestrictions (64);
    nameEditor.setSelectAllWhenFocused (true);
    nameEditor.setColour (juce::TextEditor::backgroundColourId, Palette::header);
    nameEditor.setColour (juce::TextEditor::textColourId, Palette::text);
    nameEditor.setColour (juce::TextEditor::outlineColourId, Palette::panelLine);
    nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, Palette::textDim);
    nameEditor.setColour (juce::TextEditor::highlightColourId, Palette::graph.withAlpha (0.4f));
    nameEditor.setColour (juce::CaretComponent::caretColourId, Palette::text);
    nameEditor.onReturnKey = [this] { confirm(); };
    nameEditor.onEscapeKey = [this] { dismiss(); };
    nameEditor.onTextChange = [this] { error = {}; updateSaveState(); };
    addChildComponent (nameEditor);

    cancelButton.boxed = true;
    okButton.boxed = true;
    okButton.setSelected (true);
    cancelButton.onClick = [this] { dismiss(); };
    okButton.onClick = [this] { confirm(); };
    addAndMakeVisible (cancelButton);
    addAndMakeVisible (okButton);
}

void PresetDialog::showSave (const juce::String& suggestedName,
                             std::function<bool (const juce::String&)> exists,
                             std::function<juce::String (const juce::String&)> onSave)
{
    saveMode = true;
    title = "SAVE PRESET";
    message = {};
    error = {};
    nameExists = std::move (exists);
    saveCallback = std::move (onSave);
    confirmCallback = nullptr;

    nameEditor.setText (suggestedName, false);
    nameEditor.setVisible (true);
    updateSaveState();

    setVisible (true);
    toFront (true);
    resized();
    nameEditor.grabKeyboardFocus();
    nameEditor.selectAll();
}

void PresetDialog::showConfirm (const juce::String& t, const juce::String& m,
                                const juce::String& confirmLabel, std::function<void()> onConfirm)
{
    saveMode = false;
    title = t.toUpperCase();
    message = m;
    error = {};
    confirmCallback = std::move (onConfirm);
    saveCallback = nullptr;

    nameEditor.setVisible (false);
    okButton.setText (confirmLabel.toUpperCase());

    setVisible (true);
    toFront (true);
    resized();
    grabKeyboardFocus();
}

void PresetDialog::dismiss()
{
    setVisible (false);
    confirmCallback = nullptr;
    saveCallback = nullptr;
    nameExists = nullptr;
}

void PresetDialog::updateSaveState()
{
    if (! saveMode)
        return;

    const auto name = nameEditor.getText().trim();
    const auto overwrite = name.isNotEmpty() && nameExists != nullptr && nameExists (name);
    okButton.setText (overwrite ? "OVERWRITE" : "SAVE");
    message = overwrite ? "A user preset with this name already exists." : juce::String();
    repaint();
}

void PresetDialog::confirm()
{
    if (saveMode)
    {
        if (saveCallback == nullptr)
            return;

        if (const auto result = saveCallback (nameEditor.getText()); result.isNotEmpty())
        {
            error = result;
            repaint();
            return;
        }
        dismiss();
        return;
    }

    const auto callback = confirmCallback;
    dismiss();
    if (callback != nullptr)
        callback();
}

//==============================================================================
juce::Rectangle<int> PresetDialog::panelBounds() const
{
    return getLocalBounds().withSizeKeepingCentre (360, saveMode ? 184 : 160);
}

void PresetDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.6f));

    const auto panel = panelBounds().toFloat();
    g.setColour (Palette::background);
    g.fillRoundedRectangle (panel, 6.0f);
    g.setColour (Palette::panelLine);
    g.drawRoundedRectangle (panel.reduced (0.5f), 6.0f, 1.0f);

    auto content = panel.reduced (22.0f, 18.0f);
    g.setColour (Palette::text);
    g.setFont (Fonts::bold (14.0f));
    g.drawText (title, content.removeFromTop (20.0f), juce::Justification::centredLeft);

    if (saveMode)
        content.removeFromTop (48.0f); // name editor

    const auto& line = error.isNotEmpty() ? error : message;
    if (line.isNotEmpty())
    {
        g.setColour (error.isNotEmpty() ? juce::Colour (0xffff5a5a) : Palette::textDim);
        g.setFont (Fonts::regular (12.5f));
        g.drawFittedText (line, content.removeFromTop (40.0f).toNearestInt(), juce::Justification::centredLeft, 2);
    }
}

void PresetDialog::resized()
{
    auto content = panelBounds().reduced (22, 18);
    content.removeFromTop (28);
    nameEditor.setBounds (content.removeFromTop (32));

    auto buttons = content.removeFromBottom (28);
    okButton.setBounds (buttons.removeFromRight (100));
    buttons.removeFromRight (10);
    cancelButton.setBounds (buttons.removeFromRight (90));
}

bool PresetDialog::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        dismiss();
        return true;
    }
    if (key == juce::KeyPress::returnKey)
    {
        confirm();
        return true;
    }
    return true; // modal: swallow everything else
}

void PresetDialog::mouseUp (const juce::MouseEvent& e)
{
    // Clicking the dimmed backdrop cancels.
    if (! panelBounds().contains (e.getPosition()) && e.eventComponent == this)
        dismiss();
}
