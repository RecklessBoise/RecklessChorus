#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    const juce::Identifier kStateType   { "RecklessChorus" };
    const juce::Identifier kWidthProp   { "editorWidth" };
}

RecklessChorusProcessor::RecklessChorusProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, kStateType, Parameters::createLayout()),
      snapshot (state),
      presets (state)
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
    {
        presets.loadFactory (index);
        updateHostDisplay (ChangeDetails().withProgramChanged (true));
    }
}

const juce::String RecklessChorusProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, getNumPrograms()))
        return factoryPresets()[(std::size_t) index].name;
    return {};
}

void RecklessChorusProcessor::parameterChanged (const juce::String&, float)
{
    presets.markModified();
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
    tree.setProperty (kWidthProp, editorWidth, nullptr);
    presets.writeTo (tree);

    if (const auto xml = tree.createXml())
        copyXmlToBinary (*xml, destData);
}

void RecklessChorusProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (state.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    editorWidth = static_cast<int> (tree.getProperty (kWidthProp, kDefaultEditorWidth));

    {
        const PresetManager::ScopedApply scope (presets);
        state.replaceState (tree);
    }
    presets.readFrom (tree);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RecklessChorusProcessor();
}
