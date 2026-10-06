#include "GraphPanels.h"
#include "RecklessLookAndFeel.h"

#include <utility>

namespace
{
    constexpr float kTitleHeight = 26.0f;
    constexpr int kFieldHeight = 17;

    float freqToX (double f, juce::Rectangle<float> area, double lo, double hi)
    {
        return area.getX() + area.getWidth() * (float) (std::log (f / lo) / std::log (hi / lo));
    }

    double xToFreq (float x, juce::Rectangle<float> area, double lo, double hi)
    {
        return lo * std::pow (hi / lo, (double) ((x - area.getX()) / area.getWidth()));
    }

    float dbToY (double db, juce::Rectangle<float> area, double top, double bottom)
    {
        return area.getY() + area.getHeight() * (float) ((top - db) / (top - bottom));
    }

    /** Strokes `curve` in the graph colour and fills the area underneath it. */
    void drawCurve (juce::Graphics& g, const juce::Path& curve, juce::Rectangle<float> area)
    {
        juce::Path fill (curve);
        fill.lineTo (area.getRight(), area.getBottom());
        fill.lineTo (area.getX(), area.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (Palette::graph.withAlpha (0.35f), 0.0f, area.getY(),
                                                 Palette::graph.withAlpha (0.05f), 0.0f, area.getBottom(), false));
        g.fillPath (fill);
        g.setColour (Palette::graph);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

//==============================================================================
GraphPanel::GraphPanel (juce::String t, juce::AudioProcessorValueTreeState& state, Field a, Field b)
    : title (std::move (t)),
      first (state, a.paramId, a.label),
      second (state, b.paramId, b.label)
{
    addAndMakeVisible (first);
    addAndMakeVisible (second);
}

std::array<float, 11> GraphPanel::keyOf (const reckless::ChorusParams& p)
{
    return { (float) p.engine, p.quality, p.rejection, p.emphasis, p.noise, p.variance,
             p.reactivityMs, p.ratio, p.lowCutHz, p.highCutHz, 0.0f };
}

void GraphPanel::refresh (const reckless::ChorusParams& p)
{
    params = p;
    time += 1.0 / 30.0;

    const auto key = keyOf (p);
    if (key != lastKey || isAnimated())
    {
        lastKey = key;
        repaint();
    }
}

void GraphPanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Palette::panel);
    g.fillRect (b);

    g.setColour (Palette::textDim);
    g.setFont (Fonts::bold (11.5f));
    g.drawText (title.toUpperCase(), b.removeFromTop (kTitleHeight), juce::Justification::centred);

    b.removeFromBottom ((float) (kFieldHeight * 2 + 12));
    const auto graphArea = b.reduced (0.0f, 4.0f);

    // Faint grid
    g.setColour (Palette::panelLine.withAlpha (0.5f));
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (graphArea.getY() + graphArea.getHeight() * (float) i / 4.0f), graphArea.getX(), graphArea.getRight());

    g.saveState();
    g.reduceClipRegion (graphArea.toNearestInt());
    drawGraph (g, graphArea);
    g.restoreState();
}

void GraphPanel::resized()
{
    auto b = getLocalBounds().reduced (12, 6);
    second.setBounds (b.removeFromBottom (kFieldHeight));
    b.removeFromBottom (2);
    first.setBounds (b.removeFromBottom (kFieldHeight));
}

//==============================================================================
AliasingPanel::AliasingPanel (juce::AudioProcessorValueTreeState& s)
    : GraphPanel ("Aliasing", s, { "rejection", "Rejection" }, { "emphasis", "Emphasis" }) {}

