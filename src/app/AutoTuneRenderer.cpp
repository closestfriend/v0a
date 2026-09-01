#include "app/AutoTuneRenderer.h"

#include "dsp/PitchCorrectionEngine.h"
#include "dsp/PitchMath.h"
#include "io/WavFile.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
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
    int key = 0;
    ScalePreset scale = ScalePreset::Chromatic;
    std::optional<std::string> noteMatrix;
    double retuneMilliseconds = 0.0;
    double minimumF0 = 60.0;
    double maximumF0 = 800.0;
    double periodicityEpsilon = 0.30;
    InterpolationMethod interpolation = InterpolationMethod::Linear;
    std::size_t crossfadeSamples = 0;
};

[[nodiscard]] double parseNonnegativeDouble(const std::string& text, const char* option)
{
    std::size_t consumed = 0;
    const double value = std::stod(text, &consumed);
    if (consumed != text.size() || !std::isfinite(value) || value < 0.0)
        throw std::invalid_argument(std::string(option) + " must be finite and nonnegative");
    return value;
}

[[nodiscard]] int parseKey(const std::string& text)
{
    constexpr std::array<const char*, 12> names {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    const auto found = std::find(names.begin(), names.end(), text);
    if (found == names.end())
        throw std::invalid_argument("--key must be C through B using # for accidentals");
    return static_cast<int>(std::distance(names.begin(), found));
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

    const std::array<std::string, 12> allowed {
        "--input", "--output", "--diagnostics", "--key", "--scale", "--note-matrix",
        "--retune-ms", "--min-f0", "--max-f0", "--tracking", "--interpolator",
        "--crossfade-samples"
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
    if (const auto found = values.find("--diagnostics"); found != values.end())
        options.diagnostics = found->second;
    if (const auto found = values.find("--key"); found != values.end())
        options.key = parseKey(found->second);
    if (const auto found = values.find("--scale"); found != values.end())
    {
        if (found->second == "chromatic")
            options.scale = ScalePreset::Chromatic;
        else if (found->second == "major")
            options.scale = ScalePreset::Major;
        else if (found->second == "minor")
            options.scale = ScalePreset::NaturalMinor;
        else
            throw std::invalid_argument("--scale must be chromatic, major, or minor");
    }
    if (const auto found = values.find("--note-matrix"); found != values.end())
    {
        if (found->second.size() != 12)
            throw std::invalid_argument("--note-matrix must contain exactly 12 characters");
        if (found->second.find_first_not_of("TBR") != std::string::npos)
            throw std::invalid_argument("--note-matrix characters must be T, B, or R");
        options.noteMatrix = found->second;
    }
    if (const auto found = values.find("--retune-ms"); found != values.end())
        options.retuneMilliseconds = parseNonnegativeDouble(found->second, "--retune-ms");
    if (const auto found = values.find("--min-f0"); found != values.end())
        options.minimumF0 = parseNonnegativeDouble(found->second, "--min-f0");
    if (const auto found = values.find("--max-f0"); found != values.end())
        options.maximumF0 = parseNonnegativeDouble(found->second, "--max-f0");
    if (const auto found = values.find("--tracking"); found != values.end())
    {
        if (found->second == "normal")
            options.periodicityEpsilon = 0.30;
        else if (found->second == "choosy")
            options.periodicityEpsilon = 0.18;
        else
            throw std::invalid_argument("--tracking must be normal or choosy");
    }
    if (const auto found = values.find("--interpolator"); found != values.end())
    {
        if (found->second == "linear")
            options.interpolation = InterpolationMethod::Linear;
        else if (found->second == "hermite")
            options.interpolation = InterpolationMethod::CubicHermite;
        else if (found->second == "lagrange")
            options.interpolation = InterpolationMethod::Lagrange4;
        else
            throw std::invalid_argument("--interpolator must be linear, hermite, or lagrange");
    }
    if (const auto found = values.find("--crossfade-samples"); found != values.end())
    {
        const double parsed = parseNonnegativeDouble(found->second, "--crossfade-samples");
        if (std::floor(parsed) != parsed)
            throw std::invalid_argument("--crossfade-samples must be an integer");
        options.crossfadeSamples = static_cast<std::size_t>(parsed);
    }
    return options;
}

[[nodiscard]] const char* noteModeName(NoteMode mode) noexcept
{
    switch (mode)
    {
        case NoteMode::Target: return "Target";
        case NoteMode::Bypass: return "Bypass";
        case NoteMode::Remove: return "Remove";
    }
    return "Remove";
}
} // namespace

int runAutoTuneRenderer(const std::vector<std::string>& arguments,
                        std::ostream& output,
                        std::ostream& error)
{
    try
    {
        const Options options = parseOptions(arguments);
        const AudioBuffer input = readMonoWav(options.input);
        PitchCorrectionConfig config {
            .sampleRate = static_cast<double>(input.sampleRate),
            .minimumF0 = options.minimumF0,
            .maximumF0 = options.maximumF0,
            .periodicityEpsilon = options.periodicityEpsilon,
            .energyThreshold = 1.0e-5,
            .octaveImprovement = 0.02,
            .retuneMilliseconds = options.retuneMilliseconds,
            .interpolation = options.interpolation,
            .spliceCrossfadeSamples = options.crossfadeSamples,
        };
        PitchCorrectionEngine engine;
        engine.prepare(config);
        engine.loadScale(options.key, options.scale);
        if (options.noteMatrix.has_value())
        {
            for (int pitchClass = 0; pitchClass < 12; ++pitchClass)
            {
                const char state = options.noteMatrix.value()[static_cast<std::size_t>(pitchClass)];
                engine.setNoteMode(pitchClass, state == 'T' ? NoteMode::Target
                                              : state == 'B' ? NoteMode::Bypass
                                                             : NoteMode::Remove);
            }
        }

        std::ofstream diagnostics;
        if (options.diagnostics.has_value())
        {
            diagnostics.open(options.diagnostics.value(), std::ios::trunc);
            if (!diagnostics)
                throw std::runtime_error("cannot create diagnostics CSV");
            diagnostics << "sample,time_seconds,input_f0_hz,input_midi,period_samples,"
                           "periodicity,confidence,voiced,cell_midi,note_mode,target_midi,"
                           "target_f0_hz,raw_ratio,smoothed_ratio,cycle_repetitions,"
                           "cycle_deletions\n" << std::setprecision(12);
        }

        std::vector<float> rendered;
        rendered.reserve(input.mono.size());
        std::size_t voicedFrames = 0;
        for (std::size_t index = 0; index < input.mono.size(); ++index)
        {
            rendered.push_back(engine.processSample(input.mono[index]));
            const auto& state = engine.diagnostics();
            if (state.detection.voiced)
                ++voicedFrames;
            if (diagnostics)
            {
                diagnostics << index << ','
                            << static_cast<double>(index) / input.sampleRate << ','
                            << state.detection.frequency << ','
                            << (state.detection.voiced
                                  ? frequencyToMidi(state.detection.frequency) : 0.0) << ','
                            << state.detection.periodSamples << ','
                            << state.detection.periodicity << ',' << state.detection.confidence << ','
                            << (state.detection.voiced ? 1 : 0) << ','
                            << state.quantization.cellMidi << ','
                            << noteModeName(state.quantization.cellMode) << ','
                            << state.quantization.targetMidi << ','
                            << state.quantization.targetFrequency << ','
                            << state.rawRatio << ',' << state.smoothedRatio << ','
                            << state.resamplerStats.repeatedCycles << ','
                            << state.resamplerStats.deletedCycles << '\n';
            }
        }
        writeMonoFloatWav(options.output, input.sampleRate, rendered);
        const auto& state = engine.diagnostics();
        output << "frames=" << rendered.size()
               << " voiced_frames=" << voicedFrames
               << " latency_samples=" << engine.latencySamples()
               << " repeated_cycles=" << state.resamplerStats.repeatedCycles
               << " deleted_cycles=" << state.resamplerStats.deletedCycles << '\n';
        return 0;
    }
    catch (const std::exception& exception)
    {
        error << "v0b_render: " << exception.what() << '\n';
        return 1;
    }
}
} // namespace v0a
