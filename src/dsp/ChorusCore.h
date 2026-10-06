#pragma once

#include "DspPrimitives.h"

#include <array>
#include <atomic>

namespace reckless
{

constexpr int kMaxVoices = 8;

enum class Engine { Analog = 0, Digital = 1 };
enum class LfoShape { Sine = 0, Triangle = 1 };

/** Plain parameter snapshot, in natural units. */
struct ChorusParams
{
    Engine engine       = Engine::Analog;
    float speedHz       = 0.5f;     // LFO rate
    float depth         = 0.5f;     // 0..1
    float voices        = 2.0f;     // 1..8, continuous
    float quality       = 1.0f;     // 0..1, maps to 1..48 kHz clock
    float edge          = 0.5f;     // 0..1, wet tilt (0.5 = flat)
    LfoShape shape      = LfoShape::Sine;
    bool wide           = true;
    float mix           = 0.5f;     // 0..1
    float outputDb      = 0.0f;

    // Input filtering (wet path)
    float lowCutHz      = 20.0f;
    float highCutHz     = 20000.0f;

    // Analog settings
    float rejection     = 0.5f;     // anti-aliasing filter amount
    float emphasis      = 0.5f;     // pre/de-emphasis + filter resonance
    float noise         = 0.0f;     // BBD hiss
    float variance      = 0.0f;     // per-voice deviation and drift
    float reactivityMs  = 5.0f;     // compander detector time
    float ratio         = 2.0f;     // compander ratio
};

/** Maps the Quality control (0..1) to the BBD / sample-rate-reducer clock, 1..48 kHz. */
inline double clockHzFromQuality (float quality) noexcept
{
    return 1000.0 * std::pow (48.0, static_cast<double> (std::clamp (quality, 0.0f, 1.0f)));
}

/** Cut-off of the analog anti-aliasing / reconstruction filters. */
inline double antiAliasCutoff (double sampleRate, double clockHz, float rejection) noexcept
{
    const auto closed = 0.42 * clockHz;
    const auto open = std::max (closed, std::min (0.49 * sampleRate, 22000.0));
    return std::exp (std::log (open) + (std::log (closed) - std::log (open)) * std::clamp (rejection, 0.0f, 1.0f));
}

inline double reconstructionQ (float emphasis) noexcept { return 1.307 + 2.2 * emphasis; }

/** Bipolar LFO value for a phase in cycles. */
inline float lfoValue (LfoShape shape, float phase) noexcept
{
    const auto p = phase - std::floor (phase);
    if (shape == LfoShape::Triangle)
        return 4.0f * std::abs (p - 0.5f) - 1.0f;
    return std::sin (kTwoPi * p);
}

/** Gain of voice `index` (0-based) for a continuous voice count. */
inline float voiceGain (float voices, int index) noexcept
{
    return std::clamp (voices - static_cast<float> (index), 0.0f, 1.0f);
}

//==============================================================================
/**
    Dual-engine, morphable 8-voice chorus.

    Wet path: input filter -> [analog: pre-emphasis, compressor, AA filter,
    BBD clock (S&H), saturation, hiss] or [digital: band-limited S&H] -> shared
    delay line read by up to 8 modulated taps -> [analog: reconstruction filter,
    expander, de-emphasis] -> edge tilt -> dry/wet mix.
*/
class ChorusCore
{
public:
    ChorusCore();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    /** Call once per block before process(). */
    void setParams (const ChorusParams& p);

    /** In-place processing. `right` may be nullptr for mono. */
    void process (float* left, float* right, int numSamples) noexcept;

    /** Master LFO phase in cycles [0, 1), for the UI. */
    float getLfoPhase() const noexcept { return lfoPhaseForUi.load (std::memory_order_relaxed); }

    double getSampleRate() const noexcept { return sampleRate; }

    static constexpr float kCentreDelayMs = 7.0f;
    static constexpr float kMaxDepthMs = 5.0f;

private:
    void updateFilters();
    void randomiseVoices();

    double sampleRate = 44100.0;
    ChorusParams params;

    DelayLine delay;
    Biquad inLowCut, inHighCut;
    Biquad preEmphasis;
    std::array<Biquad, 2> antiAlias;          // pre-BBD, mono
    std::array<std::array<Biquad, 2>, 2> reconstruction;  // [channel][stage]
    std::array<Biquad, 2> deEmphasis, edgeShelf;
    SampleHold sampleHold;
    EnvelopeFollower compressorEnv;
    std::array<EnvelopeFollower, 2> expanderEnv;
    Random random;

    Smoothed mixSmoothed, outputSmoothed, depthSmoothed, voicesSmoothed, varianceSmoothed;

    double lfoPhase = 0.0;
    std::array<float, kMaxVoices> voiceRateDev {}, voiceDelayDev {}, voiceGainDev {};
    std::array<float, kMaxVoices> drift {}, driftTarget {};
    int driftCounter = 0;

    double clockRatio = 1.0;
    bool digitalReducerActive = false;

    std::atomic<float> lfoPhaseForUi { 0.0f };
};

} // namespace reckless