void AliasingPanel::drawGraph (juce::Graphics& g, juce::Rectangle<float> area)
{
    constexpr double lo = 20.0, hi = 24000.0;

    const auto clock = reckless::clockHzFromQuality (params.quality);
    const auto cutoff = reckless::antiAliasCutoff (kDisplayRate, clock, params.rejection);
    reckless::Biquad s1, s2, r2;
    s1.setLowpass (kDisplayRate, cutoff, 0.541);
    s2.setLowpass (kDisplayRate, cutoff, 1.307);
    r2.setLowpass (kDisplayRate, cutoff, reckless::reconstructionQ (params.emphasis));

    juce::Path curve;
    auto firstPoint = true;
    for (auto x = area.getX(); x <= area.getRight(); x += 1.5f)
    {
        const auto f = xToFreq (x, area, lo, hi);
        const auto mag = s1.magnitude (kDisplayRate, f) * s1.magnitude (kDisplayRate, f)
                       * s2.magnitude (kDisplayRate, f) * r2.magnitude (kDisplayRate, f);
        const auto db = 20.0 * std::log10 (std::max (mag, 1.0e-6));
        const auto y = dbToY (db, area, 18.0, -60.0);
        if (std::exchange (firstPoint, false)) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    drawCurve (g, curve, area);

    // Nyquist of the BBD clock
    const auto nyquistX = freqToX (clock * 0.5, area, lo, hi);
    if (nyquistX < area.getRight())
    {
        g.setColour (Palette::textDim.withAlpha (0.6f));
        const float dashes[] = { 3.0f, 3.0f };
        g.drawDashedLine ({ nyquistX, area.getY(), nyquistX, area.getBottom() }, dashes, 2, 1.0f);
    }
}

//==============================================================================
StabilityPanel::StabilityPanel (juce::AudioProcessorValueTreeState& s)
    : GraphPanel ("Stability", s, { "noise", "Noise" }, { "variance", "Variance" }) {}

void StabilityPanel::drawGraph (juce::Graphics& g, juce::Rectangle<float> area)
{
    juce::Random rng ((juce::int64) (time * 30.0));
    const auto mid = area.getCentreY();
    const auto noiseAmp = area.getHeight() * 0.4f * params.noise * params.noise;
    const auto driftAmp = area.getHeight() * 0.25f * params.variance;

    juce::Path curve;
    auto firstPoint = true;
    for (auto x = area.getX(); x <= area.getRight(); x += 2.0f)
    {
        const auto t = (x - area.getX()) / area.getWidth();
        const auto drift = driftAmp * (0.6f * std::sin (t * 7.0f + (float) time * 0.9f)
                                     + 0.4f * std::sin (t * 17.0f - (float) time * 1.7f));
        const auto y = mid + drift + noiseAmp * (rng.nextFloat() * 2.0f - 1.0f);
        if (std::exchange (firstPoint, false)) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    drawCurve (g, curve, area);
}

//==============================================================================
CompanderPanel::CompanderPanel (juce::AudioProcessorValueTreeState& s)
    : GraphPanel ("Compander", s, { "reactivity", "Reactivity" }, { "ratio", "Ratio" }) {}

void CompanderPanel::drawGraph (juce::Graphics& g, juce::Rectangle<float> area)
{
    // Envelope of a tone burst going through the compressor: the higher the ratio,
    // the flatter the plateau; the reactivity sets how fast the detector follows.
    const auto tau = juce::jmap (std::log (params.reactivityMs / 0.5f) / std::log (100.0f), 0.004f, 0.12f);
    const auto depth = 1.0f / params.ratio;

    juce::Path top, bottom;
    auto env = 0.0f;
    const auto step = 1.5f;
    const auto coeff = 1.0f - std::exp (-step / (tau * area.getWidth()));
    const auto mid = area.getCentreY();

    for (auto x = area.getX(); x <= area.getRight(); x += step)
    {
        const auto t = (x - area.getX()) / area.getWidth();
        const auto burst = (t > 0.3f && t < 0.72f) ? 1.0f : 0.25f;
        env += (burst - env) * coeff;

        const auto level = 0.12f + 0.75f * std::pow (env, depth);
        const auto yTop = mid - level * area.getHeight() * 0.45f;
        const auto yBottom = mid + level * area.getHeight() * 0.45f;
        if (x <= area.getX())
        {
            top.startNewSubPath (x, yTop);
            bottom.startNewSubPath (x, yBottom);
        }
        else
        {
            top.lineTo (x, yTop);
            bottom.lineTo (x, yBottom);
        }
    }

    g.setColour (Palette::graph.withAlpha (0.15f));
    juce::Path band (top);
    for (auto x = area.getRight(); x >= area.getX(); x -= step)
        band.lineTo (x, mid);
    band.closeSubPath();
    g.fillPath (band);

    g.setColour (Palette::graph);
    g.strokePath (top, juce::PathStrokeType (2.0f));
    g.strokePath (bottom, juce::PathStrokeType (2.0f));
}

//==============================================================================
InputFilterPanel::InputFilterPanel (juce::AudioProcessorValueTreeState& s)
    : GraphPanel ("Input Filter", s, { "lowCut", "Low Cut" }, { "highCut", "High Cut" }) {}

void InputFilterPanel::drawGraph (juce::Graphics& g, juce::Rectangle<float> area)
{
    constexpr double lo = 20.0, hi = 20000.0;

    reckless::Biquad hp, lp;
    hp.setHighpass (kDisplayRate, params.lowCutHz, 0.707);
    lp.setLowpass (kDisplayRate, params.highCutHz, 0.707);

    juce::Path curve;
    auto firstPoint = true;
    for (auto x = area.getX(); x <= area.getRight(); x += 1.5f)
    {
        const auto f = xToFreq (x, area, lo, hi);
        const auto mag = hp.magnitude (kDisplayRate, f) * lp.magnitude (kDisplayRate, f);
        const auto y = dbToY (20.0 * std::log10 (std::max (mag, 1.0e-6)), area, 6.0, -36.0);
        if (std::exchange (firstPoint, false)) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    drawCurve (g, curve, area);
}
