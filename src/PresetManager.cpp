#include "PresetManager.h"
#include "Parameters.h"
#include "Presets.h"

namespace
{
    const juce::Identifier kPresetTag      { "RecklessChorusPreset" };
    const juce::Identifier kParamTag       { "PARAM" };
    const juce::Identifier kVersionAttr    { "version" };
    const juce::Identifier kNameAttr       { "name" };
    const juce::Identifier kIdAttr         { "id" };
    const juce::Identifier kValueAttr      { "value" };

    // Session-state properties
    const juce::Identifier kPresetNameProp     { "presetName" };
    const juce::Identifier kPresetFactoryProp  { "preset" };   // factory index, -1 for user presets
    const juce::Identifier kPresetModifiedProp { "presetModified" };

    constexpr int kFormatVersion = 1;
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s, juce::File dir)
    : state (s), userDirectory (std::move (dir))
{
}

juce::File PresetManager::defaultUserDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
#if JUCE_MAC
        .getChildFile ("Application Support")
#endif
        .getChildFile ("RecklessBoise")
        .getChildFile ("RecklessChorus")
        .getChildFile ("Presets");
}

//==============================================================================
int PresetManager::getNumFactoryPresets() const
{
    return static_cast<int> (factoryPresets().size());
}

std::vector<PresetManager::Entry> PresetManager::getUserEntries() const
{
    std::vector<Entry> entries;
    if (! userDirectory.isDirectory())
        return entries;

    for (const auto& file : userDirectory.findChildFiles (juce::File::findFiles, false, juce::String ("*") + kFileExtension))
        entries.push_back ({ file.getFileNameWithoutExtension(), -1, file });

    std::sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b)
    {
        return a.name.compareNatural (b.name) < 0;
    });
    return entries;
}

std::vector<PresetManager::Entry> PresetManager::getEntries() const
{
    std::vector<Entry> entries;
    const auto& factory = factoryPresets();
    for (int i = 0; i < (int) factory.size(); ++i)
        entries.push_back ({ factory[(std::size_t) i].name, i, {} });

    for (auto& e : getUserEntries())
        entries.push_back (std::move (e));
    return entries;
}

//==============================================================================
void PresetManager::applyValues (const std::map<juce::String, float>& naturalValues)
{
    const ScopedApply scope (*this);

    for (const auto& id : ParamIDs::all())
    {
        auto* param = state.getParameter (id);
        jassert (param != nullptr);

        auto normalised = param->getDefaultValue();
        if (const auto it = naturalValues.find (id); it != naturalValues.end())
            normalised = param->convertTo0to1 (it->second);

        param->beginChangeGesture();
        param->setValueNotifyingHost (normalised);
        param->endChangeGesture();
    }

    modified = false;
}

void PresetManager::setCurrent (const juce::String& name, int factoryIndex, const juce::File& file)
{
    const juce::SpinLock::ScopedLockType lock (nameLock);
    currentName = name;
    currentFactoryIndex = factoryIndex;
    currentFile = file;
}

void PresetManager::loadFactory (int index)
{
    const auto& factory = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) factory.size()))
        return;

    const auto& preset = factory[(std::size_t) index];
    std::map<juce::String, float> values;
    for (const auto& [id, value] : preset.values)
        values[id] = value;

    applyValues (values);
    setCurrent (preset.name, index, {});
}

juce::Result PresetManager::loadUser (const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (kPresetTag.toString()))
        return juce::Result::fail ("\"" + file.getFileName() + "\" is not a RecklessChorus preset");

    std::map<juce::String, float> values;
    for (const auto* child : xml->getChildWithTagNameIterator (kParamTag.toString()))
        if (child->hasAttribute (kIdAttr.toString()))
            values[child->getStringAttribute (kIdAttr.toString())] = (float) child->getDoubleAttribute (kValueAttr.toString());

    applyValues (values);
    setCurrent (file.getFileNameWithoutExtension(), -1, file);
    return juce::Result::ok();
}

juce::Result PresetManager::load (const Entry& entry)
{
    if (entry.isFactory())
    {
        loadFactory (entry.factoryIndex);
        return juce::Result::ok();
    }
    return loadUser (entry.file);
}

