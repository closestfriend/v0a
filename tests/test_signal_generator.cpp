#include "TestHarness.h"
#include "app/SignalGenerator.h"
#include "io/WavFile.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using v0a::readMonoWav;
using v0a::runSignalGenerator;

namespace
{
std::vector<char> fileBytes(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}
} // namespace

TEST_CASE("signal generator produces byte-identical bounded fixtures on repeated runs")
{
    const auto base = std::filesystem::temp_directory_path();
    const auto firstDir = base / "v0a-signals-first";
    const auto secondDir = base / "v0a-signals-second";
    std::filesystem::remove_all(firstDir);
    std::filesystem::remove_all(secondDir);

    std::ostringstream output;
    std::ostringstream error;
    REQUIRE(runSignalGenerator({ "v0a_generate_signals", "--output-dir", firstDir.string() },
                               output,
                               error) == 0);
    REQUIRE(runSignalGenerator({ "v0a_generate_signals", "--output-dir", secondDir.string() },
                               output,
                               error) == 0);
    REQUIRE(error.str().empty());

    for (const auto* name : { "sine_220hz.wav", "saw_220hz.wav", "vowel_a_220hz.wav", "manifest.txt" })
    {
        REQUIRE(fileBytes(firstDir / name) == fileBytes(secondDir / name));
    }

    for (const auto* name : { "sine_220hz.wav", "saw_220hz.wav", "vowel_a_220hz.wav" })
    {
        const auto audio = readMonoWav(firstDir / name);
        REQUIRE(audio.sampleRate == 48'000);
        REQUIRE(audio.mono.size() == 48'000);
        for (const float sample : audio.mono)
            REQUIRE(std::abs(sample) <= 0.800001F);
    }

    std::filesystem::remove_all(firstDir);
    std::filesystem::remove_all(secondDir);
}

int main()
{
    return test::runAll();
}
