#include "TestHarness.h"
#include "dsp/PitchDetector.h"

#include <cmath>
#include <cstdint>
#include <numbers>

using v0a::PitchDetection;
using v0a::PitchDetector;
using v0a::PitchDetectorConfig;

namespace
{
PitchDetectorConfig config()
{
    return {
        .sampleRate = 48'000.0,
        .minimumF0 = 60.0,
        .maximumF0 = 800.0,
        .decimationFactor = 8,
        .hopAnalysisSamples = 4,
        .periodicityEpsilon = 0.30,
        .energyThreshold = 1.0e-5,
        .octaveImprovement = 0.02,
    };
}

template <typename Generator>
PitchDetection render(PitchDetector& detector, std::size_t samples, Generator generator)
{
    PitchDetection result;
    for (std::size_t index = 0; index < samples; ++index)
        result = detector.processSample(generator(index));
    return result;
}
} // namespace

TEST_CASE("reference detector tracks a clean sine period and exposes diagnostics")
{
    PitchDetector detector;
    detector.prepare(config());
    constexpr double f0 = 220.0;
    const auto result = render(detector, 24'000, [](std::size_t index) {
        return static_cast<float>(0.7 * std::sin(2.0 * std::numbers::pi * f0
                                                * static_cast<double>(index) / 48'000.0));
    });

    REQUIRE(result.voiced);
    REQUIRE_NEAR(result.frequency, f0, 0.8);
    REQUIRE_NEAR(result.periodSamples, 48'000.0 / f0, 0.8);
    REQUIRE(result.periodicity < 0.02);
    REQUIRE(result.confidence > 0.98);
}

TEST_CASE("harmonic saw content resolves to its fundamental rather than a systematic octave")
{
    PitchDetector detector;
    detector.prepare(config());
    constexpr double f0 = 140.0;
    const auto result = render(detector, 24'000, [](std::size_t index) {
        double sample = 0.0;
        for (int harmonic = 1; harmonic <= 16; ++harmonic)
        {
            sample += std::sin(2.0 * std::numbers::pi * f0 * harmonic
                             * static_cast<double>(index) / 48'000.0) / harmonic;
        }
        return static_cast<float>(0.35 * sample);
    });

    REQUIRE(result.voiced);
    REQUIRE_NEAR(result.frequency, f0, 1.5);
}

TEST_CASE("a discontinuous saw does not capture short merely acceptable lags")
{
    PitchDetector detector;
    detector.prepare(config());
    constexpr double startF0 = 110.0;
    constexpr double endF0 = 130.0;
    double phase = 0.0;
    std::size_t voicedUpdates = 0;
    std::size_t grossErrors = 0;
    for (std::size_t index = 0; index < 96'000; ++index)
    {
        const double fraction = static_cast<double>(index) / 95'999.0;
        const double actualF0 = startF0 + fraction * (endF0 - startF0);
        phase += actualF0 / 48'000.0;
        phase -= std::floor(phase);
        const auto result = detector.processSample(static_cast<float>(0.55 * (2.0 * phase - 1.0)));
        if (result.updated && result.voiced)
        {
            ++voicedUpdates;
            const double ratio = result.frequency / actualF0;
            if (ratio < 0.75 || ratio > 1.35)
                ++grossErrors;
        }
    }
    REQUIRE(voicedUpdates > 100);
    REQUIRE(grossErrors * 100 < voicedUpdates);
}

TEST_CASE("explicit L versus 2L arbitration recovers a weak fundamental")
{
    PitchDetector detector;
    detector.prepare(config());
    constexpr double fundamental = 110.0;
    const auto result = render(detector, 32'000, [](std::size_t index) {
        const double time = static_cast<double>(index) / 48'000.0;
        return static_cast<float>(0.16 * std::sin(2.0 * std::numbers::pi * fundamental * time)
                                + 0.70 * std::sin(4.0 * std::numbers::pi * fundamental * time));
    });

    REQUIRE(result.voiced);
    REQUIRE_NEAR(result.frequency, fundamental, 1.5);
    REQUIRE(result.usedDoubledLag);
}

TEST_CASE("silence and deterministic white noise are rejected as unvoiced")
{
    PitchDetector detector;
    detector.prepare(config());
    const auto silence = render(detector, 16'000, [](std::size_t) { return 0.0F; });
    REQUIRE(!silence.voiced);

    detector.reset();
    std::uint32_t state = 0x12345678U;
    PitchDetection noise;
    std::size_t voicedUpdates = 0;
    std::size_t updates = 0;
    for (std::size_t index = 0; index < 48'000; ++index)
    {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        const float sample = static_cast<float>(static_cast<double>(state) / 4294967295.0 - 0.5);
        noise = detector.processSample(sample);
        if (noise.updated)
        {
            ++updates;
            if (noise.voiced)
                ++voicedUpdates;
        }
    }
    REQUIRE(updates > 100);
    REQUIRE(voicedUpdates * 20 < updates);
    REQUIRE(!noise.voiced);
}

TEST_CASE("runtime F0 bounds stay inside prepared storage and alter candidate search")
{
    PitchDetector detector;
    detector.prepare(config());
    REQUIRE(detector.setFrequencyBounds(300.0, 500.0));
    const auto excluded = render(detector, 24'000, [](std::size_t index) {
        return static_cast<float>(0.7 * std::sin(2.0 * std::numbers::pi * 220.0
                                                * static_cast<double>(index) / 48'000.0));
    });
    REQUIRE(!excluded.voiced);
    REQUIRE(!detector.setFrequencyBounds(20.0, 500.0));

    detector.reset();
    REQUIRE(detector.setFrequencyBounds(100.0, 300.0));
    const auto included = render(detector, 24'000, [](std::size_t index) {
        return static_cast<float>(0.7 * std::sin(2.0 * std::numbers::pi * 220.0
                                                * static_cast<double>(index) / 48'000.0));
    });
    REQUIRE(included.voiced);
    REQUIRE_NEAR(included.frequency, 220.0, 1.0);
    REQUIRE(detector.setPeriodicityEpsilon(0.15));
    REQUIRE(!detector.setPeriodicityEpsilon(0.0));
}

int main()
{
    return test::runAll();
}
