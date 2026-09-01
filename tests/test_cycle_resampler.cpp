#include "TestHarness.h"
#include "dsp/CycleResampler.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

using v0a::CycleResampler;
using v0a::CycleResamplerConfig;
using v0a::InterpolationMethod;

namespace
{
CycleResamplerConfig validConfig()
{
    return {
        .periodSamples = 100.0,
        .rate = 1.0,
        .bufferCapacity = 4096,
        .safetyDelaySamples = 512.0,
        .interpolation = InterpolationMethod::Linear,
        .spliceCrossfadeSamples = 0,
    };
}

bool constructionThrows(CycleResamplerConfig config)
{
    try
    {
        const CycleResampler resampler(config);
        static_cast<void>(resampler);
        return false;
    }
    catch (const std::invalid_argument&)
    {
        return true;
    }
}
} // namespace

TEST_CASE("configuration rejects nonpositive and nonfinite periods and rates")
{
    auto config = validConfig();
    config.periodSamples = 0.0;
    REQUIRE(constructionThrows(config));
    config = validConfig();
    config.periodSamples = std::numeric_limits<double>::infinity();
    REQUIRE(constructionThrows(config));
    config = validConfig();
    config.rate = -1.0;
    REQUIRE(constructionThrows(config));
    config = validConfig();
    config.rate = std::numeric_limits<double>::quiet_NaN();
    REQUIRE(constructionThrows(config));
}

TEST_CASE("configuration rejects unsafe delay buffer and crossfade relationships")
{
    auto config = validConfig();
    config.safetyDelaySamples = 50.0;
    REQUIRE(constructionThrows(config));
    config = validConfig();
    config.bufferCapacity = 550;
    REQUIRE(constructionThrows(config));
    config = validConfig();
    config.spliceCrossfadeSamples = 100;
    REQUIRE(constructionThrows(config));
}

