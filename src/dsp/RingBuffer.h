#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace v0a
{
class RingBuffer
{
public:
    explicit RingBuffer(std::size_t capacity)
        : samples_(capacity, 0.0F)
    {
        if (capacity == 0)
            throw std::invalid_argument("ring buffer capacity must be positive");
    }

    [[nodiscard]] std::size_t capacity() const noexcept
    {
        return samples_.size();
    }

    void clear() noexcept
    {
        std::fill(samples_.begin(), samples_.end(), 0.0F);
    }

    void write(std::int64_t absoluteIndex, float sample) noexcept
    {
        samples_[wrappedIndex(absoluteIndex)] = sample;
    }

    [[nodiscard]] float read(std::int64_t absoluteIndex) const noexcept
    {
        return samples_[wrappedIndex(absoluteIndex)];
    }

private:
    [[nodiscard]] std::size_t wrappedIndex(std::int64_t absoluteIndex) const noexcept
    {
        const auto modulus = static_cast<std::int64_t>(samples_.size());
        auto wrapped = absoluteIndex % modulus;
        if (wrapped < 0)
            wrapped += modulus;
        return static_cast<std::size_t>(wrapped);
    }

    std::vector<float> samples_;
};
} // namespace v0a
