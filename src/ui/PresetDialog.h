#pragma once

#include "Widgets.h"

/**
    In-editor modal overlay (scales with the UI, no separate OS window):
    either a "save preset" name prompt or a yes/no confirmation.
*/
class PresetDialog final : public juce::Component
{
public:
    PresetDialog();

    /** `exists` lets the dialog warn before overwriting; `onSave` returns an error message, or empty on success. */
    void showSave (const juce::String& suggestedName,
                   std::function<bool (const juce::String&)> exists,
                   std::function<juce::String (const juce::String&)> onSave);

    void showConfirm (const juce::String& title, const juce::String& message,
                      const juce::String& confirmLabel, std::function<void()> onConfirm);

    void dismiss();

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void confirm();
    void updateSaveState();
    juce::Rectangle<int> panelBounds() const;

    juce::TextEditor nameEditor;
    FlatButton cancelButton { "CANCEL", 12.0f }, okButton { "SAVE", 12.0f };

    bool saveMode = true;
    juce::String title, message, error;
    std::function<bool (const juce::String&)> nameExists;
    std::function<juce::String (const juce::String&)> saveCallback;
    std::function<void()> confirmCallback;
};
