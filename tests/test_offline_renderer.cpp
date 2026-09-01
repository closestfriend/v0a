#include "TestHarness.h"
#include "app/OfflineRenderer.h"
#include "io/WavFile.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using v0a::readMonoWav;
using v0a::runOfflineRenderer;
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

std::uint64_t summaryValue(const std::string& summary, const std::string& key)
{
    const auto begin = summary.find(key);
    if (begin == std::string::npos)
        return 0;
    return std::stoull(summary.substr(begin + key.size()));
}
} // namespace

TEST_CASE("renderer rejects simultaneous manual period and F0")
{
    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> arguments {
        "v0a_render", "--input", "in.wav", "--output", "out.wav",
        "--period", "100", "--f0", "480", "--ratio", "1.0",
        "--interpolator", "linear"
    };
    REQUIRE(runOfflineRenderer(arguments, output, error) != 0);
    REQUIRE(error.str().find("exactly one") != std::string::npos);
}

TEST_CASE("renderer preserves frame count and writes repeat diagnostics")
{
    const auto inputPath = tempFile("v0a-render-input.wav");
    const auto outputPath = tempFile("v0a-render-output.wav");
    const auto csvPath = tempFile("v0a-render-output.csv");
    std::vector<float> input(8'000);
    for (std::size_t index = 0; index < input.size(); ++index)
        input[index] = static_cast<float>(0.5 * std::sin(index * 0.08));
    writeMonoFloatWav(inputPath, 48'000, input);

    std::ostringstream output;
    std::ostringstream error;
    const std::vector<std::string> arguments {
        "v0a_render", "--input", inputPath.string(), "--output", outputPath.string(),
        "--f0", "480", "--ratio", "1.05", "--interpolator", "hermite",
        "--crossfade-samples", "4", "--diagnostics", csvPath.string()
    };

    REQUIRE(runOfflineRenderer(arguments, output, error) == 0);
    REQUIRE(error.str().empty());
    const auto rendered = readMonoWav(outputPath);
    REQUIRE(rendered.sampleRate == 48'000);
    REQUIRE(rendered.mono.size() == input.size());
    REQUIRE(summaryValue(output.str(), "repeated_cycles=") > 0);
    REQUIRE(output.str().find("deleted_cycles=0") != std::string::npos);
    const auto csv = readText(csvPath);
    REQUIRE(csv.find("time_seconds,input_f0_hz") == 0);

    std::filesystem::remove(inputPath);
    std::filesystem::remove(outputPath);
    std::filesystem::remove(csvPath);
}

int main()
{
    return test::runAll();
}
