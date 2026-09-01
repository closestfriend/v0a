#include "app/SignalGenerator.h"

#include "io/WavFile.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace v0a
{
namespace
{
constexpr std::uint32_t sampleRate = 48'000;
constexpr std::size_t frameCount = 48'000;
constexpr double fundamental = 220.0;

[[nodiscard]] std::vector<float> sineSignal()
{
    std::vector<float> samples(frameCount);
    for (std::size_t index = 0; index < samples.size(); ++index)
    {
        const double phase = 2.0 * std::numbers::pi * fundamental
                           * static_cast<double>(index) / sampleRate;
        samples[index] = static_cast<float>(0.6 * std::sin(phase));
    }
    return samples;
}

[[nodiscard]] std::vector<float> sawSignal()
{
    std::vector<float> samples(frameCount, 0.0F);
    const int harmonicCount = static_cast<int>((sampleRate * 0.5) / fundamental);
    for (int harmonic = 1; harmonic <= harmonicCount; ++harmonic)
    {
        const double amplitude = 1.0 / harmonic;
        for (std::size_t index = 0; index < samples.size(); ++index)
        {
            const double phase = 2.0 * std::numbers::pi * fundamental * harmonic
                               * static_cast<double>(index) / sampleRate;
            samples[index] += static_cast<float>(amplitude * std::sin(phase));
        }
    }
    const auto peak = std::max_element(samples.begin(), samples.end(), [](float left, float right) {
        return std::abs(left) < std::abs(right);
    });
    const float scale = peak == samples.end() ? 0.0F : 0.7F / std::abs(*peak);
    for (float& sample : samples)
        sample *= scale;
    return samples;
}

[[nodiscard]] std::vector<float> vowelSignal()
{
    std::vector<float> samples(frameCount, 0.0F);
    constexpr double formant1 = 730.0;
    constexpr double formant2 = 1090.0;
    constexpr double formant3 = 2440.0;
    constexpr double bandwidth1 = 90.0;
    constexpr double bandwidth2 = 120.0;
    constexpr double bandwidth3 = 180.0;
    const auto gaussian = [](double frequency, double center, double bandwidth) {
        const double normalized = (frequency - center) / bandwidth;
        return std::exp(-0.5 * normalized * normalized);
    };

    const int harmonicCount = static_cast<int>((sampleRate * 0.5) / fundamental);
    for (int harmonic = 1; harmonic <= harmonicCount; ++harmonic)
    {
        const double frequency = fundamental * harmonic;
        const double envelope = 0.2 / harmonic
                              + gaussian(frequency, formant1, bandwidth1)
                              + 0.8 * gaussian(frequency, formant2, bandwidth2)
                              + 0.5 * gaussian(frequency, formant3, bandwidth3);
        for (std::size_t index = 0; index < samples.size(); ++index)
        {
            const double phase = 2.0 * std::numbers::pi * frequency
                               * static_cast<double>(index) / sampleRate;
            samples[index] += static_cast<float>(envelope * std::sin(phase));
        }
    }
    const auto peak = std::max_element(samples.begin(), samples.end(), [](float left, float right) {
        return std::abs(left) < std::abs(right);
    });
    const float scale = peak == samples.end() ? 0.0F : 0.7F / std::abs(*peak);
    for (float& sample : samples)
        sample *= scale;
    return samples;
}
} // namespace

int runSignalGenerator(const std::vector<std::string>& arguments,
                       std::ostream& output,
                       std::ostream& error)
{
    try
    {
        if (arguments.size() != 3 || arguments[1] != "--output-dir")
            throw std::invalid_argument("usage: v0a_generate_signals --output-dir <directory>");
        const std::filesystem::path directory = arguments[2];
        std::filesystem::create_directories(directory);

        writeMonoFloatWav(directory / "sine_220hz.wav", sampleRate, sineSignal());
        writeMonoFloatWav(directory / "saw_220hz.wav", sampleRate, sawSignal());
        writeMonoFloatWav(directory / "vowel_a_220hz.wav", sampleRate, vowelSignal());

        std::ofstream manifest(directory / "manifest.txt", std::ios::trunc);
        if (!manifest)
            throw std::runtime_error("cannot create signal manifest");
        manifest << "sample_rate=48000\n"
                    "frames=48000\n"
                    "fundamental_hz=220\n"
                    "phase=0\n"
                    "format=mono_float32_wav\n"
                    "sine_peak=0.6\n"
                    "saw_peak=0.7\n"
                    "vowel_peak=0.7\n";
        output << "generated 3 deterministic signals in " << directory.string() << '\n';
        return 0;
    }
    catch (const std::exception& exception)
    {
        error << "v0a_generate_signals: " << exception.what() << '\n';
        return 1;
    }
}
} // namespace v0a
