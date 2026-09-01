#include "dsp/HumanizeController.h"

#include <algorithm>
#include <cmath>

namespace v0a
{
void HumanizeController::prepare(double sampleRate) noexcept
{
    if (std::isfinite(sampleRate) && sampleRate > 0.0)
        sampleRate_ = sampleRate;
    reset();
}

void HumanizeController::reset() noexcept
{
    currentTargetMidi_ = -1000;
    sustainedSamples_ = 0;
}

void HumanizeController::setAmount(double normalizedAmount) noexcept
{
    amount_ = std::isfinite(normalizedAmount)
            ? std::clamp(normalizedAmount, 0.0, 1.0) : 0.0;
}

void HumanizeController::setTiming(double onsetMilliseconds, double rampMilliseconds) noexcept
{
    onsetMilliseconds_ = std::isfinite(onsetMilliseconds)
        ? std::max(0.0, onsetMilliseconds) : 0.0;
    rampMilliseconds_ = std::isfinite(rampMilliseconds)
        ? std::max(0.0, rampMilliseconds) : 0.0;
}

void HumanizeController::setMode(HumanizeTauMode mode) noexcept
{
    mode_ = mode;
}

void HumanizeController::setMaximumMultiplier(double multiplier) noexcept
{
    maximumMultiplier_ = std::isfinite(multiplier) ? std::max(1.0, multiplier) : 1.0;
}

void HumanizeController::setMaximumAdditionalMilliseconds(double milliseconds) noexcept
{
    maximumAdditionalMilliseconds_ = std::isfinite(milliseconds)
        ? std::max(0.0, milliseconds) : 0.0;
}

double HumanizeController::effectiveTauMilliseconds(double baseTauMilliseconds,
                                                     int targetMidi,
                                                     bool voiced) noexcept
{
    const double baseTau = std::isfinite(baseTauMilliseconds)
                         ? std::max(0.0, baseTauMilliseconds) : 0.0;
    if (!voiced)
    {
        reset();
        return baseTau;
    }
    if (targetMidi != currentTargetMidi_)
    {
        currentTargetMidi_ = targetMidi;
        sustainedSamples_ = 0;
    }
    else
    {
        ++sustainedSamples_;
    }

    if (amount_ <= 0.0)
        return baseTau;
    const double elapsedMilliseconds = 1'000.0 * static_cast<double>(sustainedSamples_)
                                     / sampleRate_;
    if (elapsedMilliseconds <= onsetMilliseconds_)
        return baseTau;
    const double progress = rampMilliseconds_ <= 0.0 ? 1.0
        : std::clamp((elapsedMilliseconds - onsetMilliseconds_) / rampMilliseconds_, 0.0, 1.0);
    const double depth = amount_ * progress;
    if (mode_ == HumanizeTauMode::Multiplicative)
        return baseTau * (1.0 + depth * (maximumMultiplier_ - 1.0));
    return baseTau + depth * maximumAdditionalMilliseconds_;
}
} // namespace v0a
