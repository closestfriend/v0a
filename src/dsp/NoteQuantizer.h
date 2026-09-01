#pragma once

#include "dsp/NoteMatrix.h"

namespace v0a
{
struct QuantizationResult
{
    double detectedMidi = 0.0;
    int cellMidi = 0;
    NoteMode cellMode = NoteMode::Remove;
    bool hasTarget = false;
    bool bypassed = false;
    int targetMidi = 0;
    double targetFrequency = 0.0;
    double ratio = 1.0;
};

class NoteQuantizer
{
public:
    [[nodiscard]] QuantizationResult quantize(double detectedMidi,
                                              const NoteMatrix& matrix) const noexcept;
};
} // namespace v0a
