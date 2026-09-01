#include "io/WavFile.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace v0a
{
namespace
{
[[nodiscard]] std::uint16_t readU16(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    if (offset + 2 > bytes.size())
        throw std::runtime_error("truncated WAV 16-bit field");
    return static_cast<std::uint16_t>(bytes[offset])
         | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

[[nodiscard]] std::uint32_t readU32(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    if (offset + 4 > bytes.size())
        throw std::runtime_error("truncated WAV 32-bit field");
    return static_cast<std::uint32_t>(bytes[offset])
         | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U)
         | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U)
         | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

[[nodiscard]] bool hasTag(std::span<const std::uint8_t> bytes,
                          std::size_t offset,
                          const char* tag)
{
    return offset + 4 <= bytes.size()
        && std::memcmp(bytes.data() + offset, tag, 4) == 0;
}

void writeU16(std::ostream& stream, std::uint16_t value)
{
    const char bytes[2] {
        static_cast<char>(value & 0xFFU),
        static_cast<char>((value >> 8U) & 0xFFU),
    };
    stream.write(bytes, 2);
}

void writeU32(std::ostream& stream, std::uint32_t value)
{
    const char bytes[4] {
        static_cast<char>(value & 0xFFU),
        static_cast<char>((value >> 8U) & 0xFFU),
        static_cast<char>((value >> 16U) & 0xFFU),
        static_cast<char>((value >> 24U) & 0xFFU),
    };
    stream.write(bytes, 4);
}
} // namespace

AudioBuffer readMonoWav(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
        throw std::runtime_error("cannot open WAV: " + path.string());
    const auto end = stream.tellg();
    if (end < 0)
        throw std::runtime_error("cannot determine WAV length");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream && !bytes.empty())
        throw std::runtime_error("cannot read complete WAV");

    if (bytes.size() < 12 || !hasTag(bytes, 0, "RIFF") || !hasTag(bytes, 8, "WAVE"))
        throw std::runtime_error("not a RIFF/WAVE file");
    const auto declaredRiffBytes = static_cast<std::uint64_t>(readU32(bytes, 4)) + 8U;
    if (declaredRiffBytes > bytes.size())
        throw std::runtime_error("truncated RIFF payload");

    std::uint16_t format = 0;
    std::uint16_t channels = 0;
    std::uint16_t blockAlign = 0;
    std::uint16_t bitsPerSample = 0;
    std::uint32_t sampleRate = 0;
    bool foundFormat = false;
    std::size_t dataOffset = 0;
    std::size_t dataSize = 0;
    bool foundData = false;

    std::size_t offset = 12;
    while (offset + 8 <= declaredRiffBytes)
    {
        const auto chunkSize = static_cast<std::size_t>(readU32(bytes, offset + 4));
        const auto payload = offset + 8;
        if (payload + chunkSize > bytes.size() || payload + chunkSize > declaredRiffBytes)
            throw std::runtime_error("truncated WAV chunk");

        if (hasTag(bytes, offset, "fmt "))
        {
            if (chunkSize < 16)
                throw std::runtime_error("WAV fmt chunk is too short");
            format = readU16(bytes, payload);
            channels = readU16(bytes, payload + 2);
            sampleRate = readU32(bytes, payload + 4);
            blockAlign = readU16(bytes, payload + 12);
            bitsPerSample = readU16(bytes, payload + 14);
            foundFormat = true;
        }
        else if (hasTag(bytes, offset, "data"))
        {
            dataOffset = payload;
            dataSize = chunkSize;
            foundData = true;
        }

        offset = payload + chunkSize + (chunkSize & 1U);
    }

    if (!foundFormat || !foundData)
        throw std::runtime_error("WAV requires fmt and data chunks");
    if (channels != 1)
        throw std::runtime_error("V0A requires a mono WAV");
    if (sampleRate == 0)
        throw std::runtime_error("WAV sample rate must be positive");

    const bool integerPcm = format == 1 && (bitsPerSample == 16
                                         || bitsPerSample == 24
                                         || bitsPerSample == 32);
    const bool floatPcm = format == 3 && bitsPerSample == 32;
    if (!integerPcm && !floatPcm)
        throw std::runtime_error("unsupported WAV encoding");

    const auto bytesPerSample = static_cast<std::size_t>(bitsPerSample / 8U);
    if (blockAlign != bytesPerSample || dataSize % bytesPerSample != 0)
        throw std::runtime_error("invalid mono WAV block alignment");

    AudioBuffer result;
    result.sampleRate = sampleRate;
    result.mono.resize(dataSize / bytesPerSample);
    for (std::size_t frame = 0; frame < result.mono.size(); ++frame)
    {
        const auto sampleOffset = dataOffset + frame * bytesPerSample;
        if (floatPcm)
        {
            result.mono[frame] = std::bit_cast<float>(readU32(bytes, sampleOffset));
        }
        else if (bitsPerSample == 16)
        {
            const auto sample = static_cast<std::int16_t>(readU16(bytes, sampleOffset));
            result.mono[frame] = static_cast<float>(sample) / 32768.0F;
        }
        else if (bitsPerSample == 24)
        {
            std::uint32_t packed = static_cast<std::uint32_t>(bytes[sampleOffset])
                                 | (static_cast<std::uint32_t>(bytes[sampleOffset + 1]) << 8U)
                                 | (static_cast<std::uint32_t>(bytes[sampleOffset + 2]) << 16U);
            if ((packed & 0x00800000U) != 0)
                packed |= 0xFF000000U;
            const auto sample = static_cast<std::int32_t>(packed);
            result.mono[frame] = static_cast<float>(sample) / 8388608.0F;
        }
        else
        {
            const auto sample = static_cast<std::int32_t>(readU32(bytes, sampleOffset));
            result.mono[frame] = static_cast<float>(static_cast<double>(sample) / 2147483648.0);
        }

        if (!std::isfinite(result.mono[frame]))
            throw std::runtime_error("WAV contains a non-finite float sample");
    }
    return result;
}

void writeMonoFloatWav(const std::filesystem::path& path,
                       std::uint32_t sampleRate,
                       std::span<const float> samples)
{
    if (sampleRate == 0)
        throw std::runtime_error("WAV sample rate must be positive");
    if (samples.size() > (std::numeric_limits<std::uint32_t>::max() - 50U) / 4U)
        throw std::runtime_error("WAV is too large for RIFF32");

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream)
        throw std::runtime_error("cannot create WAV: " + path.string());

    const auto dataBytes = static_cast<std::uint32_t>(samples.size() * sizeof(float));
    stream.write("RIFF", 4);
    writeU32(stream, 50U + dataBytes);
    stream.write("WAVE", 4);
    stream.write("fmt ", 4);
    writeU32(stream, 18);
    writeU16(stream, 3);
    writeU16(stream, 1);
    writeU32(stream, sampleRate);
    writeU32(stream, sampleRate * 4U);
    writeU16(stream, 4);
    writeU16(stream, 32);
    writeU16(stream, 0);
    stream.write("fact", 4);
    writeU32(stream, 4);
    writeU32(stream, static_cast<std::uint32_t>(samples.size()));
    stream.write("data", 4);
    writeU32(stream, dataBytes);
    for (const float sample : samples)
    {
        if (!std::isfinite(sample))
            throw std::runtime_error("cannot write non-finite WAV sample");
        writeU32(stream, std::bit_cast<std::uint32_t>(sample));
    }
    if (!stream)
        throw std::runtime_error("failed while writing WAV");
}
} // namespace v0a
