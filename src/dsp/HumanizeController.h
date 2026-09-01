#pragma once

#include <cstdint>

namespace v0a
{
enum class HumanizeTauMode
{
    Multiplicative,
    Additive
};

class HumanizeController
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void setAmount(double normalizedAmount) noexcept;
    void setTiming(double onsetMilliseconds, double rampMilliseconds) noexcept;
    void setMode(HumanizeTauMode mode) noexcept;
    void setMaximumMultiplier(double multiplier) noexcept;
    void setMaximumAdditionalMilliseconds(double milliseconds) noexcept;
    [[nodiscard]] double effectiveTauMilliseconds(double baseTauMilliseconds,
                                                  int targetMidi,
                                                  bool voiced) noexcept;

private:
    double sampleRate_ = 48'000.0;
    double amount_ = 0.0;
    double onsetMilliseconds_ = 100.0;
    double rampMilliseconds_ = 500.0;
    double maximumMultiplier_ = 4.0;
    double maximumAdditionalMilliseconds_ = 100.0;
    HumanizeTauMode mode_ = HumanizeTauMode::Additive;
    int currentTargetMidi_ = -1000;
    std::uint64_t sustainedSamples_ = 0;
};
} // namespace v0a
