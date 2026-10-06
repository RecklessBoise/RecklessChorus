#include "ChorusCore.h"

namespace reckless
{

namespace
{
    constexpr float kCompanderRef = 0.1f;      // -20 dBFS pivot level
    constexpr float kMaxCompanderGain = 31.6f; // +30 dB
    constexpr int kDriftInterval = 256;        // samples between new drift targets

    /** Rational tanh approximation: soft BBD-style saturation. */
    inline float softClip (float x) noexcept
    {
        x = std::clamp (x, -3.0f, 3.0f);
        const auto x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float companderGain (float env, float exponent, float maxGain = kMaxCompanderGain) noexcept
    {
        return std::min (std::pow (std::max (env, 1.0e-5f) / kCompanderRef, exponent), maxGain);
    }
}

ChorusCore::ChorusCore()
{
    randomiseVoices();
}

void ChorusCore::prepare (double newSampleRate, int /*maxBlockSize*/)
{
    sampleRate = newSampleRate;

    const auto maxDelayMs = kCentreDelayMs + kMaxDepthMs + 4.0f;
    delay.prepare (static_cast<int> (std::ceil (maxDelayMs * 0.001 * sampleRate)));

    mixSmoothed.setTime (sampleRate, 30.0);
    outputSmoothed.setTime (sampleRate, 30.0);
    depthSmoothed.setTime (sampleRate, 60.0);
    voicesSmoothed.setTime (sampleRate, 80.0);
    varianceSmoothed.setTime (sampleRate, 100.0);

    reset();
    setParams (params);
}

void ChorusCore::reset()
{
    delay.reset();
    for (auto* f : { &inLowCut, &inHighCut, &preEmphasis, &antiAlias[0], &antiAlias[1],
                     &deEmphasis[0], &deEmphasis[1], &edgeShelf[0], &edgeShelf[1] })
        f->reset();
    for (auto& ch : reconstruction)
        for (auto& f : ch)
            f.reset();

    sampleHold.reset();
    compressorEnv.reset (kCompanderRef);
    for (auto& e : expanderEnv)
        e.reset (kCompanderRef);

    lfoPhase = 0.0;
    drift.fill (0.0f);
    driftTarget.fill (0.0f);
    driftCounter = 0;
    random.seed (0x5EEDu);

    mixSmoothed.snap (params.mix);
    outputSmoothed.snap (std::pow (10.0f, params.outputDb / 20.0f));
    depthSmoothed.snap (params.depth);
    voicesSmoothed.snap (params.voices);
    varianceSmoothed.snap (params.variance);
}

void ChorusCore::randomiseVoices()
{
    // Fixed seed: every instance gets the same "chip lottery", so renders are repeatable.
    Random r (0xC0FFEEu);
    for (int v = 0; v < kMaxVoices; ++v)
    {
        voiceDelayDev[(std::size_t) v] = r.bipolar();
        voiceGainDev[(std::size_t) v]  = r.bipolar();
        voiceRateDev[(std::size_t) v]  = r.bipolar();
    }
}

void ChorusCore::setParams (const ChorusParams& p)
{
    params = p;
    params.voices = std::clamp (params.voices, 1.0f, static_cast<float> (kMaxVoices));
    params.ratio = std::max (params.ratio, 1.0f);

    mixSmoothed.setTarget (std::clamp (params.mix, 0.0f, 1.0f));
    outputSmoothed.setTarget (std::pow (10.0f, params.outputDb / 20.0f));
    depthSmoothed.setTarget (std::clamp (params.depth, 0.0f, 1.0f));
    voicesSmoothed.setTarget (params.voices);
    varianceSmoothed.setTarget (std::clamp (params.variance, 0.0f, 1.0f));

    const auto reactivity = std::max (0.1, static_cast<double> (params.reactivityMs));
    compressorEnv.setTimes (sampleRate, reactivity, reactivity * 4.0);
    for (auto& e : expanderEnv)
        e.setTimes (sampleRate, reactivity, reactivity * 4.0);

    updateFilters();
}

void ChorusCore::updateFilters()
{
    const auto analog = params.engine == Engine::Analog;
    const auto clockHz = clockHzFromQuality (params.quality);
    clockRatio = clockHz / sampleRate;
    digitalReducerActive = ! analog && clockRatio < 1.0;

    inLowCut.setHighpass (sampleRate, params.lowCutHz, 0.707);
    inHighCut.setLowpass (sampleRate, params.highCutHz, 0.707);

    const auto emphasisDb = 12.0 * params.emphasis;
    preEmphasis.setHighShelf (sampleRate, 2500.0, emphasisDb);
    for (auto& f : deEmphasis)
        f.setHighShelf (sampleRate, 2500.0, -emphasisDb);

    // 4th-order Butterworth anti-aliasing filter ahead of the clocked stage.
    const auto aaCutoff = analog ? antiAliasCutoff (sampleRate, clockHz, params.rejection)
                                 : 0.45 * clockHz;
    antiAlias[0].setLowpass (sampleRate, aaCutoff, 0.541);
    antiAlias[1].setLowpass (sampleRate, aaCutoff, 1.307);

    // Reconstruction filter: emphasis adds a resonant bump near the cut-off.
    for (auto& ch : reconstruction)
    {
        ch[0].setLowpass (sampleRate, aaCutoff, 0.541);
        ch[1].setLowpass (sampleRate, aaCutoff, reconstructionQ (params.emphasis));
    }

    const auto edgeDb = (params.edge - 0.5) * 24.0;
    for (auto& f : edgeShelf)
        f.setHighShelf (sampleRate, 3000.0, edgeDb);
}

void ChorusCore::process (float* left, float* right, int numSamples) noexcept
{
    const auto analog = params.engine == Engine::Analog;
    const auto stereo = right != nullptr;
    const auto wide = stereo && params.wide;
    const auto shape = params.shape;
    const auto lfoInc = static_cast<double> (params.speedHz) / sampleRate;
    const auto msToSamples = static_cast<float> (sampleRate * 0.001);
    const auto compander = analog && params.ratio > 1.001f;
    const auto compressExp = 1.0f / params.ratio - 1.0f;
    const auto expandExp = params.ratio - 1.0f;
    // The expander may only restore what the compressor took from a +6 dBFS peak, so
    // detector mismatch pumps the level instead of blowing it up.
    const auto maxExpandGain = std::pow (2.0f / kCompanderRef, expandExp / params.ratio);
    const auto noiseAmp = 0.03f * params.noise * params.noise;
    const auto driftCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.4 * sampleRate)));

    for (int i = 0; i < numSamples; ++i)
    {
        const auto dryL = left[i];
        const auto dryR = stereo ? right[i] : dryL;

        //======================================================================
        // Write side: everything ahead of the delay line is mono.
        auto x = 0.5f * (dryL + dryR);
        x = inHighCut.process (inLowCut.process (x));

        if (analog)
        {
            x = preEmphasis.process (x);
            if (compander)
                x *= companderGain (compressorEnv.process (x), compressExp);
            x = antiAlias[1].process (antiAlias[0].process (x));
            x = sampleHold.process (x, clockRatio);
            x = softClip (x);
            x += noiseAmp * 0.5f * (random.bipolar() + random.bipolar());
        }
        else if (digitalReducerActive)
        {
            x = antiAlias[1].process (antiAlias[0].process (x));
            x = sampleHold.process (x, clockRatio);
        }

        delay.push (x);

        //======================================================================
        // Voices
        lfoPhase += lfoInc;
        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;

        const auto voices = voicesSmoothed.next();
        const auto depthMs = depthSmoothed.next() * kMaxDepthMs;
        const auto variance = varianceSmoothed.next();

        if (++driftCounter >= kDriftInterval)
        {
            driftCounter = 0;
            for (auto& t : driftTarget)
                t = random.bipolar();
        }

        auto wetL = 0.0f, wetR = 0.0f, gainSum = 0.0f;

        for (int v = 0; v < kMaxVoices; ++v)
        {
            const auto idx = static_cast<std::size_t> (v);
            drift[idx] += (driftTarget[idx] - drift[idx]) * driftCoeff;

            const auto g = voiceGain (voices, v);
            if (g <= 0.0f)
                continue;

            const auto phase = static_cast<float> (lfoPhase)
                             + static_cast<float> (v) / voices
                             + variance * 0.08f * (voiceRateDev[idx] + drift[idx]);
            const auto centreMs = kCentreDelayMs + variance * (1.5f * voiceDelayDev[idx] + 0.6f * drift[idx]);
            const auto gain = g * (1.0f + 0.2f * variance * voiceGainDev[idx]);
            // Right taps sit halfway between left taps: inverted LFO for one voice (Juno style),
            // interleaved phases for several voices so even counts stay decorrelated.
            const auto rightOffset = 0.5f / voices;

            const auto sL = delay.read ((centreMs + depthMs * lfoValue (shape, phase)) * msToSamples);
            const auto sR = wide ? delay.read ((centreMs + depthMs * lfoValue (shape, phase + rightOffset)) * msToSamples)
                                 : sL;

            wetL += gain * sL;
            wetR += gain * sR;
            gainSum += g;
        }

        const auto norm = 1.0f / std::sqrt (std::max (gainSum, 1.0f));
        wetL *= norm;
        wetR *= norm;

        //======================================================================
        // Read side
        if (analog)
        {
            wetL = reconstruction[0][1].process (reconstruction[0][0].process (wetL));
            if (compander)
                wetL *= companderGain (expanderEnv[0].process (wetL), expandExp, maxExpandGain);
            wetL = deEmphasis[0].process (wetL);

            if (stereo)
            {
                wetR = reconstruction[1][1].process (reconstruction[1][0].process (wetR));
                if (compander)
                    wetR *= companderGain (expanderEnv[1].process (wetR), expandExp, maxExpandGain);
                wetR = deEmphasis[1].process (wetR);
            }
        }

        wetL = edgeShelf[0].process (wetL);
        if (stereo)
            wetR = edgeShelf[1].process (wetR);

        const auto mix = mixSmoothed.next();
        const auto out = outputSmoothed.next();

        left[i] = (dryL * (1.0f - mix) + wetL * mix) * out;
        if (stereo)
            right[i] = (dryR * (1.0f - mix) + wetR * mix) * out;
    }

    lfoPhaseForUi.store (static_cast<float> (lfoPhase), std::memory_order_relaxed);
}

} // namespace reckless
