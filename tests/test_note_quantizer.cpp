#include "TestHarness.h"
#include "dsp/NoteQuantizer.h"

#include <array>
#include <cmath>

using v0a::NoteMatrix;
using v0a::NoteMode;
using v0a::NoteQuantizer;
using v0a::ScalePreset;

TEST_CASE("ordinary chromatic cells own the 49 cent side of each midpoint")
{
    NoteMatrix matrix;
    NoteQuantizer quantizer;

    const auto below = quantizer.quantize(60.49, matrix);
    const auto above = quantizer.quantize(60.51, matrix);
    REQUIRE(below.cellMidi == 60);
    REQUIRE(below.targetMidi == 60);
    REQUIRE(above.cellMidi == 61);
    REQUIRE(above.targetMidi == 61);
    REQUIRE_NEAR(below.ratio, std::pow(2.0, -0.49 / 12.0), 1.0e-12);
    REQUIRE_NEAR(above.ratio, std::pow(2.0, 0.49 / 12.0), 1.0e-12);
}

TEST_CASE("bypass owns only its fixed cell while remove owns no capture region")
{
    NoteMatrix matrix;
    NoteQuantizer quantizer;
    constexpr int fPitchClass = 5;

    matrix.setMode(fPitchClass, NoteMode::Bypass);
    for (const double midi : { 64.51, 65.0, 65.49 })
    {
        const auto result = quantizer.quantize(midi, matrix);
        REQUIRE(result.bypassed);
        REQUIRE(result.cellMode == NoteMode::Bypass);
        REQUIRE_NEAR(result.ratio, 1.0, 1.0e-12);
    }

    matrix.setMode(fPitchClass, NoteMode::Remove);
    for (const double midi : { 64.51, 65.0, 65.49 })
    {
        const auto result = quantizer.quantize(midi, matrix);
        REQUIRE(!result.bypassed);
        REQUIRE(result.hasTarget);
        REQUIRE(result.targetMidi == (midi < 65.0 ? 64 : 66));
    }
}

TEST_CASE("target mode captures its own F cell at minus 49 zero and plus 49 cents")
{
    NoteMatrix matrix;
    NoteQuantizer quantizer;
    for (const double midi : { 64.51, 65.0, 65.49 })
    {
        const auto result = quantizer.quantize(midi, matrix);
        REQUIRE(!result.bypassed);
        REQUIRE(result.targetMidi == 65);
    }
}

TEST_CASE("equal-distance target ties resolve upward")
{
    NoteMatrix matrix;
    for (int pitchClass = 0; pitchClass < 12; ++pitchClass)
        matrix.setMode(pitchClass, NoteMode::Remove);
    matrix.setMode(4, NoteMode::Target);
    matrix.setMode(6, NoteMode::Target);

    const auto result = NoteQuantizer {}.quantize(65.0, matrix);
    REQUIRE(result.hasTarget);
    REQUIRE(result.targetMidi == 66);
}

TEST_CASE("a matrix with no targets returns explicit unity")
{
    NoteMatrix matrix;
    for (int pitchClass = 0; pitchClass < 12; ++pitchClass)
        matrix.setMode(pitchClass, NoteMode::Remove);

    const auto result = NoteQuantizer {}.quantize(60.2, matrix);
    REQUIRE(!result.hasTarget);
    REQUIRE(!result.bypassed);
    REQUIRE_NEAR(result.ratio, 1.0, 1.0e-12);
}

TEST_CASE("key and scale load presets while manual edits enter Custom")
{
    NoteMatrix matrix;
    matrix.loadPreset(2, ScalePreset::Major);
    const std::array<NoteMode, 12> dMajor {
        NoteMode::Remove, NoteMode::Target, NoteMode::Target, NoteMode::Remove,
        NoteMode::Target, NoteMode::Remove, NoteMode::Target, NoteMode::Target,
        NoteMode::Remove, NoteMode::Target, NoteMode::Remove, NoteMode::Target
    };
    for (int pitchClass = 0; pitchClass < 12; ++pitchClass)
        REQUIRE(matrix.mode(pitchClass) == dMajor[static_cast<std::size_t>(pitchClass)]);
    REQUIRE(matrix.keyPitchClass() == 2);
    REQUIRE(matrix.scale() == ScalePreset::Major);

    matrix.setMode(1, NoteMode::Bypass);
    REQUIRE(matrix.scale() == ScalePreset::Custom);
    matrix.loadPreset(9, ScalePreset::NaturalMinor);
    REQUIRE(matrix.keyPitchClass() == 9);
    REQUIRE(matrix.scale() == ScalePreset::NaturalMinor);
    REQUIRE(matrix.mode(1) == NoteMode::Remove);
}

int main()
{
    return test::runAll();
}
