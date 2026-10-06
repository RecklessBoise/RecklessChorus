#pragma once

#include <juce_core/juce_core.h>

#include <utility>
#include <vector>

/** Factory preset: parameter values in natural units. Parameters left out use their default. */
struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;
};

const std::vector<FactoryPreset>& factoryPresets();
