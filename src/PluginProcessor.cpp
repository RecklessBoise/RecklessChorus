#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    const juce::Identifier kStateType   { "RecklessChorus" };
    const juce::Identifier kPresetProp  { "preset" };
    const juce::Identifier kWidthProp   { "editorWidth" };
    const juce::Identifier kModifiedProp { "presetModified" };
}

RecklessChorusProcessor::RecklessChorusProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, kStateType, Parameters::createLayout()),
      snapshot (state)
{
    for (const auto& id : ParamIDs::all())
        state.addParameterListener (id, this);
}

RecklessChorusProcessor::~RecklessChorusProcessor()
{
    for (const auto& id : ParamIDs::all())
        state.removeParameterListener (id, this);
}

//==============================================================================
void RecklessChorusProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    core.setParams (snapshot.read());
    core.prepare (sampleRate, samplesPerBlock);
}

void RecklessChorusProcessor::reset()
{
    core.reset();
}

bool RecklessChorusProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    const auto monoOrStereo = [] (const juce::AudioChannelSet& s)
    {
        return s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo();
    };

    return monoOrStereo (in) && monoOrStereo (out) && in.size() <= out.size();
}

void RecklessChorusProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numIn = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    for (auto ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numIn == 1 && numOut > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    core.setParams (snapshot.read());
    core.process (buffer.getWritePointer (0),
                  numOut > 1 ? buffer.getWritePointer (1) : nullptr,
                  numSamples);
}

//==============================================================================
juce::AudioProcessorEditor* RecklessChorusProcessor::createEditor()
{
    return new RecklessChorusEditor (*this);
}

//==============================================================================
int RecklessChorusProcessor::getNumPrograms()
{
    return static_cast<int> (factoryPresets().size());
}

void RecklessChorusProcessor::setCurrentProgram (int index)
{
    if (juce::isPositiveAndBelow (index, getNumPrograms()))
        applyPreset (index);
}

const juce::String RecklessChorusProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, getNumPrograms()))
        return factoryPresets()[(std::size_t) index].name;
    return {};
}

void RecklessChorusProcessor::applyPreset (int index)
{
    const auto& preset = factoryPresets()[(std::size_t) index];

    applyingPreset = true;
    for (const auto& id : ParamIDs::all())
    {
        auto* param = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id));
        jassert (param != nullptr);

        auto normalised = param->getDefaultValue();
        for (const auto& [presetId, value] : preset.values)
            if (id == presetId)
                normalised = param->convertTo0to1 (value);

        param->beginChangeGesture();
        param->setValueNotifyingHost (normalised);
        param->endChangeGesture();
    }
    applyingPreset = false;

    currentPreset = index;
    presetModified = false;
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void RecklessChorusProcessor::parameterChanged (const juce::String&, float)
{
    if (! applyingPreset)
        presetModified = true;
}

void RecklessChorusProcessor::toggleAB()
{
    abSlots[(std::size_t) activeSlot] = state.copyState();
    activeSlot ^= 1;

    if (const auto& other = abSlots[(std::size_t) activeSlot]; other.isValid())
        state.replaceState (other.createCopy());
}

//==============================================================================
void RecklessChorusProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto tree = state.copyState();
    tree.setProperty (kPresetProp, currentPreset, nullptr);
    tree.setProperty (kWidthProp, editorWidth, nullptr);
    tree.setProperty (kModifiedProp, presetModified.load(), nullptr);

    if (const auto xml = tree.createXml())
        copyXmlToBinary (*xml, destData);
}

void RecklessChorusProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (state.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    currentPreset = juce::jlimit (0, getNumPrograms() - 1, static_cast<int> (tree.getProperty (kPresetProp, 0)));
    editorWidth = static_cast<int> (tree.getProperty (kWidthProp, kDefaultEditorWidth));

    applyingPreset = true;
    state.replaceState (tree);
    applyingPreset = false;
    presetModified = static_cast<bool> (tree.getProperty (kModifiedProp, false));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RecklessChorusProcessor();
}
