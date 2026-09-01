#include "TestHarness.h"
#include "dsp/TargetController.h"

using v0a::NoteMatrix;
using v0a::NoteMode;
using v0a::NoteQuantizer;
using v0a::TargetController;

TEST_CASE("zero hysteresis and confirmation preserve memoryless target selection")
{
    NoteMatrix matrix;
    NoteQuantizer quantizer;
    TargetController controller;
    controller.setParameters(0.0, 0);
    REQUIRE(controller.process(quantizer.quantize(60.49, matrix), 60.49, 1.0).targetMidi == 60);
    REQUIRE(controller.process(quantizer.quantize(60.51, matrix), 60.51, 1.0).targetMidi == 61);
}

TEST_CASE("experimental hysteresis requires a literal advantage over the active target")
{
    NoteMatrix matrix;
    NoteQuantizer quantizer;
    TargetController controller;
    controller.setParameters(20.0, 0);
    static_cast<void>(controller.process(quantizer.quantize(60.40, matrix), 60.40, 1.0));
    REQUIRE(controller.process(quantizer.quantize(60.51, matrix), 60.51, 1.0).targetMidi == 60);
    REQUIRE(controller.process(quantizer.quantize(60.61, matrix), 60.61, 1.0).targetMidi == 61);
}

TEST_CASE("experimental confirmation counts elapsed input periods")
{
    NoteMatrix matrix;
    NoteQuantizer quantizer;
    TargetController controller;
    controller.setParameters(0.0, 2);
    static_cast<void>(controller.process(quantizer.quantize(60.40, matrix), 60.40, 1.0));
    REQUIRE(controller.process(quantizer.quantize(60.60, matrix), 60.60, 1.0).targetMidi == 60);
    REQUIRE(controller.process(quantizer.quantize(60.60, matrix), 60.60, 1.0).targetMidi == 61);
}

TEST_CASE("Bypass fixed cells take effect immediately regardless of persistence")
{
    NoteMatrix matrix;
    NoteQuantizer quantizer;
    TargetController controller;
    controller.setParameters(50.0, 8);
    static_cast<void>(controller.process(quantizer.quantize(64.2, matrix), 64.2, 1.0));
    matrix.setMode(5, NoteMode::Bypass);
    const auto result = controller.process(quantizer.quantize(65.1, matrix), 65.1, 0.1);
    REQUIRE(result.bypassed);
    REQUIRE_NEAR(result.ratio, 1.0, 1.0e-12);
}

int main()
{
    return test::runAll();
}
