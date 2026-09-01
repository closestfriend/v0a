#include "dsp/NoteQuantizer.h"

#include "dsp/PitchMath.h"

#include <cmath>
#include <limits>

namespace v0a
{
QuantizationResult NoteQuantizer::quantize(double detectedMidi,
                                           const NoteMatrix& matrix) const noexcept
{
    QuantizationResult result;
    result.detectedMidi = detectedMidi;
    if (!std::isfinite(detectedMidi))
        return result;

    result.cellMidi = static_cast<int>(std::floor(detectedMidi + 0.5));
    result.cellMode = matrix.mode(result.cellMidi);
    if (result.cellMode == NoteMode::Bypass)
    {
        result.bypassed = true;
        result.targetMidi = result.cellMidi;
        result.targetFrequency = midiToFrequency(static_cast<double>(result.targetMidi));
        return result;
    }
    if (!matrix.hasTarget())
        return result;

    double bestDistance = std::numeric_limits<double>::infinity();
    int bestMidi = result.cellMidi;
    for (int midi = result.cellMidi - 12; midi <= result.cellMidi + 12; ++midi)
    {
        if (matrix.mode(midi) != NoteMode::Target)
            continue;
        const double distance = std::abs(static_cast<double>(midi) - detectedMidi);
        if (distance < bestDistance - 1.0e-12
            || (std::abs(distance - bestDistance) <= 1.0e-12 && midi > bestMidi))
        {
            bestDistance = distance;
            bestMidi = midi;
        }
    }

    result.hasTarget = true;
    result.targetMidi = bestMidi;
    result.targetFrequency = midiToFrequency(static_cast<double>(bestMidi));
    result.ratio = std::exp2((static_cast<double>(bestMidi) - detectedMidi) / 12.0);
    return result;
}
} // namespace v0a
