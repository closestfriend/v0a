#include "app/OfflineRenderer.h"

#include "dsp/CycleResampler.h"
#include "io/WavFile.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace v0a
{
namespace
{
struct Options
{
    std::filesystem::path input;
    std::filesystem::path output;
    std::optional<std::filesystem::path> diagnostics;
    std::optional<double> period;
    std::optional<double> f0;
    double ratio = 0.0;
    InterpolationMethod interpolation = InterpolationMethod::Linear;
    std::size_t crossfadeSamples = 0;
    std::optional<double> crossfadeFraction;
};

[[nodiscard]] double parsePositiveDouble(const std::string& text, const char* option)
{
    std::size_t consumed = 0;
    const double value = std::stod(text, &consumed);
    if (consumed != text.size() || !std::isfinite(value) || value <= 0.0)
        throw std::invalid_argument(std::string(option) + " must be finite and positive");
    return value;
}

[[nodiscard]] std::size_t parseNonnegativeSize(const std::string& text, const char* option)
{
    std::size_t consumed = 0;
    const auto value = std::stoull(text, &consumed);
    if (consumed != text.size() || value > std::numeric_limits<std::size_t>::max())
        throw std::invalid_argument(std::string(option) + " must be a nonnegative integer");
    return static_cast<std::size_t>(value);
}

[[nodiscard]] Options parseOptions(const std::vector<std::string>& arguments)
{
    std::unordered_map<std::string, std::string> values;
    for (std::size_t index = 1; index < arguments.size(); index += 2)
    {
        if (index + 1 >= arguments.size() || !arguments[index].starts_with("--"))
            throw std::invalid_argument("arguments must be --name value pairs");
        if (!values.emplace(arguments[index], arguments[index + 1]).second)
            throw std::invalid_argument("duplicate option: " + arguments[index]);
    }

    const auto required = [&values](const char* name) -> const std::string& {
        const auto found = values.find(name);
        if (found == values.end())
            throw std::invalid_argument(std::string("missing required option: ") + name);
        return found->second;
    };

    const std::vector<std::string> allowed {
        "--input", "--output", "--diagnostics", "--period", "--f0", "--ratio",
        "--interpolator", "--crossfade-samples", "--crossfade-period-fraction"
    };
    for (const auto& [name, ignored] : values)
    {
        static_cast<void>(ignored);
        if (std::find(allowed.begin(), allowed.end(), name) == allowed.end())
            throw std::invalid_argument("unknown option: " + name);
    }

    Options options;
    options.input = required("--input");
    options.output = required("--output");
    options.ratio = parsePositiveDouble(required("--ratio"), "--ratio");

    if (const auto found = values.find("--period"); found != values.end())
        options.period = parsePositiveDouble(found->second, "--period");
    if (const auto found = values.find("--f0"); found != values.end())
        options.f0 = parsePositiveDouble(found->second, "--f0");
    if (options.period.has_value() == options.f0.has_value())
        throw std::invalid_argument("provide exactly one of --period or --f0");

    const auto interpolation = required("--interpolator");
    if (interpolation == "linear")
        options.interpolation = InterpolationMethod::Linear;
    else if (interpolation == "hermite")
        options.interpolation = InterpolationMethod::CubicHermite;
    else if (interpolation == "lagrange")
        options.interpolation = InterpolationMethod::Lagrange4;
    else
        throw std::invalid_argument("--interpolator must be linear, hermite, or lagrange");

    if (const auto found = values.find("--crossfade-samples"); found != values.end())
        options.crossfadeSamples = parseNonnegativeSize(found->second, "--crossfade-samples");
    if (const auto found = values.find("--crossfade-period-fraction"); found != values.end())
        options.crossfadeFraction = parsePositiveDouble(found->second, "--crossfade-period-fraction");
    if (options.crossfadeSamples != 0 && options.crossfadeFraction.has_value())
        throw std::invalid_argument("crossfade samples and period fraction are mutually exclusive");
    if (const auto found = values.find("--diagnostics"); found != values.end())
        options.diagnostics = found->second;
    return options;
}

[[nodiscard]] std::size_t bufferCapacityFor(double period, double safetyDelay)
{
    const auto required = static_cast<std::size_t>(std::ceil(safetyDelay + period + 8.0));
    std::size_t capacity = 1024;
    while (capacity <= required)
        capacity *= 2;
    return capacity;
}
} // namespace

int runOfflineRenderer(const std::vector<std::string>& arguments,
                       std::ostream& output,
                       std::ostream& error)
{
    try
    {
        const Options options = parseOptions(arguments);
        const AudioBuffer input = readMonoWav(options.input);
        const double period = options.period.value_or(
            static_cast<double>(input.sampleRate) / options.f0.value_or(1.0));
        if (period < 4.0)
            throw std::invalid_argument("manual period must be at least 4 samples");
        const double inputF0 = static_cast<double>(input.sampleRate) / period;

        std::size_t crossfadeSamples = options.crossfadeSamples;
        if (options.crossfadeFraction.has_value())
        {
            crossfadeSamples = static_cast<std::size_t>(
                std::llround(period * options.crossfadeFraction.value()));
        }
        if (static_cast<double>(crossfadeSamples) >= period)
            throw std::invalid_argument("crossfade must be shorter than one period");

        const double safetyDelay = std::max(2.0 * period + 8.0,
                                            period + static_cast<double>(crossfadeSamples) + 8.0);
        CycleResamplerConfig config {
            .periodSamples = period,
            .rate = options.ratio,
            .bufferCapacity = bufferCapacityFor(period, safetyDelay),
            .safetyDelaySamples = safetyDelay,
            .interpolation = options.interpolation,
            .spliceCrossfadeSamples = crossfadeSamples,
        };
        CycleResampler resampler(config);
        std::vector<float> rendered;
        rendered.reserve(input.mono.size());

        std::ofstream diagnostics;
        if (options.diagnostics.has_value())
        {
            diagnostics.open(options.diagnostics.value(), std::ios::trunc);
            if (!diagnostics)
                throw std::runtime_error("cannot create diagnostics CSV");
            diagnostics << "time_seconds,input_f0_hz,input_pitch_cents,period_samples,"
                           "periodicity,confidence,voiced,note_region,note_mode,target_midi,"
                           "target_f0_hz,raw_ratio,smoothed_ratio,output_f0_estimate,"
                           "cycle_repetitions,cycle_deletions\n";
            diagnostics << std::setprecision(12);
        }

        const double inputCents = 1200.0 * std::log2(inputF0 / 440.0);
        const double outputF0 = inputF0 * options.ratio;
        for (std::size_t index = 0; index < input.mono.size(); ++index)
        {
            rendered.push_back(resampler.processSample(input.mono[index]));
            if (diagnostics)
            {
                const auto& stats = resampler.stats();
                diagnostics << static_cast<double>(index) / input.sampleRate << ','
                            << inputF0 << ',' << inputCents << ',' << period << ",,,1,,,,"
                            << outputF0 << ',' << options.ratio << ',' << options.ratio << ','
                            << outputF0 << ',' << stats.repeatedCycles << ','
                            << stats.deletedCycles << '\n';
            }
        }

        writeMonoFloatWav(options.output, input.sampleRate, rendered);
        output << "frames=" << rendered.size()
               << " repeated_cycles=" << resampler.stats().repeatedCycles
               << " deleted_cycles=" << resampler.stats().deletedCycles << '\n';
        return 0;
    }
    catch (const std::exception& exception)
    {
        error << "v0a_render: " << exception.what() << '\n';
        return 1;
    }
}
} // namespace v0a
