#include "app/V0BAcceptance.h"

#include "dsp/NoteQuantizer.h"
#include "dsp/PitchCorrectionEngine.h"
#include "dsp/PitchMath.h"
#include "io/WavFile.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace v0a
{
namespace
{
constexpr std::uint32_t sampleRate = 48'000;

struct CheckRunner
{
    std::ostream& output;
    std::ostream& error;
    int failures = 0;

    void check(bool condition, const std::string& name, const std::string& detail)
    {
        if (condition)
            output << "PASS " << name << " (" << detail << ")\n";
        else
        {
            error << "FAIL " << name << " (" << detail << ")\n";
            ++failures;
        }
    }
};

[[nodiscard]] PitchCorrectionConfig defaultConfig()
{
    PitchCorrectionConfig config;
    config.sampleRate = sampleRate;
    config.minimumF0 = 60.0;
    config.maximumF0 = 800.0;
    config.periodicityEpsilon = 0.30;
    config.retuneMilliseconds = 0.0;
    return config;
}

[[nodiscard]] std::string number(double value, int precision = 3)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

[[nodiscard]] double crossingFrequency(const std::vector<float>& samples,
                                       std::size_t begin,
                                       std::size_t end)
{
    begin = std::max<std::size_t>(begin, 1);
    end = std::min(end, samples.size());
    std::vector<double> crossings;
    for (std::size_t index = begin; index < end; ++index)
    {
        if (samples[index - 1] <= 0.0F && samples[index] > 0.0F)
        {
            const double left = samples[index - 1];
            const double right = samples[index];
            const double fraction = -left / (right - left);
            crossings.push_back(static_cast<double>(index - 1) + fraction);
        }
    }
    if (crossings.size() < 2)
        return 0.0;
    return static_cast<double>(sampleRate)
         * static_cast<double>(crossings.size() - 1) / (crossings.back() - crossings.front());
}

[[nodiscard]] std::vector<double> localCycleFrequencies(const std::vector<float>& samples,
                                                        std::size_t begin,
                                                        std::size_t end)
{
    std::vector<double> crossings;
    std::vector<double> frequencies;
    begin = std::max<std::size_t>(begin, 1);
    end = std::min(end, samples.size());
    for (std::size_t index = begin; index < end; ++index)
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
        const double frequency = sampleRate / (crossings[index] - crossings[index - 1]);
        if (frequency > 150.0 && frequency < 300.0)
            frequencies.push_back(frequency);
    }
    return frequencies;
}

[[nodiscard]] double robustCentsRange(std::vector<double> frequencies)
{
    if (frequencies.size() < 10)
        return 1.0e9;
    std::sort(frequencies.begin(), frequencies.end());
    const double low = frequencies[frequencies.size() / 10];
    const double high = frequencies[(frequencies.size() * 9) / 10];
    return 1200.0 * std::log2(high / low);
}

void runSineGliss(const std::filesystem::path& directory, CheckRunner& checks)
{
    constexpr double startMidi = 57.0;
    constexpr double endMidi = 61.0;
    const std::size_t frames = 4 * sampleRate;
    PitchCorrectionEngine engine;
    engine.prepare(defaultConfig());
    std::vector<float> input(frames);
    std::vector<float> rendered;
    rendered.reserve(frames);
    std::ofstream csv(directory / "sine_gliss.csv", std::ios::trunc);
    csv << "sample,actual_midi,detected_f0,target_midi,raw_ratio,smoothed_ratio\n"
        << std::setprecision(10);

    double phase = 0.0;
    double totalDetectorCents = 0.0;
    double maximumDetectorCents = 0.0;
    double maximumImmediateError = 0.0;
    std::size_t detectorUpdates = 0;
    int previousTarget = -1000;
    std::vector<double> transitionErrors;
    for (std::size_t index = 0; index < frames; ++index)
    {
        const double fraction = static_cast<double>(index) / static_cast<double>(frames - 1);
        const double actualMidi = startMidi + fraction * (endMidi - startMidi);
        const double actualF0 = midiToFrequency(actualMidi);
        phase += actualF0 / sampleRate;
        phase -= std::floor(phase);
        input[index] = static_cast<float>(0.7 * std::sin(2.0 * std::numbers::pi * phase));
        rendered.push_back(engine.processSample(input[index]));
        const auto& state = engine.diagnostics();
        maximumImmediateError = std::max(maximumImmediateError,
                                         std::abs(state.rawRatio - state.smoothedRatio));
        if (state.detection.updated && state.detection.voiced)
        {
            const double detectorCents = std::abs(
                1200.0 * std::log2(state.detection.frequency / actualF0));
            totalDetectorCents += detectorCents;
            maximumDetectorCents = std::max(maximumDetectorCents, detectorCents);
            ++detectorUpdates;
            const int target = state.quantization.targetMidi;
            if (previousTarget != -1000 && target != previousTarget)
            {
                const double boundary = 0.5 * static_cast<double>(previousTarget + target);
                transitionErrors.push_back(std::abs(actualMidi - boundary) * 100.0);
            }
            previousTarget = target;
            csv << index << ',' << actualMidi << ',' << state.detection.frequency << ','
                << target << ',' << state.rawRatio << ',' << state.smoothedRatio << '\n';
        }
    }

    const double meanDetectorCents = detectorUpdates > 0
        ? totalDetectorCents / static_cast<double>(detectorUpdates) : 1.0e9;
    const double maximumTransitionError = transitionErrors.empty() ? 1.0e9
        : *std::max_element(transitionErrors.begin(), transitionErrors.end());
    checks.check(detectorUpdates > 100 && meanDetectorCents < 8.0 && maximumDetectorCents < 20.0,
                 "sine gliss detector follows input F0",
                 "mean=" + number(meanDetectorCents) + "c max=" + number(maximumDetectorCents) + "c");
    checks.check(transitionErrors.size() >= 4 && maximumTransitionError < 20.0,
                 "sine gliss targets change near chromatic midpoints",
                 "transitions=" + std::to_string(transitionErrors.size())
                     + " max=" + number(maximumTransitionError) + "c");
    checks.check(maximumImmediateError < 1.0e-12,
                 "Retune 0 follows target immediately",
                 "max ratio error=" + number(maximumImmediateError, 12));

    double maximumStaircaseError = 0.0;
    for (int second = 1; second <= 3; ++second)
    {
        const std::size_t center = static_cast<std::size_t>(second) * sampleRate;
        const double measured = crossingFrequency(rendered, center - 4'800, center + 4'800);
        const double expected = midiToFrequency(57.0 + second);
        maximumStaircaseError = std::max(maximumStaircaseError,
                                         std::abs(1200.0 * std::log2(measured / expected)));
    }
    checks.check(maximumStaircaseError < 20.0,
                 "sine gliss output forms the expected pitch staircase",
                 "max center error=" + number(maximumStaircaseError) + "c");
    writeMonoFloatWav(directory / "sine_gliss_input.wav", sampleRate, input);
    writeMonoFloatWav(directory / "sine_gliss_output.wav", sampleRate, rendered);
}

void runSawGliss(const std::filesystem::path& directory, CheckRunner& checks)
{
    constexpr double startMidi = 45.0;
    constexpr double endMidi = 50.0;
    const std::size_t frames = 10 * sampleRate;
    PitchCorrectionEngine engine;
    engine.prepare(defaultConfig());
    std::vector<float> input(frames);
    std::vector<float> rendered;
    rendered.reserve(frames);
    std::ofstream csv(directory / "saw_gliss.csv", std::ios::trunc);
    csv << "sample,actual_f0,detected_f0,periodicity,integer_lag,doubled,target_midi,raw_ratio\n"
        << std::setprecision(10);
    double phase = 0.0;
    std::size_t voicedUpdates = 0;
    std::size_t octaveErrors = 0;
    std::size_t directionErrors = 0;
    double maximumResidualFraction = 0.0;
    for (std::size_t index = 0; index < frames; ++index)
    {
        const double fraction = static_cast<double>(index) / static_cast<double>(frames - 1);
        const double actualF0 = midiToFrequency(startMidi + fraction * (endMidi - startMidi));
        phase += actualF0 / sampleRate;
        phase -= std::floor(phase);
        input[index] = static_cast<float>(0.55 * (2.0 * phase - 1.0));
        rendered.push_back(engine.processSample(input[index]));
        const auto& state = engine.diagnostics();
        if (state.detection.updated && state.detection.voiced)
        {
            ++voicedUpdates;
            const double octaveRatio = state.detection.frequency / actualF0;
            if (octaveRatio < 0.75 || octaveRatio > 1.35)
                ++octaveErrors;
            const double frequencyDelta = state.quantization.targetFrequency
                                        - state.detection.frequency;
            if ((frequencyDelta > 0.01 && state.rawRatio <= 1.0)
                || (frequencyDelta < -0.01 && state.rawRatio >= 1.0))
                ++directionErrors;
            csv << index << ',' << actualF0 << ',' << state.detection.frequency << ','
                << state.detection.periodicity << ',' << state.detection.integerLag << ','
                << (state.detection.usedDoubledLag ? 1 : 0) << ','
                << state.quantization.targetMidi << ',' << state.rawRatio << '\n';
        }
        if (state.detection.periodSamples > 0.0)
        {
            maximumResidualFraction = std::max(
                maximumResidualFraction,
                std::abs(state.resamplerStats.residualSeparation)
                    / state.detection.periodSamples);
        }
    }
    const auto& stats = engine.diagnostics().resamplerStats;
    const double octaveErrorRate = voicedUpdates > 0
        ? static_cast<double>(octaveErrors) / static_cast<double>(voicedUpdates) : 1.0;
    checks.check(voicedUpdates > 100 && octaveErrorRate < 0.02,
                 "saw gliss avoids systematic octave mistracking",
                 "voiced updates=" + std::to_string(voicedUpdates)
                     + " octave error=" + number(100.0 * octaveErrorRate) + "%");
    checks.check(directionErrors == 0,
                 "saw gliss correction direction is correct",
                 "direction errors=" + std::to_string(directionErrors));
    checks.check(stats.repeatedCycles > 0 && stats.deletedCycles > 0
                     && maximumResidualFraction < 1.05,
                 "saw gliss cycle behavior remains bounded",
                 "repeat=" + std::to_string(stats.repeatedCycles)
                     + " delete=" + std::to_string(stats.deletedCycles)
                     + " residual=" + number(maximumResidualFraction) + "P");
    writeMonoFloatWav(directory / "saw_gliss_input.wav", sampleRate, input);
    writeMonoFloatWav(directory / "saw_gliss_output.wav", sampleRate, rendered);
}

void runVibrato(const std::filesystem::path& directory, CheckRunner& checks)
{
    const std::size_t frames = 3 * sampleRate;
    PitchCorrectionEngine engine;
    engine.prepare(defaultConfig());
    std::vector<float> input(frames);
    std::vector<float> rendered;
    rendered.reserve(frames);
    double phase = 0.0;
    for (std::size_t index = 0; index < frames; ++index)
    {
        const double time = static_cast<double>(index) / sampleRate;
        const double midi = 57.0 + 0.40 * std::sin(2.0 * std::numbers::pi * 5.0 * time);
        phase += midiToFrequency(midi) / sampleRate;
        phase -= std::floor(phase);
        input[index] = static_cast<float>(0.7 * std::sin(2.0 * std::numbers::pi * phase));
        rendered.push_back(engine.processSample(input[index]));
    }
    const auto begin = static_cast<std::size_t>(1.0 * sampleRate);
    const double inputRange = robustCentsRange(localCycleFrequencies(input, begin, frames));
    const double outputRange = robustCentsRange(localCycleFrequencies(rendered, begin, frames));
    checks.check(outputRange < inputRange * 0.55 && outputRange < 45.0,
                 "Retune 0 strongly flattens vibrato",
                 "input=" + number(inputRange) + "c output=" + number(outputRange) + "c");
    writeMonoFloatWav(directory / "vibrato_input.wav", sampleRate, input);
    writeMonoFloatWav(directory / "vibrato_output.wav", sampleRate, rendered);
}

void runVoicedUnvoiced(const std::filesystem::path& directory, CheckRunner& checks)
{
    const std::size_t frames = 2 * sampleRate;
    PitchCorrectionEngine engine;
    engine.prepare(defaultConfig());
    std::vector<float> input(frames);
    std::vector<float> rendered;
    rendered.reserve(frames);
    std::uint32_t noiseState = 0x31415926U;
    std::size_t noiseUpdates = 0;
    std::size_t unvoicedUpdates = 0;
    std::size_t nonUnityUnvoiced = 0;
    for (std::size_t index = 0; index < frames; ++index)
    {
        float sample = 0.0F;
        if (index < sampleRate)
        {
            sample = static_cast<float>(0.7 * std::sin(
                2.0 * std::numbers::pi * 230.0 * static_cast<double>(index) / sampleRate));
        }
        else
        {
            noiseState ^= noiseState << 13U;
            noiseState ^= noiseState >> 17U;
            noiseState ^= noiseState << 5U;
            sample = static_cast<float>(static_cast<double>(noiseState) / 4294967295.0 - 0.5);
        }
        input[index] = sample;
        rendered.push_back(engine.processSample(sample));
        const auto& state = engine.diagnostics();
        if (index > sampleRate + sampleRate / 5 && state.detection.updated)
        {
            ++noiseUpdates;
            if (!state.detection.voiced)
            {
                ++unvoicedUpdates;
                if (state.rawRatio != 1.0 || state.smoothedRatio != 1.0)
                    ++nonUnityUnvoiced;
            }
        }
    }
    const double unvoicedRate = noiseUpdates > 0
        ? static_cast<double>(unvoicedUpdates) / static_cast<double>(noiseUpdates) : 0.0;
    checks.check(noiseUpdates > 100 && unvoicedRate > 0.95 && nonUnityUnvoiced == 0,
                 "unvoiced material returns to unity",
                 "unvoiced=" + number(100.0 * unvoicedRate)
                     + "% non-unity=" + std::to_string(nonUnityUnvoiced));
    writeMonoFloatWav(directory / "tone_noise_input.wav", sampleRate, input);
    writeMonoFloatWav(directory / "tone_noise_output.wav", sampleRate, rendered);
}

void runBoundary(CheckRunner& checks)
{
    NoteMatrix matrix;
    NoteQuantizer quantizer;
    const auto below = quantizer.quantize(60.49, matrix);
    const auto above = quantizer.quantize(60.51, matrix);
    checks.check(below.targetMidi == 60 && above.targetMidi == 61,
                 "49/51-cent memoryless boundary",
                 "targets=" + std::to_string(below.targetMidi) + "/"
                     + std::to_string(above.targetMidi));
}
} // namespace

int runV0BAcceptance(const std::vector<std::string>& arguments,
                     std::ostream& output,
                     std::ostream& error)
{
    try
    {
        if (arguments.size() != 3 || arguments[1] != "--output-dir")
            throw std::invalid_argument("usage: v0b_acceptance --output-dir <directory>");
        const std::filesystem::path directory = arguments[2];
        std::filesystem::create_directories(directory);
        CheckRunner checks { output, error };
        runSineGliss(directory, checks);
        runSawGliss(directory, checks);
        runVibrato(directory, checks);
        runVoicedUnvoiced(directory, checks);
        runBoundary(checks);
        output << "V0B acceptance: " << (checks.failures == 0 ? "PASS" : "FAIL") << '\n';
        return checks.failures == 0 ? 0 : 1;
    }
    catch (const std::exception& exception)
    {
        error << "v0b_acceptance: " << exception.what() << '\n';
        return 1;
    }
}
} // namespace v0a
