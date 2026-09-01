#pragma once

#include "dsp/NoteQuantizer.h"

namespace v0a
{
class TargetController
{
public:
    void reset() noexcept;
    void setParameters(double hysteresisCents, int confirmationPeriods) noexcept;
    [[nodiscard]] QuantizationResult process(const QuantizationResult& memoryless,
                                             double detectedMidi,
                                             double elapsedPeriods) noexcept;

private:
    [[nodiscard]] QuantizationResult withActiveTarget(const QuantizationResult& source,
                                                      double detectedMidi) const noexcept;

    double hysteresisCents_ = 0.0;
    int confirmationPeriods_ = 0;
    int activeTargetMidi_ = -1000;
    int pendingTargetMidi_ = -1000;
    double pendingPeriods_ = 0.0;
};
} // namespace v0a
