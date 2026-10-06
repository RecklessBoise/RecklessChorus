#include "Presets.h"
#include "Parameters.h"

using namespace ParamIDs;

const std::vector<FactoryPreset>& factoryPresets()
{
    // engine: 0 = Analog, 1 = Digital — shape: 0 = Sine, 1 = Triangle
    static const std::vector<FactoryPreset> presets {
        { "Default", {} },
        { "Juno Mode I",     { { voices, 1.0f }, { speed, 0.5f }, { depth, 60.0f }, { quality, 88.0f },
                               { noise, 15.0f }, { variance, 10.0f } } },
        { "Juno Mode II",    { { voices, 1.0f }, { speed, 0.83f }, { depth, 80.0f }, { quality, 88.0f },
                               { noise, 15.0f }, { variance, 10.0f } } },
        { "Dimension Wash",  { { voices, 4.0f }, { speed, 0.25f }, { depth, 30.0f }, { mix, 55.0f },
                               { emphasis, 35.0f }, { variance, 20.0f } } },
        { "Thick Ensemble",  { { voices, 8.0f }, { speed, 0.3f }, { depth, 70.0f }, { mix, 60.0f },
                               { variance, 35.0f } } },
        { "Vocal Shimmer",   { { engine, 1.0f }, { voices, 3.0f }, { speed, 0.8f }, { depth, 35.0f },
                               { edge, 62.0f }, { mix, 35.0f }, { lowCut, 180.0f } } },
        { "Lo-Fi Crunch",    { { engine, 1.0f }, { voices, 2.0f }, { speed, 1.2f }, { depth, 50.0f },
                               { quality, 35.0f }, { mix, 60.0f } } },
        { "Vintage Pedal",   { { voices, 1.0f }, { shape, 1.0f }, { speed, 1.6f }, { depth, 70.0f },
                               { wide, 0.0f }, { noise, 25.0f }, { variance, 30.0f }, { rejection, 30.0f },
                               { quality, 70.0f } } },
        { "Rotary-ish",      { { engine, 1.0f }, { voices, 2.0f }, { speed, 6.0f }, { depth, 25.0f },
                               { mix, 50.0f } } },
        { "Broken BBD",      { { voices, 3.0f }, { quality, 20.0f }, { rejection, 10.0f }, { emphasis, 80.0f },
                               { noise, 40.0f }, { variance, 60.0f }, { ratio, 3.0f }, { reactivity, 2.0f } } },
        { "Slow Pad Drift",  { { voices, 6.0f }, { speed, 0.08f }, { depth, 90.0f }, { variance, 50.0f },
                               { mix, 65.0f }, { highCut, 9000.0f } } },
    };
    return presets;
}
