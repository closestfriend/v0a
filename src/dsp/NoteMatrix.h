#pragma once

#include <array>

namespace v0a
{
enum class NoteMode
{
    Target,
    Bypass,
    Remove
};

enum class ScalePreset
{
    Chromatic,
    Major,
    NaturalMinor,
    Custom
};

class NoteMatrix
{
public:
    NoteMatrix() noexcept;

    void loadPreset(int keyPitchClass, ScalePreset scale) noexcept;
    void setMode(int pitchClass, NoteMode mode) noexcept;
    [[nodiscard]] NoteMode mode(int pitchClass) const noexcept;
    [[nodiscard]] int keyPitchClass() const noexcept;
    [[nodiscard]] ScalePreset scale() const noexcept;
    [[nodiscard]] bool hasTarget() const noexcept;

private:
    std::array<NoteMode, 12> modes_ {};
    int keyPitchClass_ = 0;
    ScalePreset scale_ = ScalePreset::Chromatic;
};
} // namespace v0a
