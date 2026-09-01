#pragma once

#include "dsp/Interpolators.h"
#include "dsp/RingBuffer.h"

#include <cstddef>
#include <cstdint>

namespace v0a
{
struct CycleResamplerConfig
{
    double periodSamples = 0.0;
    double rate = 1.0;
    std::size_t bufferCapacity = 0;
    double safetyDelaySamples = 0.0;
    InterpolationMethod interpolation = InterpolationMethod::Linear;
    std::size_t spliceCrossfadeSamples = 0;
};

struct CycleResamplerStats
{
    std::uint64_t repeatedCycles = 0;
    std::uint64_t deletedCycles = 0;
    double residualSeparation = 0.0;
    double minimumResidual = 0.0;
    double maximumResidual = 0.0;
    double totalCycleCompensationSamples = 0.0;
};

class CycleResampler
{
public:
    explicit CycleResampler(const CycleResamplerConfig& config);

    [[nodiscard]] float processSample(float input) noexcept;
    [[nodiscard]] bool setPeriodAndRate(double periodSamples, double rate) noexcept;
    [[nodiscard]] bool setInterpolation(InterpolationMethod interpolation) noexcept;
    [[nodiscard]] bool setSpliceCrossfadeSamples(std::size_t samples) noexcept;
    [[nodiscard]] const CycleResamplerStats& stats() const noexcept;
    void reset() noexcept;

private:
    void compensateDrift() noexcept;
    void beginCrossfade(double preJumpReadPosition) noexcept;
    void updateResidualStats() noexcept;

    CycleResamplerConfig config_;
    RingBuffer buffer_;
    std::int64_t writePosition_ = 0;
    double readPosition_ = 0.0;
    double crossfadeOldReadPosition_ = 0.0;
    std::size_t crossfadeIndex_ = 0;
    bool crossfadeActive_ = false;
    CycleResamplerStats stats_;
};
} // namespace v0a
