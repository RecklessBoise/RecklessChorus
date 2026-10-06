#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <map>
#include <vector>

/**
    Factory presets (compiled in) plus user presets stored as .rcpreset XML files.

    All methods except markModified() and isModified() must be called on the
    message thread. The current preset name is guarded so the host may query
    the session state from another thread.
*/
class PresetManager
{
public:
    struct Entry
    {
        juce::String name;
        int factoryIndex = -1;  // >= 0 for factory presets
        juce::File file;        // set for user presets

        bool isFactory() const noexcept { return factoryIndex >= 0; }
    };

    static constexpr const char* kFileExtension = ".rcpreset";
    static constexpr int kMaxNameLength = 64;

    explicit PresetManager (juce::AudioProcessorValueTreeState& state,
                            juce::File userDirectory = defaultUserDirectory());

    /** ~/Library/Application Support/RecklessBoise/RecklessChorus/Presets (or %APPDATA% on Windows). */
    static juce::File defaultUserDirectory();
    juce::File getUserDirectory() const { return userDirectory; }

    /** Factory presets first, then user presets sorted by name. */
    std::vector<Entry> getEntries() const;
    std::vector<Entry> getUserEntries() const;
    int getNumFactoryPresets() const;

    void loadFactory (int index);
    juce::Result loadUser (const juce::File& file);
    juce::Result load (const Entry& entry);

    /** Moves through all presets (factory then user), wrapping around. */
    juce::Result step (int delta);

    /** Writes the current settings as a user preset and makes it current. Overwrites an existing file. */
    juce::Result save (const juce::String& name);
    bool userPresetExists (const juce::String& name) const;
    juce::File fileForName (const juce::String& name) const;

    /** Moves the current user preset to the trash and falls back to "Default". */
    juce::Result deleteCurrentUserPreset();

    /** Tests turn this off so deleted presets don't pile up in the system trash. */
    void setDeleteToTrash (bool shouldUseTrash) noexcept { deleteToTrash = shouldUseTrash; }

    juce::String getCurrentName() const;
    bool isCurrentUserPreset() const;
    int getCurrentFactoryIndex() const noexcept { return currentFactoryIndex; }

    bool isModified() const noexcept { return modified.load(); }
    /** Parameter-listener hook; ignored while a preset is being applied. Thread-safe. */
    void markModified() noexcept;

    /** Session state persistence. */
    void writeTo (juce::ValueTree& tree) const;
    void readFrom (const juce::ValueTree& tree);

    /** Guards a bulk state change (session restore, A/B) so it doesn't flag the preset as modified. */
    struct ScopedApply
    {
        explicit ScopedApply (PresetManager& m) : manager (m) { manager.applying = true; }
        ~ScopedApply() { manager.applying = false; }
        PresetManager& manager;
    };

private:
    void applyValues (const std::map<juce::String, float>& naturalValues);
    void setCurrent (const juce::String& name, int factoryIndex, const juce::File& file);
    static juce::String sanitise (const juce::String& name);

    juce::AudioProcessorValueTreeState& state;
    juce::File userDirectory;

    mutable juce::SpinLock nameLock;
    juce::String currentName { "Default" };
    juce::File currentFile;
    int currentFactoryIndex = 0;

    std::atomic<bool> modified { false }, applying { false };
    bool deleteToTrash = true;
};
