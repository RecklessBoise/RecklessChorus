#include "PluginProcessor.h"
#include "PresetManager.h"
#include "Presets.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace
{
    /** Processor plus a PresetManager pointed at a throwaway folder. */
    struct Fixture
    {
        Fixture()
            : directory (juce::File::createTempFile ("reckless-presets")),
              presets (processor.getState(), directory)
        {
            presets.setDeleteToTrash (false);
        }

        ~Fixture() { directory.deleteRecursively(); }

        float get (const char* id) { return processor.getState().getRawParameterValue (id)->load(); }

        void set (const char* id, float value)
        {
            auto* p = processor.getState().getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (value));
        }

        juce::ScopedJuceInitialiser_GUI juce;
        RecklessChorusProcessor processor;
        juce::File directory;
        PresetManager presets;
    };
}

TEST_CASE ("Saved user preset restores every parameter", "[presets]")
{
    Fixture f;
    f.set (ParamIDs::depth, 73.0f);
    f.set (ParamIDs::voices, 5.5f);
    f.set (ParamIDs::engine, 1.0f);
    f.set (ParamIDs::reactivity, 12.0f);

    REQUIRE (f.presets.save ("My Wide Pad").wasOk());
    REQUIRE (f.presets.fileForName ("My Wide Pad").existsAsFile());
    REQUIRE (f.presets.getCurrentName() == "My Wide Pad");
    REQUIRE (f.presets.isCurrentUserPreset());

    f.presets.loadFactory (0);
    REQUIRE_THAT (f.get (ParamIDs::depth), Catch::Matchers::WithinAbs (50.0, 1.0e-3));

    REQUIRE (f.presets.loadUser (f.presets.fileForName ("My Wide Pad")).wasOk());
    REQUIRE_THAT (f.get (ParamIDs::depth), Catch::Matchers::WithinAbs (73.0, 1.0e-3));
    REQUIRE_THAT (f.get (ParamIDs::voices), Catch::Matchers::WithinAbs (5.5, 1.0e-3));
    REQUIRE_THAT (f.get (ParamIDs::reactivity), Catch::Matchers::WithinAbs (12.0, 1.0e-3));
    REQUIRE (f.get (ParamIDs::engine) > 0.5f);
    REQUIRE (f.presets.getCurrentName() == "My Wide Pad");
    REQUIRE_FALSE (f.presets.isModified());
}

TEST_CASE ("Preset names are validated and sanitised", "[presets]")
{
    Fixture f;
    REQUIRE (f.presets.save ("   ").failed());
    REQUIRE (f.presets.save ("a/b:c?").wasOk());
    REQUIRE (f.presets.getUserEntries().size() == 1);
    REQUIRE_FALSE (f.presets.getUserEntries().front().name.containsAnyOf ("/:?"));

    REQUIRE (f.presets.userPresetExists ("a/b:c?"));
    REQUIRE_FALSE (f.presets.userPresetExists ("other"));
}

TEST_CASE ("Saving an existing name overwrites it", "[presets]")
{
    Fixture f;
    f.set (ParamIDs::mix, 10.0f);
    REQUIRE (f.presets.save ("Same").wasOk());
    f.set (ParamIDs::mix, 90.0f);
    REQUIRE (f.presets.save ("Same").wasOk());
    REQUIRE (f.presets.getUserEntries().size() == 1);

    f.presets.loadFactory (0);
    REQUIRE (f.presets.loadUser (f.presets.fileForName ("Same")).wasOk());
    REQUIRE_THAT (f.get (ParamIDs::mix), Catch::Matchers::WithinAbs (90.0, 1.0e-3));
}

TEST_CASE ("Entries list factory presets first, then user presets by name", "[presets]")
{
    Fixture f;
    REQUIRE (f.presets.save ("Zeta").wasOk());
    REQUIRE (f.presets.save ("alpha").wasOk());

    const auto entries = f.presets.getEntries();
    const auto numFactory = (std::size_t) f.presets.getNumFactoryPresets();
    REQUIRE (entries.size() == numFactory + 2);
    REQUIRE (entries.front().isFactory());
    REQUIRE (entries[numFactory].name == "alpha");
    REQUIRE (entries[numFactory + 1].name == "Zeta");
}

TEST_CASE ("Stepping walks from factory into user presets and wraps", "[presets]")
{
    Fixture f;
    REQUIRE (f.presets.save ("User One").wasOk());

    f.presets.loadFactory (f.presets.getNumFactoryPresets() - 1);
    REQUIRE (f.presets.step (1).wasOk());
    REQUIRE (f.presets.getCurrentName() == "User One");

    REQUIRE (f.presets.step (1).wasOk());
    REQUIRE (f.presets.getCurrentFactoryIndex() == 0);

    REQUIRE (f.presets.step (-1).wasOk());
    REQUIRE (f.presets.getCurrentName() == "User One");
}

TEST_CASE ("Loading a corrupt file fails and leaves the state alone", "[presets]")
{
    Fixture f;
    f.directory.createDirectory();
    const auto bad = f.directory.getChildFile (juce::String ("Broken") + PresetManager::kFileExtension);
    REQUIRE (bad.replaceWithText ("this is not xml"));

    f.set (ParamIDs::depth, 33.0f);
    REQUIRE (f.presets.loadUser (bad).failed());
    REQUIRE_THAT (f.get (ParamIDs::depth), Catch::Matchers::WithinAbs (33.0, 1.0e-3));
}

TEST_CASE ("Deleting the current user preset removes it and falls back to Default", "[presets]")
{
    Fixture f;
    REQUIRE (f.presets.save ("Temp").wasOk());
    REQUIRE (f.presets.deleteCurrentUserPreset().wasOk());
    REQUIRE_FALSE (f.presets.fileForName ("Temp").existsAsFile());
    REQUIRE (f.presets.getCurrentFactoryIndex() == 0);

    REQUIRE (f.presets.deleteCurrentUserPreset().failed()); // factory presets are protected
}

TEST_CASE ("Editing a parameter flags the preset as modified", "[presets]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    RecklessChorusProcessor processor;
    auto& presets = processor.getPresets();

    processor.setCurrentProgram (3);
    REQUIRE_FALSE (presets.isModified());

    auto* depth = processor.getState().getParameter (ParamIDs::depth);
    depth->setValueNotifyingHost (0.9f);
    REQUIRE (presets.isModified());
}

TEST_CASE ("Session state keeps the user preset name", "[presets]")
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::MemoryBlock block;
    {
        RecklessChorusProcessor processor;
        // Simulate a session where a user preset was active and then tweaked.
        auto state = processor.getState().copyState();
        state.setProperty ("presetName", "Live Set Chorus", nullptr);
        state.setProperty ("preset", -1, nullptr);
        state.setProperty ("presetModified", true, nullptr);
        if (auto xml = state.createXml())
            juce::AudioProcessor::copyXmlToBinary (*xml, block);
    }

    RecklessChorusProcessor restored;
    restored.setStateInformation (block.getData(), (int) block.getSize());
    REQUIRE (restored.getPresets().getCurrentName() == "Live Set Chorus");
    REQUIRE (restored.getPresets().isCurrentUserPreset());
    REQUIRE (restored.getPresets().isModified());

    juce::MemoryBlock again;
    restored.getStateInformation (again);
    RecklessChorusProcessor roundTrip;
    roundTrip.setStateInformation (again.getData(), (int) again.getSize());
    REQUIRE (roundTrip.getPresets().getCurrentName() == "Live Set Chorus");
}
