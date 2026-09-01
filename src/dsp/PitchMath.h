#pragma once

#include <cmath>

namespace v0a
{
[[nodiscard]] inline double frequencyToMidi(double frequency) noexcept
{
    return frequency > 0.0 && std::isfinite(frequency)
         ? 69.0 + 12.0 * std::log2(frequency / 440.0)
         : 0.0;
}

[[nodiscard]] inline double midiToFrequency(double midi) noexcept
{
    return std::isfinite(midi) ? 440.0 * std::exp2((midi - 69.0) / 12.0) : 0.0;
}

[[nodiscard]] inline int positiveModulo12(int value) noexcept
{
    const int remainder = value % 12;
    return remainder < 0 ? remainder + 12 : remainder;
}
} // namespace v0a