TEST_CASE("unity rate stays finite and emits no cycle events")
{
    CycleResampler resampler(validConfig());
    for (std::size_t index = 0; index < 20'000; ++index)
        REQUIRE(std::isfinite(resampler.processSample(0.25F)));

    REQUIRE(resampler.stats().repeatedCycles == 0);
    REQUIRE(resampler.stats().deletedCycles == 0);
    REQUIRE_NEAR(resampler.stats().residualSeparation, 0.0, 1.0e-9);
}

TEST_CASE("runtime period and rate updates drive the existing bounded compensation loop")
{
    CycleResampler resampler(validConfig());
    REQUIRE(resampler.setPeriodAndRate(80.0, 1.04));
    for (std::size_t index = 0; index < 40'000; ++index)
        static_cast<void>(resampler.processSample(0.0F));

    REQUIRE(resampler.stats().repeatedCycles > 0);
    REQUIRE(resampler.stats().deletedCycles == 0);
    REQUIRE(std::abs(resampler.stats().residualSeparation) < 80.0);

    resampler.reset();
    REQUIRE(resampler.setPeriodAndRate(120.0, 0.96));
    for (std::size_t index = 0; index < 40'000; ++index)
        static_cast<void>(resampler.processSample(0.0F));

    REQUIRE(resampler.stats().deletedCycles > 0);
    REQUIRE(resampler.stats().repeatedCycles == 0);
    REQUIRE(std::abs(resampler.stats().residualSeparation) < 120.0);
}

TEST_CASE("invalid runtime updates are rejected without changing unity behavior")
{
    CycleResampler resampler(validConfig());
    REQUIRE(!resampler.setPeriodAndRate(0.0, 1.0));
    REQUIRE(!resampler.setPeriodAndRate(100.0, 0.0));
    REQUIRE(!resampler.setPeriodAndRate(510.0, 1.0));
    REQUIRE(!resampler.setPeriodAndRate(std::numeric_limits<double>::quiet_NaN(), 1.0));

    for (std::size_t index = 0; index < 20'000; ++index)
        static_cast<void>(resampler.processSample(0.0F));
    REQUIRE(resampler.stats().repeatedCycles == 0);
    REQUIRE(resampler.stats().deletedCycles == 0);
}

TEST_CASE("runtime interpolation and splice controls remain within prepared limits")
{
    CycleResampler resampler(validConfig());
    REQUIRE(resampler.setInterpolation(InterpolationMethod::CubicHermite));
    REQUIRE(resampler.setInterpolation(InterpolationMethod::Lagrange4));
    REQUIRE(resampler.setSpliceCrossfadeSamples(16));
    REQUIRE(!resampler.setSpliceCrossfadeSamples(100));
    REQUIRE(resampler.setPeriodAndRate(80.0, 1.02));
    REQUIRE(!resampler.setSpliceCrossfadeSamples(80));
    for (std::size_t index = 0; index < 10'000; ++index)
        REQUIRE(std::isfinite(resampler.processSample(static_cast<float>(std::sin(index * 0.03)))));
}

TEST_CASE("rate above unity repeats cycles and rate below unity deletes cycles")
{
    auto sharper = validConfig();
    sharper.rate = 1.05;
    CycleResampler repeatResampler(sharper);

    auto flatter = validConfig();
    flatter.rate = 0.95;
    CycleResampler deleteResampler(flatter);

    for (std::size_t index = 0; index < 20'000; ++index)
    {
        static_cast<void>(repeatResampler.processSample(0.0F));
        static_cast<void>(deleteResampler.processSample(0.0F));
    }

    REQUIRE(repeatResampler.stats().repeatedCycles > 0);
    REQUIRE(repeatResampler.stats().deletedCycles == 0);
    REQUIRE(deleteResampler.stats().deletedCycles > 0);
    REQUIRE(deleteResampler.stats().repeatedCycles == 0);
    REQUIRE_NEAR(repeatResampler.stats().totalCycleCompensationSamples,
                 -static_cast<double>(repeatResampler.stats().repeatedCycles) * sharper.periodSamples,
                 1.0e-9);
    REQUIRE_NEAR(deleteResampler.stats().totalCycleCompensationSamples,
                 static_cast<double>(deleteResampler.stats().deletedCycles) * flatter.periodSamples,
                 1.0e-9);
}

TEST_CASE("stationary event density converges to absolute rate drift divided by period")
{
    struct Case
    {
        double period;
        double rate;
    };
    constexpr std::size_t sampleCount = 200'000;

    for (const auto testCase : { Case { 100.0, 1.03 },
                                 Case { 100.0, 0.97 },
                                 Case { 137.5, 1.059463 },
                                 Case { 137.5, 0.943874 },
                                 Case { 80.25, 1.001 } })
    {
        auto config = validConfig();
        config.periodSamples = testCase.period;
        config.rate = testCase.rate;
        CycleResampler resampler(config);
        for (std::size_t index = 0; index < sampleCount; ++index)
            static_cast<void>(resampler.processSample(0.0F));

        const auto events = resampler.stats().repeatedCycles + resampler.stats().deletedCycles;
        const double measuredDensity = static_cast<double>(events) / static_cast<double>(sampleCount);
        const double expectedDensity = std::abs(testCase.rate - 1.0) / testCase.period;
        const double residualWidth = 2.0 * testCase.period;
        const double finiteRenderBound = residualWidth
                                       / (static_cast<double>(sampleCount) * testCase.period);
        REQUIRE_NEAR(measuredDensity, expectedDensity, finiteRenderBound);
    }
}

TEST_CASE("zero-crossfade processing is deterministic across reset")
{
    auto config = validConfig();
    config.rate = 1.059463;
    CycleResampler resampler(config);
    std::vector<float> first;
    std::vector<float> second;
    first.reserve(10'000);
    second.reserve(10'000);

    for (std::size_t index = 0; index < 10'000; ++index)
        first.push_back(resampler.processSample(static_cast<float>(std::sin(index * 0.03))));
    resampler.reset();
    for (std::size_t index = 0; index < 10'000; ++index)
        second.push_back(resampler.processSample(static_cast<float>(std::sin(index * 0.03))));

    REQUIRE(first == second);
}

TEST_CASE("nonzero splice crossfade changes the event transition while remaining deterministic")
{
    auto rawConfig = validConfig();
    rawConfig.periodSamples = 32.0;
    rawConfig.rate = 1.25;
    rawConfig.spliceCrossfadeSamples = 0;
    auto fadedConfig = rawConfig;
    fadedConfig.spliceCrossfadeSamples = 8;

    CycleResampler raw(rawConfig);
    CycleResampler faded(fadedConfig);
    std::vector<float> rawOutput;
    std::vector<float> fadedOutput;
    rawOutput.reserve(4'000);
    fadedOutput.reserve(4'000);

    for (std::size_t index = 0; index < 4'000; ++index)
    {
        const float input = static_cast<float>(0.7 * std::sin(index * 0.071)
                                             + 0.2 * std::sin(index * 0.013));
        rawOutput.push_back(raw.processSample(input));
        fadedOutput.push_back(faded.processSample(input));
    }

    REQUIRE(raw.stats().repeatedCycles == faded.stats().repeatedCycles);
    REQUIRE(rawOutput != fadedOutput);

    faded.reset();
    for (std::size_t index = 0; index < 4'000; ++index)
    {
        const float input = static_cast<float>(0.7 * std::sin(index * 0.071)
                                             + 0.2 * std::sin(index * 0.013));
        REQUIRE(faded.processSample(input) == fadedOutput[index]);
    }
}

int main()
{
    return test::runAll();
}
