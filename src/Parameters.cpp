#include "Parameters.h"

#include <map>

namespace ParamIDs
{
    const juce::StringArray& all()
    {
        static const juce::StringArray ids { engine, speed, depth, voices, quality, edge, shape, wide,
                                             mix, output, lowCut, highCut, rejection, emphasis, noise,
                                             variance, reactivity, ratio };
        return ids;
    }
}

namespace Parameters
{
namespace
{
    using Attributes = juce::AudioParameterFloatAttributes;
    constexpr int kVersion = 1;

    juce::NormalisableRange<float> skewed (float lo, float hi, float centre, float interval = 0.0f)
    {
        juce::NormalisableRange<float> r (lo, hi, interval);
        r.setSkewForCentre (centre);
        return r;
    }

    Attributes percent()
    {
        return Attributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + "%"; })
            .withValueFromStringFunction ([] (const juce::String& s) { return s.retainCharacters ("-0123456789.").getFloatValue(); });
    }

    Attributes hertz()
    {
        return Attributes()
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float v, int)
            {
                if (v >= 1000.0f)
                    return juce::String (v / 1000.0f, 2) + " kHz";
                return juce::String (v, v < 10.0f ? 2 : 0) + " Hz";
            })
            .withValueFromStringFunction ([] (const juce::String& s)
            {
                const auto v = s.retainCharacters ("0123456789.").getFloatValue();
                return s.containsIgnoreCase ("k") ? v * 1000.0f : v;
            });
    }

    std::unique_ptr<juce::AudioParameterFloat> floatParam (const char* id, const juce::String& name,
                                                           juce::NormalisableRange<float> range,
                                                           float defaultValue, Attributes attributes)
    {
        return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, kVersion }, name,
                                                            range, defaultValue, attributes);
    }

    std::unique_ptr<juce::AudioParameterFloat> percentParam (const char* id, const juce::String& name, float defaultValue)
    {
        return floatParam (id, name, { 0.0f, 100.0f, 0.0f }, defaultValue, percent());
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::engine, kVersion },
                                                              "Engine", juce::StringArray { "Analog", "Digital" }, 0));

    layout.add (floatParam (ParamIDs::speed, "Speed", skewed (0.02f, 10.0f, 1.0f), 0.5f, hertz()));
    layout.add (percentParam (ParamIDs::depth, "Depth", 50.0f));

    layout.add (floatParam (ParamIDs::voices, "Voices", { 1.0f, 8.0f, 0.0f }, 2.0f,
                            Attributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); })));

    layout.add (floatParam (ParamIDs::quality, "Quality", { 0.0f, 100.0f, 0.0f }, 100.0f,
                            Attributes()
                                .withLabel ("%")
                                .withStringFromValueFunction ([] (float v, int)
                                {
                                    return juce::String (reckless::clockHzFromQuality (v / 100.0f) / 1000.0, 1) + " kHz";
                                })));

    layout.add (percentParam (ParamIDs::edge, "Edge", 50.0f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::shape, kVersion },
                                                              "Shape", juce::StringArray { "Sine", "Triangle" }, 0));
    // A two-state choice rather than AudioParameterBool: it restores exactly from any normalised value.
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::wide, kVersion },
                                                              "Wide", juce::StringArray { "Off", "On" }, 1));

    layout.add (percentParam (ParamIDs::mix, "Mix", 50.0f));
    layout.add (floatParam (ParamIDs::output, "Output", { -24.0f, 12.0f, 0.0f }, 0.0f,
                            Attributes()
                                .withLabel ("dB")
                                .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })));

    layout.add (floatParam (ParamIDs::lowCut, "Low Cut", skewed (20.0f, 1000.0f, 150.0f), 20.0f, hertz()));
    layout.add (floatParam (ParamIDs::highCut, "High Cut", skewed (1000.0f, 20000.0f, 6000.0f), 20000.0f, hertz()));

    layout.add (percentParam (ParamIDs::rejection, "Rejection", 50.0f));
    layout.add (percentParam (ParamIDs::emphasis, "Emphasis", 50.0f));
    layout.add (percentParam (ParamIDs::noise, "Noise", 0.0f));
    layout.add (percentParam (ParamIDs::variance, "Variance", 0.0f));

    layout.add (floatParam (ParamIDs::reactivity, "Reactivity", skewed (0.5f, 50.0f, 5.0f), 5.0f,
                            Attributes()
                                .withLabel ("ms")
                                .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " ms"; })));
    layout.add (floatParam (ParamIDs::ratio, "Ratio", { 1.0f, 4.0f, 0.0f }, 2.0f,
                            Attributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2); })));

    return layout;
}

