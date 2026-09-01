#include "TestHarness.h"
#include "dsp/PitchCorrectionEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

using v0a::PitchCorrectionConfig;
using v0a::PitchCorrectionEngine;

namespace
{
PitchCorrectionConfig config()
{
    PitchCorrectionConfig result;
    result.sampleRate = 48'000.0;
    result.minimumF0 = 60.0;
    result.maximumF0 = 800.0;
    result.retuneMilliseconds = 0.0;
    result.spliceCrossfadeSamples = 0;
    return result;
}

double robustCycleRangeCents(const std::vector<float>& samples, std::size_t begin)
{
    std::vector<double> crossings;
    std::vector<double> frequencies;
    for (std::size_t index = std::max<std::size_t>(begin, 1); index < samples.size(); ++index)
    {
        if (samples[index - 1] <= 0.0F && samples[index] > 0.0F)
        {
            const double left = samples[index - 1];
            const double right = samples[index];
            crossings.push_back(static_cast<double>(index - 1) - left / (right - left));
        }
    }
    for (std::size_t index = 1; index < crossings.size(); ++index)
    {
        const double frequency = 48'000.0 / (crossings[index] - crossings[index - 1]);
        if (frequency > 150.0 && frequency < 300.0)
            frequencies.push_back(frequency);
    }
    std::sort(frequencies.begin(), frequencies.end());
    const double low = frequencies[frequencies.size() / 10];
    const double high = frequencies[(frequencies.size() * 9) / 10];
    return 1200.0 * std::log2(high / low);
}
} // namespace

TEST_CASE("shared engine detects quantizes and drives the verified resampler")
{
    PitchCorrectionEngine engine;
    engine.prepare(config());
    constexpr double inputF0 = 230.0;
    for (std::size_t index = 0; index < 96'000; ++index)
    {
        const float input = static_cast<float>(0.7 * std::sin(
            2.0 * std::numbers::pi * inputF0 * static_cast<double>(index) / 48'000.0));
        REQUIRE(std::isfinite(engine.processSample(input)));
    }

    const auto& diagnostics = engine.diagnostics();
    REQUIRE(diagnostics.detection.voiced);
    REQUIRE_NEAR(diagnostics.detection.frequency, inputF0, 1.0);
    REQUIRE(diagnostics.quantization.targetMidi == 58);
    REQUIRE_NEAR(diagnostics.quantization.targetFrequency, 233.081880759, 1.0e-6);
    REQUIRE_NEAR(diagnostics.rawRatio, 233.081880759 / inputF0, 0.006);
    REQUIRE_NEAR(diagnostics.smoothedRatio, diagnostics.rawRatio, 1.0e-12);
    REQUIRE(diagnostics.resamplerStats.repeatedCycles > 0);
    REQUIRE(diagnostics.resamplerStats.deletedCycles == 0);
    REQUIRE(std::abs(diagnostics.resamplerStats.residualSeparation)
            < diagnostics.detection.periodSamples);
}

TEST_CASE("unvoiced transition returns correction to unity")
{
    PitchCorrectionEngine engine;
    engine.prepare(config());
    for (std::size_t index = 0; index < 48'000; ++index)
    {
        const float input = static_cast<float>(0.7 * std::sin(
            2.0 * std::numbers::pi * 230.0 * static_cast<double>(index) / 48'000.0));
        static_cast<void>(engine.processSample(input));
    }
    REQUIRE(engine.diagnostics().detection.voiced);

    std::uint32_t state = 0x8badf00dU;
    for (std::size_t index = 0; index < 48'000; ++index)
    {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        const float noise = static_cast<float>(static_cast<double>(state) / 4294967295.0 - 0.5);
        static_cast<void>(engine.processSample(noise));
    }
    REQUIRE(!engine.diagnostics().detection.voiced);
    REQUIRE_NEAR(engine.diagnostics().rawRatio, 1.0, 1.0e-12);
    REQUIRE_NEAR(engine.diagnostics().smoothedRatio, 1.0, 1.0e-12);
}

TEST_CASE("reported engine latency equals the unity-path sample delay")
{
    PitchCorrectionEngine engine;
    engine.prepare(config());
    const std::size_t latency = engine.latencySamples();
    REQUIRE(latency > 0);

    std::size_t impulseIndex = 0;
    for (std::size_t index = 0; index <= latency + 2; ++index)
    {
        const float output = engine.processSample(index == 0 ? 1.0F : 0.0F);
        if (std::abs(output) > 0.5F)
            impulseIndex = index;
    }
    REQUIRE(impulseIndex == latency);
}

TEST_CASE("correction control is aligned to the delayed waveform and flattens vibrato")
{
    PitchCorrectionEngine engine;
    engine.prepare(config());
    constexpr std::size_t frames = 3 * 48'000;
    std::vector<float> input;
    std::vector<float> output;
    input.reserve(frames);
    output.reserve(frames);
    double phase = 0.0;
    for (std::size_t index = 0; index < frames; ++index)
    {
        const double time = static_cast<double>(index) / 48'000.0;
        const double midi = 57.0 + 0.4 * std::sin(2.0 * std::numbers::pi * 5.0 * time);
        const double frequency = 440.0 * std::exp2((midi - 69.0) / 12.0);
        phase += frequency / 48'000.0;
        phase -= std::floor(phase);
        input.push_back(static_cast<float>(0.7 * std::sin(2.0 * std::numbers::pi * phase)));
        output.push_back(engine.processSample(input.back()));
    }

    const double inputRange = robustCycleRangeCents(input, 48'000);
    const double outputRange = robustCycleRangeCents(output, 48'000);
    REQUIRE(inputRange > 65.0);
    REQUIRE(outputRange < 45.0);
    REQUIRE(outputRange < 0.55 * inputRange);
}

int main()
{
    return test::runAll();
}
