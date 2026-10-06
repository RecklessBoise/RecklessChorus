#include "dsp/ChorusCore.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>
#include <vector>

using namespace reckless;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlock = 256;

    struct Signal
    {
        std::vector<float> left, right;
    };

    Signal makeSine (int numSamples, float freq, float amp = 0.5f)
    {
        Signal s { std::vector<float> ((std::size_t) numSamples), std::vector<float> ((std::size_t) numSamples) };
        for (int i = 0; i < numSamples; ++i)
        {
            const auto v = amp * std::sin (kTwoPi * freq * (float) i / (float) kSampleRate);
            s.left[(std::size_t) i] = v;
            s.right[(std::size_t) i] = v;
        }
        return s;
    }

    void run (ChorusCore& core, Signal& s, bool stereo = true)
    {
        const auto n = (int) s.left.size();
        for (int pos = 0; pos < n; pos += kBlock)
        {
            const auto len = std::min (kBlock, n - pos);
            core.process (s.left.data() + pos, stereo ? s.right.data() + pos : nullptr, len);
        }
    }

    bool allFinite (const std::vector<float>& v)
    {
        return std::all_of (v.begin(), v.end(), [] (float x) { return std::isfinite (x); });
    }

    float peak (const std::vector<float>& v, std::size_t from = 0)
    {
        auto p = 0.0f;
        for (auto i = from; i < v.size(); ++i)
            p = std::max (p, std::abs (v[i]));
        return p;
    }

    std::unique_ptr<ChorusCore> makeCore (const ChorusParams& p)
    {
        auto core = std::make_unique<ChorusCore>();
        core->setParams (p);
        core->prepare (kSampleRate, kBlock);
        return core;
    }
}

TEST_CASE ("Mix at zero passes the dry signal through bit-exactly", "[mix]")
{
    const auto engine = GENERATE (Engine::Analog, Engine::Digital);

    ChorusParams p;
    p.engine = engine;
    p.mix = 0.0f;
    p.noise = 1.0f;
    p.quality = 0.2f;

    auto core = makeCore (p);
    auto signal = makeSine (kBlock * 40, 440.0f);
    const auto reference = signal;
    run (*core, signal);

    REQUIRE (signal.left == reference.left);
    REQUIRE (signal.right == reference.right);
}

TEST_CASE ("Output stays finite and bounded across extreme settings", "[stability]")
{
    const auto engine = GENERATE (Engine::Analog, Engine::Digital);
    const auto voices = GENERATE (1.0f, 3.5f, 8.0f);
    const auto quality = GENERATE (0.0f, 1.0f);

    ChorusParams p;
    p.engine = engine;
    p.voices = voices;
    p.quality = quality;
    p.depth = 1.0f;
    p.speedHz = 10.0f;
    p.mix = 1.0f;
    p.edge = 1.0f;
    p.emphasis = 1.0f;
    p.rejection = 0.0f;
    p.noise = 1.0f;
    p.variance = 1.0f;
    p.ratio = 4.0f;
    p.reactivityMs = 0.5f;

    auto core = makeCore (p);
    auto signal = makeSine (kBlock * 200, 3000.0f, 1.0f);
    run (*core, signal);

    REQUIRE (allFinite (signal.left));
    REQUIRE (allFinite (signal.right));
    REQUIRE (peak (signal.left) < 8.0f);
    REQUIRE (peak (signal.right) < 8.0f);
}

TEST_CASE ("Silence in gives (near) silence out without hiss", "[silence]")
{
    const auto engine = GENERATE (Engine::Analog, Engine::Digital);

    ChorusParams p;
    p.engine = engine;
    p.mix = 1.0f;

    auto core = makeCore (p);
    Signal s { std::vector<float> (kBlock * 50, 0.0f), std::vector<float> (kBlock * 50, 0.0f) };
    run (*core, s);

    REQUIRE (peak (s.left) < 1.0e-6f);
    REQUIRE (peak (s.right) < 1.0e-6f);
}

TEST_CASE ("Wet signal is present and decorrelated in wide mode", "[stereo]")
{
    ChorusParams p;
    p.engine = Engine::Digital;
    p.mix = 1.0f;
    p.depth = 1.0f;
    p.speedHz = 1.0f;
    p.wide = true;

    auto core = makeCore (p);
    auto signal = makeSine (kBlock * 400, 220.0f);
    run (*core, signal);

    const std::size_t from = kBlock * 20;
    REQUIRE (peak (signal.left, from) > 0.1f);

    auto diff = 0.0f;
    for (auto i = from; i < signal.left.size(); ++i)
        diff = std::max (diff, std::abs (signal.left[i] - signal.right[i]));
    REQUIRE (diff > 0.01f);
}

TEST_CASE ("Wide off yields identical channels", "[stereo]")
{
    const auto engine = GENERATE (Engine::Analog, Engine::Digital);

    ChorusParams p;
    p.engine = engine;
    p.mix = 1.0f;
    p.wide = false;

    auto core = makeCore (p);
    auto signal = makeSine (kBlock * 40, 330.0f);
    run (*core, signal);

    REQUIRE (signal.left == signal.right);
}

TEST_CASE ("Mono processing works without a right channel", "[mono]")
{
    ChorusParams p;
    p.mix = 0.7f;
    p.voices = 5.0f;

    auto core = makeCore (p);
    auto signal = makeSine (kBlock * 40, 330.0f);
    run (*core, signal, false);

    REQUIRE (allFinite (signal.left));
    REQUIRE (peak (signal.left) > 0.1f);
}

TEST_CASE ("Voice gains morph continuously", "[voices]")
{
    REQUIRE (voiceGain (1.0f, 0) == 1.0f);
    REQUIRE (voiceGain (1.0f, 1) == 0.0f);
    REQUIRE_THAT (voiceGain (2.5f, 2), Catch::Matchers::WithinAbs (0.5f, 1.0e-6f));
    REQUIRE (voiceGain (8.0f, 7) == 1.0f);
}

TEST_CASE ("Quality maps to a 1-48 kHz clock", "[quality]")
{
    REQUIRE_THAT (clockHzFromQuality (0.0f), Catch::Matchers::WithinRel (1000.0, 1.0e-9));
    REQUIRE_THAT (clockHzFromQuality (1.0f), Catch::Matchers::WithinRel (48000.0, 1.0e-9));
}

TEST_CASE ("LFO shapes are bipolar and bounded", "[lfo]")
{
    for (auto shape : { LfoShape::Sine, LfoShape::Triangle })
        for (int i = 0; i < 1000; ++i)
        {
            const auto v = lfoValue (shape, (float) i / 333.0f - 1.0f);
            REQUIRE (v >= -1.0001f);
            REQUIRE (v <= 1.0001f);
        }

    REQUIRE_THAT (lfoValue (LfoShape::Triangle, 0.0f), Catch::Matchers::WithinAbs (1.0f, 1.0e-6f));
    REQUIRE_THAT (lfoValue (LfoShape::Triangle, 0.5f), Catch::Matchers::WithinAbs (-1.0f, 1.0e-6f));
}

TEST_CASE ("Processing is deterministic across instances", "[determinism]")
{
    ChorusParams p;
    p.noise = 0.5f;
    p.variance = 0.5f;
    p.mix = 1.0f;

    auto a = makeCore (p);
    auto b = makeCore (p);
    auto sa = makeSine (kBlock * 20, 500.0f);
    auto sb = sa;
    run (*a, sa);
    run (*b, sb);

    REQUIRE (sa.left == sb.left);
}
