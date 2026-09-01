#include "dsp/NoteMatrix.h"

#include "dsp/PitchMath.h"

#include <algorithm>
#include <array>

namespace v0a
{
namespace
{
constexpr std::array<int, 7> majorIntervals { 0, 2, 4, 5, 7, 9, 11 };
constexpr std::array<int, 7> naturalMinorIntervals { 0, 2, 3, 5, 7, 8, 10 };
}

NoteMatrix::NoteMatrix() noexcept
{
    loadPreset(0, ScalePreset::Chromatic);
}

void NoteMatrix::loadPreset(int keyPitchClass, ScalePreset scale) noexcept
{
    keyPitchClass_ = positiveModulo12(keyPitchClass);
    if (scale == ScalePreset::Custom)
    {
        scale_ = ScalePreset::Custom;
        return;
    }

    modes_.fill(NoteMode::Remove);
    if (scale == ScalePreset::Chromatic)
    {
        modes_.fill(NoteMode::Target);
    }
    else
    {
        const auto& intervals = scale == ScalePreset::NaturalMinor
                              ? naturalMinorIntervals : majorIntervals;
        for (const int interval : intervals)
            modes_[static_cast<std::size_t>(positiveModulo12(keyPitchClass_ + interval))]
                = NoteMode::Target;
    }
    scale_ = scale;
}

void NoteMatrix::setMode(int pitchClass, NoteMode modeValue) noexcept
{
    modes_[static_cast<std::size_t>(positiveModulo12(pitchClass))] = modeValue;
    scale_ = ScalePreset::Custom;
}

NoteMode NoteMatrix::mode(int pitchClass) const noexcept
{
    return modes_[static_cast<std::size_t>(positiveModulo12(pitchClass))];
}

int NoteMatrix::keyPitchClass() const noexcept
{
    return keyPitchClass_;
}

ScalePreset NoteMatrix::scale() const noexcept
{
    return scale_;
}

bool NoteMatrix::hasTarget() const noexcept
{
    return std::find(modes_.begin(), modes_.end(), NoteMode::Target) != modes_.end();
}
} // namespace v0a
