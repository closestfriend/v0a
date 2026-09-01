#include "dsp/RetuneController.h"

#include <algorithm>
#include <cmath>

namespace v0a
{
void RetuneController::prepare(double sampleRate) noexcept
{
    if (std::isfinite(sampleRate) && sampleRate > 0.0)
        sampleRate_ = sampleRate;
    updateCoefficient();
    reset();
}

void RetuneController::reset() noexcept
{
    currentRatio_ = 1.0;
}

void RetuneController::setRetuneMilliseconds(double milliseconds) noexcept
{
    milliseconds_ = std::isfinite(milliseconds) ? std::max(0.0, milliseconds) : 0.0;
    updateCoefficient();
}

double RetuneController::process(double rawRatio) noexcept
{
    if (!std::isfinite(rawRatio) || rawRatio <= 0.0)
        rawRatio = 1.0;

    if (milliseconds_ <= 0.0)
        currentRatio_ = rawRatio;
    else
        currentRatio_ = alpha_ * currentRatio_ + (1.0 - alpha_) * rawRatio;
    return currentRatio_;
}

double RetuneController::currentRatio() const noexcept
{
    return currentRatio_;
}

void RetuneController::updateCoefficient() noexcept
{
    if (milliseconds_ <= 0.0)
    {
        alpha_ = 0.0;
        return;
    }
    const double tauSamples = 0.001 * milliseconds_ * sampleRate_;
    alpha_ = std::exp(-1.0 / tauSamples);
}
} // namespace v0a