Info infoFor (const juce::String& id)
{
    static const std::map<juce::String, Info> infos {
        { ParamIDs::engine,     { "Engine",     "Analog bucket-brigade model or clean digital delay line" } },
        { ParamIDs::speed,      { "Speed",      "Rate of the voice modulators" } },
        { ParamIDs::depth,      { "Depth",      "Amount of delay-time modulation" } },
        { ParamIDs::voices,     { "Voices",     "Number of chorus voices, morphing continuously from 1 to 8" } },
        { ParamIDs::quality,    { "Quality",    "Clock of the BBD / sample-rate reducer, from 1 to 48 kHz" } },
        { ParamIDs::edge,       { "Edge",       "Brightness of the wet signal" } },
        { ParamIDs::shape,      { "Shape",      "Modulator waveform: sine or triangle" } },
        { ParamIDs::wide,       { "Wide",       "Spread the voices across the stereo field" } },
        { ParamIDs::mix,        { "Mix",        "Balance between dry and chorused signal" } },
        { ParamIDs::output,     { "Output",     "Output level" } },
        { ParamIDs::lowCut,     { "Low Cut",    "High-pass filter on the signal entering the chorus" } },
        { ParamIDs::highCut,    { "High Cut",   "Low-pass filter on the signal entering the chorus" } },
        { ParamIDs::rejection,  { "Rejection",  "Steepness of the anti-aliasing filters around the BBD clock" } },
        { ParamIDs::emphasis,   { "Emphasis",   "Pre/de-emphasis and resonance of the BBD filters" } },
        { ParamIDs::noise,      { "Noise",      "Hiss generated inside the bucket-brigade line" } },
        { ParamIDs::variance,   { "Variance",   "Per-voice component tolerance and slow drift" } },
        { ParamIDs::reactivity, { "Reactivity", "Attack time of the compander detectors" } },
        { ParamIDs::ratio,      { "Ratio",      "Compander ratio: higher values pump and breathe more" } },
    };

    if (const auto it = infos.find (id); it != infos.end())
        return it->second;
    return {};
}

//==============================================================================
Snapshot::Snapshot (juce::AudioProcessorValueTreeState& s)
    : engine (get (s, ParamIDs::engine)), speed (get (s, ParamIDs::speed)), depth (get (s, ParamIDs::depth)),
      voices (get (s, ParamIDs::voices)), quality (get (s, ParamIDs::quality)), edge (get (s, ParamIDs::edge)),
      shape (get (s, ParamIDs::shape)), wide (get (s, ParamIDs::wide)), mix (get (s, ParamIDs::mix)),
      output (get (s, ParamIDs::output)), lowCut (get (s, ParamIDs::lowCut)), highCut (get (s, ParamIDs::highCut)),
      rejection (get (s, ParamIDs::rejection)), emphasis (get (s, ParamIDs::emphasis)), noise (get (s, ParamIDs::noise)),
      variance (get (s, ParamIDs::variance)), reactivity (get (s, ParamIDs::reactivity)), ratio (get (s, ParamIDs::ratio))
{
}

std::atomic<float>* Snapshot::get (juce::AudioProcessorValueTreeState& s, const char* id)
{
    auto* p = s.getRawParameterValue (id);
    jassert (p != nullptr);
    return p;
}

reckless::ChorusParams Snapshot::read() const noexcept
{
    reckless::ChorusParams p;
    p.engine       = engine->load() > 0.5f ? reckless::Engine::Digital : reckless::Engine::Analog;
    p.speedHz      = speed->load();
    p.depth        = depth->load() * 0.01f;
    p.voices       = voices->load();
    p.quality      = quality->load() * 0.01f;
    p.edge         = edge->load() * 0.01f;
    p.shape        = shape->load() > 0.5f ? reckless::LfoShape::Triangle : reckless::LfoShape::Sine;
    p.wide         = wide->load() > 0.5f;
    p.mix          = mix->load() * 0.01f;
    p.outputDb     = output->load();
    p.lowCutHz     = lowCut->load();
    p.highCutHz    = highCut->load();
    p.rejection    = rejection->load() * 0.01f;
    p.emphasis     = emphasis->load() * 0.01f;
    p.noise        = noise->load() * 0.01f;
    p.variance     = variance->load() * 0.01f;
    p.reactivityMs = reactivity->load();
    p.ratio        = ratio->load();
    return p;
}

} // namespace Parameters
