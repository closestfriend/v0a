#include "TestHarness.h"
#include "app/AutoTuneRenderer.h"
#include "io/WavFile.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <sstream>
#include <string>
#include <vector>

using v0a::readMonoWav;
using v0a::runAutoTuneRenderer;
using v0a::writeMonoFloatWav;

namespace
{
std::filesystem::path tempFile(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

std::string readText(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}
} // namespace

TEST_CASE("automatic renderer writes a same-length WAV and detector diagnostics")
{
    const auto inputPath = tempFile("v0b-render-input.wav");
    const auto outputPath = tempFile("v0b-render-output.wav");
    const auto csvPath = tempFile("v0b-render-output.csv");
    std::vector<float> input(24'000);
    for (std::size_t index = 0; index < input.size(); ++index)
    {
        input[index] = static_cast<float>(0.7 * std::sin(
            2.0 * std::numbers::pi * 230.0 * static_cast<double>(index) / 48'000.0));
    }
    writeMonoFloatWav(inputPath, 48'000, input);

    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> arguments {
        "v0b_render", "--input", inputPath.string(), "--output", outputPath.string(),
        "--diagnostics", csvPath.string(), "--key", "C", "--scale", "chromatic",
        "--retune-ms", "0", "--tracking", "normal", "--interpolator", "linear"
    };
    REQUIRE(runAutoTuneRenderer(arguments, output, error) == 0);
    REQUIRE(error.str().empty());
    REQUIRE(readMonoWav(outputPath).mono.size() == input.size());
    REQUIRE(output.str().find("latency_samples=") != std::string::npos);
    REQUIRE(output.str().find("voiced_frames=0") == std::string::npos);

    const std::string csv = readText(csvPath);
    REQUIRE(csv.starts_with("sample,time_seconds,input_f0_hz,input_midi,period_samples,"));
    REQUIRE(csv.find(",58,233.081") != std::string::npos);

    std::filesystem::remove(inputPath);
    std::filesystem::remove(outputPath);
    std::filesystem::remove(csvPath);
}

TEST_CASE("automatic renderer rejects malformed note matrices")
{
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> arguments {
        "v0b_render", "--input", "in.wav", "--output", "out.wav",
        "--note-matrix", "TTB"
    };
    REQUIRE(runAutoTuneRenderer(arguments, output, error) != 0);
    REQUIRE(error.str().find("12 characters") != std::string::npos);
}

int main()
{
    return test::runAll();
}
