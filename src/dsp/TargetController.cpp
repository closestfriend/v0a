#include "dsp/TargetController.h"

#include "dsp/PitchMath.h"

#include <algorithm>
#include <cmath>

namespace v0a
{
void TargetController::reset() noexcept
{
    activeTargetMidi_ = -1000;
    pendingTargetMidi_ = -1000;
    pendingPeriods_ = 0.0;
}

void TargetController::setParameters(double hysteresisCents, int confirmationPeriods) noexcept
{
    hysteresisCents_ = std::isfinite(hysteresisCents)
                     ? std::max(0.0, hysteresisCents) : 0.0;
    confirmationPeriods_ = std::max(0, confirmationPeriods);
    pendingTargetMidi_ = -1000;
    pendingPeriods_ = 0.0;
}

QuantizationResult TargetController::process(const QuantizationResult& memoryless,
                                             double detectedMidi,
                                             double elapsedPeriods) noexcept
{
    if (memoryless.bypassed || !memoryless.hasTarget)
    {
        reset();
        return memoryless;
    }
    if (activeTargetMidi_ == -1000)
    {
        activeTargetMidi_ = memoryless.targetMidi;
        return memoryless;
    }
    if (memoryless.targetMidi == activeTargetMidi_)
    {
        pendingTargetMidi_ = -1000;
        pendingPeriods_ = 0.0;
        return memoryless;
    }

    const double activeDistance = std::abs(static_cast<double>(activeTargetMidi_) - detectedMidi);
    const double candidateDistance = std::abs(static_cast<double>(memoryless.targetMidi)
                                              - detectedMidi);
    const double advantageCents = 100.0 * (activeDistance - candidateDistance);
    if (advantageCents <= hysteresisCents_)
    {
        pendingTargetMidi_ = -1000;
        pendingPeriods_ = 0.0;
        return withActiveTarget(memoryless, detectedMidi);
    }

    if (confirmationPeriods_ > 0)
    {
        if (pendingTargetMidi_ != memoryless.targetMidi)
        {
            pendingTargetMidi_ = memoryless.targetMidi;
            pendingPeriods_ = 0.0;
        }
        pendingPeriods_ += std::isfinite(elapsedPeriods) ? std::max(0.0, elapsedPeriods) : 0.0;
        if (pendingPeriods_ < static_cast<double>(confirmationPeriods_))
            return withActiveTarget(memoryless, detectedMidi);
    }

    activeTargetMidi_ = memoryless.targetMidi;
    pendingTargetMidi_ = -1000;
    pendingPeriods_ = 0.0;
    return memoryless;
}

QuantizationResult TargetController::withActiveTarget(const QuantizationResult& source,
                                                      double detectedMidi) const noexcept
{
    QuantizationResult result = source;
    result.targetMidi = activeTargetMidi_;
    result.targetFrequency = midiToFrequency(static_cast<double>(activeTargetMidi_));
    result.ratio = std::exp2((static_cast<double>(activeTargetMidi_) - detectedMidi) / 12.0);
    return result;
}
} // namespace v0a
