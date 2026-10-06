#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace reckless
{

constexpr double kPi = 3.14159265358979323846;
constexpr float kTwoPi = 6.28318530717958647692f;

//==============================================================================
/** RBJ biquad, transposed direct form II. Coefficients are recomputed per block. */
class Biquad
{
public:
    void reset() noexcept { z1 = z2 = 0.0; }

    void setLowpass (double sampleRate, double freq, double q) noexcept
    {
        const auto w = omega (sampleRate, freq);
        const auto cs = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        setNormalised ((1.0 - cs) * 0.5, 1.0 - cs, (1.0 - cs) * 0.5,
                       1.0 + alpha, -2.0 * cs, 1.0 - alpha);
    }

    void setHighpass (double sampleRate, double freq, double q) noexcept
    {
        const auto w = omega (sampleRate, freq);
        const auto cs = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        setNormalised ((1.0 + cs) * 0.5, -(1.0 + cs), (1.0 + cs) * 0.5,
                       1.0 + alpha, -2.0 * cs, 1.0 - alpha);
    }

    void setHighShelf (double sampleRate, double freq, double gainDb) noexcept
    {
        const auto a = std::pow (10.0, gainDb / 40.0);
        const auto w = omega (sampleRate, freq);
        const auto cs = std::cos (w);
        const auto alpha = std::sin (w) / 2.0 * std::sqrt (2.0);
        const auto sq = 2.0 * std::sqrt (a) * alpha;
        setNormalised (a * ((a + 1.0) + (a - 1.0) * cs + sq),
                       -2.0 * a * ((a - 1.0) + (a + 1.0) * cs),
                       a * ((a + 1.0) + (a - 1.0) * cs - sq),
                       (a + 1.0) - (a - 1.0) * cs + sq,
                       2.0 * ((a - 1.0) - (a + 1.0) * cs),
                       (a + 1.0) - (a - 1.0) * cs - sq);
    }

    void setIdentity() noexcept { b0 = 1.0; b1 = b2 = a1 = a2 = 0.0; }

    float process (float x) noexcept
    {
        const auto in = static_cast<double> (x);
        const auto y = b0 * in + z1;
        z1 = b1 * in - a1 * y + z2;
        z2 = b2 * in - a2 * y;
        return static_cast<float> (y);
    }

    /** Magnitude response at a frequency (used by the UI graphs). */
    double magnitude (double sampleRate, double freq) const noexcept
    {
        const auto w = 2.0 * kPi * freq / sampleRate;
        const auto c1 = std::cos (w), s1 = std::sin (w);
        const auto c2 = std::cos (2.0 * w), s2 = std::sin (2.0 * w);
        const auto nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
        const auto dr = 1.0 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        return std::sqrt ((nr * nr + ni * ni) / (dr * dr + di * di));
    }

private:
    static double omega (double sampleRate, double freq) noexcept
    {
        return 2.0 * kPi * std::clamp (freq, 5.0, 0.49 * sampleRate) / sampleRate;
    }

    void setNormalised (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0;
        a1 = na1 / na0; a2 = na2 / na0;
    }

    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
};

//==============================================================================
/** Circular buffer with 4-point Hermite fractional reads. */
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        std::size_t size = 1;
        while (size < static_cast<std::size_t> (maxDelaySamples + 8))
            size <<= 1;
        buffer.assign (size, 0.0f);
        mask = size - 1;
        writeIndex = 0;
    }

    void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    void push (float x) noexcept
    {
        buffer[writeIndex] = x;
        writeIndex = (writeIndex + 1) & mask;
    }

    /** Reads `delay` samples behind the most recently pushed sample. */
    float read (float delay) const noexcept
    {
        const auto maxDelay = static_cast<float> (mask) - 4.0f;
        delay = std::clamp (delay, 2.0f, maxDelay);
        const auto pos = static_cast<float> (writeIndex) - 1.0f - delay + static_cast<float> (mask + 1);
        const auto ip = static_cast<std::size_t> (pos);
        const auto f = pos - static_cast<float> (ip);

        const auto xm1 = buffer[(ip - 1) & mask];
        const auto x0  = buffer[ip & mask];
        const auto x1  = buffer[(ip + 1) & mask];
        const auto x2  = buffer[(ip + 2) & mask];

        const auto c1 = 0.5f * (x1 - xm1);
        const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

private:
    std::vector<float> buffer;
    std::size_t mask = 0, writeIndex = 0;
};

//==============================================================================
/** Zero-order hold running at an arbitrary clock rate (BBD sampling / bit-crusher style). */
class SampleHold
{
public:
    void reset() noexcept { phase = 1.0; held = 0.0f; }

    float process (float x, double clockRatio) noexcept
    {
        if (clockRatio >= 1.0)
            return held = x;

        phase += clockRatio;
        if (phase >= 1.0)
        {
            phase -= 1.0;
            held = x;
        }
        return held;
    }

private:
    double phase = 1.0;
    float held = 0.0f;
};

//==============================================================================
class EnvelopeFollower
{
public:
    void setTimes (double sampleRate, double attackMs, double releaseMs) noexcept
    {
        attack  = static_cast<float> (std::exp (-1000.0 / (attackMs * sampleRate)));
        release = static_cast<float> (std::exp (-1000.0 / (releaseMs * sampleRate)));
    }

    void reset (float value = 0.0f) noexcept { env = value; }

    float process (float x) noexcept
    {
        const auto in = std::abs (x);
        const auto coeff = in > env ? attack : release;
        env = in + coeff * (env - in);
        return env;
    }

private:
    float attack = 0.0f, release = 0.0f, env = 0.0f;
};

//==============================================================================
/** Small deterministic PRNG (xorshift32) — real-time safe. */
class Random
{
public:
    explicit Random (std::uint32_t seed = 0x9E3779B9u) noexcept : state (seed ? seed : 1u) {}

    void seed (std::uint32_t s) noexcept { state = s ? s : 1u; }

    /** Uniform in [-1, 1). */
    float bipolar() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float> (state) * (2.0f / 4294967296.0f) - 1.0f;
    }

private:
    std::uint32_t state;
};

//==============================================================================
/** One-pole parameter smoother. */
class Smoothed
{
public:
    void setTime (double sampleRate, double timeMs) noexcept
    {
        coeff = static_cast<float> (std::exp (-1000.0 / (timeMs * sampleRate)));
    }

    void snap (float v) noexcept { current = target = v; }
    void setTarget (float v) noexcept { target = v; }

    float next() noexcept
    {
        current = target + coeff * (current - target);
        if (std::abs (current - target) < 1.0e-6f)
            current = target;
        return current;
    }

    float get() const noexcept { return current; }

private:
    float coeff = 0.0f, current = 0.0f, target = 0.0f;
};

} // namespace reckless
