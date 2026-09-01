#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace v0a
{
struct AudioBuffer
{
    std::uint32_t sampleRate = 0;
    std::vector<float> mono;
};

[[nodiscard]] AudioBuffer readMonoWav(const std::filesystem::path& path);
void writeMonoFloatWav(const std::filesystem::path& path,
                       std::uint32_t sampleRate,
                       std::span<const float> samples);
} // namespace v0a
