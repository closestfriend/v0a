#include "TestHarness.h"
#include "io/WavFile.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

using v0a::AudioBuffer;
using v0a::readMonoWav;
using v0a::writeMonoFloatWav;

namespace
{
void appendU16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void appendU32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
}

std::uint16_t decodeU16(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset])
         | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

std::uint32_t decodeU32(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset])
         | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U)
         | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U)
         | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

void appendTag(std::vector<std::uint8_t>& bytes, const char* tag)
{
    for (int index = 0; index < 4; ++index)
        bytes.push_back(static_cast<std::uint8_t>(tag[index]));
}

std::vector<std::uint8_t> pcmWav(std::uint16_t channels,
                                 std::uint16_t bitsPerSample,
                                 std::span<const std::uint8_t> sampleBytes)
{
    std::vector<std::uint8_t> bytes;
    appendTag(bytes, "RIFF");
    appendU32(bytes, 36U + static_cast<std::uint32_t>(sampleBytes.size()));
    appendTag(bytes, "WAVE");
    appendTag(bytes, "fmt ");
    appendU32(bytes, 16);
    appendU16(bytes, 1);
    appendU16(bytes, channels);
    appendU32(bytes, 48'000);
    const auto blockAlign = static_cast<std::uint16_t>(channels * bitsPerSample / 8U);
    appendU32(bytes, 48'000U * blockAlign);
    appendU16(bytes, blockAlign);
    appendU16(bytes, bitsPerSample);
    appendTag(bytes, "data");
    appendU32(bytes, static_cast<std::uint32_t>(sampleBytes.size()));
    bytes.insert(bytes.end(), sampleBytes.begin(), sampleBytes.end());
    return bytes;
}

std::filesystem::path testPath(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

void writeBytes(const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    const auto size = static_cast<std::size_t>(stream.tellg());
    std::vector<std::uint8_t> bytes(size);
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return bytes;
}

bool readThrows(const std::filesystem::path& path)
{
    try
    {
        static_cast<void>(readMonoWav(path));
        return false;
    }
    catch (const std::runtime_error&)
    {
        return true;
    }
}
} // namespace

TEST_CASE("float32 mono WAV round-trips sample rate frame count and samples")
{
    const auto path = testPath("v0a-float-roundtrip.wav");
    const std::vector<float> samples { -1.0F, -0.25F, 0.0F, 0.5F, 1.0F };
    writeMonoFloatWav(path, 48'000, samples);
    const AudioBuffer decoded = readMonoWav(path);

    REQUIRE(decoded.sampleRate == 48'000);
    REQUIRE(decoded.mono.size() == samples.size());
    for (std::size_t index = 0; index < samples.size(); ++index)
        REQUIRE_NEAR(decoded.mono[index], samples[index], 1.0e-7);
    std::filesystem::remove(path);
}

TEST_CASE("float32 writer emits WAVEFORMATEX extension and fact sample count")
{
    const auto path = testPath("v0a-float-header.wav");
    const std::vector<float> samples { 0.0F, 0.25F, -0.25F };
    writeMonoFloatWav(path, 48'000, samples);
    const auto bytes = readBytes(path);

    REQUIRE(decodeU32(bytes, 16) == 18);
    REQUIRE(decodeU16(bytes, 36) == 0);
    REQUIRE(std::string(reinterpret_cast<const char*>(bytes.data() + 38), 4) == "fact");
    REQUIRE(decodeU32(bytes, 42) == 4);
    REQUIRE(decodeU32(bytes, 46) == samples.size());
    REQUIRE(std::string(reinterpret_cast<const char*>(bytes.data() + 50), 4) == "data");
    std::filesystem::remove(path);
}

TEST_CASE("PCM16 mono decoding uses signed full-scale normalization")
{
    const auto path = testPath("v0a-pcm16.wav");
    const std::vector<std::uint8_t> raw { 0x00, 0x80, 0x00, 0x00, 0x00, 0x40, 0xFF, 0x7F };
    const auto bytes = pcmWav(1, 16, raw);
    writeBytes(path, bytes);
    const auto decoded = readMonoWav(path);

    REQUIRE(decoded.mono.size() == 4);
    REQUIRE_NEAR(decoded.mono[0], -1.0, 1.0e-7);
    REQUIRE_NEAR(decoded.mono[1], 0.0, 1.0e-7);
    REQUIRE_NEAR(decoded.mono[2], 0.5, 1.0e-7);
    REQUIRE_NEAR(decoded.mono[3], 32767.0 / 32768.0, 1.0e-7);
    std::filesystem::remove(path);
}

TEST_CASE("PCM24 and PCM32 mono decoding sign-extends little-endian samples")
{
    const auto path24 = testPath("v0a-pcm24.wav");
    const std::vector<std::uint8_t> raw24 { 0x00, 0x00, 0x80, 0x00, 0x00, 0x40 };
    writeBytes(path24, pcmWav(1, 24, raw24));
    const auto decoded24 = readMonoWav(path24);
    REQUIRE_NEAR(decoded24.mono[0], -1.0, 1.0e-7);
    REQUIRE_NEAR(decoded24.mono[1], 0.5, 1.0e-7);

    const auto path32 = testPath("v0a-pcm32.wav");
    const std::vector<std::uint8_t> raw32 { 0x00, 0x00, 0x00, 0x80,
                                            0x00, 0x00, 0x00, 0x40 };
    writeBytes(path32, pcmWav(1, 32, raw32));
    const auto decoded32 = readMonoWav(path32);
    REQUIRE_NEAR(decoded32.mono[0], -1.0, 1.0e-7);
    REQUIRE_NEAR(decoded32.mono[1], 0.5, 1.0e-7);

    std::filesystem::remove(path24);
    std::filesystem::remove(path32);
}

TEST_CASE("reader rejects stereo and truncated WAV data")
{
    const auto stereoPath = testPath("v0a-stereo.wav");
    const std::vector<std::uint8_t> stereoRaw { 0, 0, 0, 0 };
    writeBytes(stereoPath, pcmWav(2, 16, stereoRaw));
    REQUIRE(readThrows(stereoPath));

    const auto truncatedPath = testPath("v0a-truncated.wav");
    auto truncated = pcmWav(1, 16, stereoRaw);
    truncated.resize(truncated.size() - 1);
    writeBytes(truncatedPath, truncated);
    REQUIRE(readThrows(truncatedPath));

    std::filesystem::remove(stereoPath);
    std::filesystem::remove(truncatedPath);
}

int main()
{
    return test::runAll();
}
