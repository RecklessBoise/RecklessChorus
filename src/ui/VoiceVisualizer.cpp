#include "VoiceVisualizer.h"
#include "RecklessLookAndFeel.h"

VoiceVisualizer::VoiceVisualizer()
{
    setInterceptsMouseClicks (false, false);
    setOpaque (false);
}

void VoiceVisualizer::update (float lfoPhase, const reckless::ChorusParams& p, double elapsedSeconds)
{
    phase = lfoPhase;
    params = p;
    // Slow drift so the ring feels alive even at low LFO rates; faster LFOs spin it a bit more.
    rotation += static_cast<float> (elapsedSeconds) * (0.12f + 0.05f * std::sqrt (params.speedHz));
    rotation = std::fmod (rotation, juce::MathConstants<float>::twoPi);
    repaint();
}

void VoiceVisualizer::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto digital = params.engine == reckless::Engine::Digital;
    const auto accent = Palette::accentFor (digital);

    const auto baseRadius = size * 0.29f;
    const auto orbit = size * (0.06f + 0.11f * params.depth);
    const auto perVoice = 8;
    const auto activeVoices = juce::jmax (1, (int) std::ceil (params.voices - 1.0e-3f));
    const auto count = activeVoices * perVoice;

    // Soft glow behind the ring
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.10f), centre.x, centre.y,
                                             accent.withAlpha (0.0f), centre.x + size * 0.5f, centre.y, true));
    g.fillEllipse (bounds.withSizeKeepingCentre (size, size));

    for (int k = 0; k < count; ++k)
    {
        const auto voiceIndex = k % activeVoices; // interleave voices around the ring
        const auto gain = reckless::voiceGain (params.voices, voiceIndex);
        if (gain <= 0.0f)
            continue;

        const auto t = (float) k / (float) count;
        const auto lfo = reckless::lfoValue (params.shape, phase + (float) voiceIndex / juce::jmax (1.0f, params.voices));
        const auto angle = rotation + t * juce::MathConstants<float>::twoPi;
        const auto distance = orbit * (1.0f + 0.35f * lfo);
        const auto c = centre + juce::Point<float> (std::cos (angle), std::sin (angle)) * distance;
        const auto r = baseRadius * (1.0f + 0.04f * lfo);

        // Digital engine draws crisp circles; analog ones wobble slightly with variance.
        const auto squash = 1.0f - (digital ? 0.0f : 0.06f * params.variance * std::sin (angle * 3.0f + phase * 6.0f));
        const auto alpha = juce::jmap (gain, 0.0f, 1.0f, 0.0f, 0.75f) * (0.55f + 0.45f * std::abs (std::sin (angle + phase * 3.0f)));

        g.setColour (accent.withAlpha (alpha));
        g.drawEllipse (c.x - r, c.y - r * squash, r * 2.0f, r * 2.0f * squash, 1.3f);
    }
}
