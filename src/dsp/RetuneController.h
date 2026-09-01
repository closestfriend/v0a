#pragma once

namespace v0a
{
class RetuneController
{
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void setRetuneMilliseconds(double milliseconds) noexcept;
    [[nodiscard]] double process(double rawRatio) noexcept;
    [[nodiscard]] double currentRatio() const noexcept;

private:
    void updateCoefficient() noexcept;

    double sampleRate_ = 48'000.0;
    double milliseconds_ = 0.0;
    double alpha_ = 0.0;
    double currentRatio_ = 1.0;
};
} // namespace v0a