juce::Result PresetManager::step (int delta)
{
    const auto entries = getEntries();
    const auto count = (int) entries.size();
    if (count == 0)
        return juce::Result::ok();

    int index = 0;
    {
        const juce::SpinLock::ScopedLockType lock (nameLock);
        for (int i = 0; i < count; ++i)
        {
            const auto& e = entries[(std::size_t) i];
            if ((e.isFactory() && e.factoryIndex == currentFactoryIndex)
                || (! e.isFactory() && currentFactoryIndex < 0 && e.file == currentFile))
            {
                index = i;
                break;
            }
        }
    }

    return load (entries[(std::size_t) (((index + delta) % count + count) % count)]);
}

//==============================================================================
juce::String PresetManager::sanitise (const juce::String& name)
{
    return juce::File::createLegalFileName (name.trim()).substring (0, kMaxNameLength).trim();
}

juce::File PresetManager::fileForName (const juce::String& name) const
{
    return userDirectory.getChildFile (sanitise (name) + kFileExtension);
}

bool PresetManager::userPresetExists (const juce::String& name) const
{
    return sanitise (name).isNotEmpty() && fileForName (name).existsAsFile();
}

juce::Result PresetManager::save (const juce::String& rawName)
{
    const auto name = sanitise (rawName);
    if (name.isEmpty())
        return juce::Result::fail ("Please enter a preset name");

    if (const auto created = userDirectory.createDirectory(); created.failed())
        return juce::Result::fail ("Could not create the presets folder: " + created.getErrorMessage());

    juce::XmlElement xml (kPresetTag.toString());
    xml.setAttribute (kVersionAttr, kFormatVersion);
    xml.setAttribute (kNameAttr, name);

    for (const auto& id : ParamIDs::all())
    {
        auto* param = state.getParameter (id);
        auto* child = xml.createNewChildElement (kParamTag.toString());
        child->setAttribute (kIdAttr, id);
        child->setAttribute (kValueAttr, (double) param->convertFrom0to1 (param->getValue()));
    }

    const auto file = fileForName (name);
    if (! xml.writeTo (file))
        return juce::Result::fail ("Could not write " + file.getFullPathName());

    setCurrent (name, -1, file);
    modified = false;
    return juce::Result::ok();
}

juce::Result PresetManager::deleteCurrentUserPreset()
{
    juce::File file;
    {
        const juce::SpinLock::ScopedLockType lock (nameLock);
        if (currentFactoryIndex >= 0)
            return juce::Result::fail ("Factory presets can't be deleted");
        file = currentFile;
    }

    if (file.existsAsFile() && ! (deleteToTrash ? file.moveToTrash() : file.deleteFile()))
        return juce::Result::fail ("Could not delete " + file.getFileName());

    loadFactory (0);
    return juce::Result::ok();
}

//==============================================================================
juce::String PresetManager::getCurrentName() const
{
    const juce::SpinLock::ScopedLockType lock (nameLock);
    return currentName;
}

bool PresetManager::isCurrentUserPreset() const
{
    const juce::SpinLock::ScopedLockType lock (nameLock);
    return currentFactoryIndex < 0;
}

void PresetManager::markModified() noexcept
{
    if (! applying.load())
        modified = true;
}

void PresetManager::writeTo (juce::ValueTree& tree) const
{
    const juce::SpinLock::ScopedLockType lock (nameLock);
    tree.setProperty (kPresetNameProp, currentName, nullptr);
    tree.setProperty (kPresetFactoryProp, currentFactoryIndex, nullptr);
    tree.setProperty (kPresetModifiedProp, modified.load(), nullptr);
}

void PresetManager::readFrom (const juce::ValueTree& tree)
{
    const auto factoryIndex = static_cast<int> (tree.getProperty (kPresetFactoryProp, 0));
    const auto& factory = factoryPresets();

    if (juce::isPositiveAndBelow (factoryIndex, (int) factory.size()))
        setCurrent (factory[(std::size_t) factoryIndex].name, factoryIndex, {});
    else
    {
        const auto name = tree.getProperty (kPresetNameProp, "Default").toString();
        setCurrent (name, -1, fileForName (name));
    }

    modified = static_cast<bool> (tree.getProperty (kPresetModifiedProp, false));
}
