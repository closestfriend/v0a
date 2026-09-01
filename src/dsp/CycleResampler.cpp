#include "dsp/CycleResampler.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace v0a
{
namespace
{
void validateConfig(const CycleResamplerConfig& config)
{
    if (!std::isfinite(config.periodSamples) || config.periodSamples <= 0.0)
        throw std::invalid_argument("periodSamples must be finite and positive");
    if (!std::isfinite(config.rate) || config.rate <= 0.0)
        throw std::invalid_argument("rate must be finite and positive");
    if (!std::isfinite(config.safetyDelaySamples)
        || config.safetyDelaySamples <= config.periodSamples + 3.0)
    {
        throw std::invalid_argument("safetyDelaySamples must exceed one period plus interpolation lookahead");
    }
    if (static_cast<double>(config.bufferCapacity)
        <= config.safetyDelaySamples + config.periodSamples + 4.0)
    {
        throw std::invalid_argument("bufferCapacity is too small for the configured delay and period");
    }
    if (static_cast<double>(config.spliceCrossfadeSamples) >= config.periodSamples)
        throw std::invalid_argument("splice crossfade must be shorter than one period");
}
} // namespace

CycleResampler::CycleResampler(const CycleResamplerConfig& config)
    : config_(config), buffer_(config.bufferCapacity)
{
    validateConfig(config_);
    reset();
}

float CycleResampler::processSample(float input) noexcept
{
    buffer_.write(writePosition_, input);
    const float newTrajectory = interpolate(buffer_, readPosition_, config_.interpolation);
    float output = newTrajectory;

    if (crossfadeActive_)
    {
        const float oldTrajectory = interpolate(buffer_, crossfadeOldReadPosition_, config_.interpolation);
        const auto denominator = config_.spliceCrossfadeSamples > 1
                               ? config_.spliceCrossfadeSamples - 1
                               : 1;
        const float progress = static_cast<float>(crossfadeIndex_)
                             / static_cast<float>(denominator);
        output = oldTrajectory + progress * (newTrajectory - oldTrajectory);
        crossfadeOldReadPosition_ += config_.rate;
        ++crossfadeIndex_;
        if (crossfadeIndex_ >= config_.spliceCrossfadeSamples)
            crossfadeActive_ = false;
    }

    readPosition_ += config_.rate;
    ++writePosition_;
    compensateDrift();
    updateResidualStats();
    return output;
}

bool CycleResampler::setPeriodAndRate(double periodSamples, double rate) noexcept
{
    if (!std::isfinite(periodSamples) || periodSamples <= 0.0
        || !std::isfinite(rate) || rate <= 0.0
        || config_.safetyDelaySamples <= periodSamples + 3.0
        || static_cast<double>(config_.bufferCapacity)
             <= config_.safetyDelaySamples + periodSamples + 4.0
        || static_cast<double>(config_.spliceCrossfadeSamples) >= periodSamples)
    {
        return false;
    }

    config_.periodSamples = periodSamples;
    config_.rate = rate;
    return true;
}

bool CycleResampler::setInterpolation(InterpolationMethod interpolation) noexcept
{
    config_.interpolation = interpolation;
    return true;
}

bool CycleResampler::setSpliceCrossfadeSamples(std::size_t samples) noexcept
{
    if (static_cast<double>(samples) >= config_.periodSamples)
        return false;
    config_.spliceCrossfadeSamples = samples;
    crossfadeActive_ = false;
    crossfadeIndex_ = 0;
    return true;
}

const CycleResamplerStats& CycleResampler::stats() const noexcept
{
    return stats_;
}

void CycleResampler::reset() noexcept
{
    buffer_.clear();
    writePosition_ = 0;
    readPosition_ = -config_.safetyDelaySamples;
    crossfadeOldReadPosition_ = readPosition_;
    crossfadeIndex_ = 0;
    crossfadeActive_ = false;
    stats_ = {};
}

void CycleResampler::compensateDrift() noexcept
{
    const double referencePosition = static_cast<double>(writePosition_) - config_.safetyDelaySamples;
    double residual = readPosition_ - referencePosition;

    while (residual >= config_.periodSamples)
    {
        beginCrossfade(readPosition_);
        readPosition_ -= config_.periodSamples;
        residual -= config_.periodSamples;
        ++stats_.repeatedCycles;
        stats_.totalCycleCompensationSamples -= config_.periodSamples;
    }

    while (residual <= -config_.periodSamples)
    {
        beginCrossfade(readPosition_);
        readPosition_ += config_.periodSamples;
        residual += config_.periodSamples;
        ++stats_.deletedCycles;
        stats_.totalCycleCompensationSamples += config_.periodSamples;
    }
}

void CycleResampler::beginCrossfade(double preJumpReadPosition) noexcept
{
    if (config_.spliceCrossfadeSamples == 0)
        return;

    crossfadeOldReadPosition_ = preJumpReadPosition;
    crossfadeIndex_ = 0;
    crossfadeActive_ = true;
}

void CycleResampler::updateResidualStats() noexcept
{
    const double referencePosition = static_cast<double>(writePosition_) - config_.safetyDelaySamples;
    stats_.residualSeparation = readPosition_ - referencePosition;
    stats_.minimumResidual = std::min(stats_.minimumResidual, stats_.residualSeparation);
    stats_.maximumResidual = std::max(stats_.maximumResidual, stats_.residualSeparation);
}
} // namespace v0a
