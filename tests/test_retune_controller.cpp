#include "TestHarness.h"
#include "dsp/RetuneController.h"

#include <cmath>

using v0a::RetuneController;

TEST_CASE("Retune Speed zero replaces the correction ratio on the same sample")
{
    RetuneController controller;
    controller.prepare(48'000.0);
    controller.setRetuneMilliseconds(0.0);
    REQUIRE_NEAR(controller.process(1.059463094359), 1.059463094359, 1.0e-12);
    REQUIRE_NEAR(controller.process(0.943874312682), 0.943874312682, 1.0e-12);
}

TEST_CASE("nonzero Retune follows a ratio-domain one-pole without target-change reset")
{
    RetuneController controller;
    controller.prepare(1'000.0);
    controller.setRetuneMilliseconds(100.0);
    const double alpha = std::exp(-1.0 / 100.0);

    const double first = controller.process(2.0);
    REQUIRE_NEAR(first, alpha + (1.0 - alpha) * 2.0, 1.0e-12);
    const double second = controller.process(0.5);
    REQUIRE_NEAR(second, alpha * first + (1.0 - alpha) * 0.5, 1.0e-12);
}

TEST_CASE("invalid input ratios return unity and reset is deterministic")
{
    RetuneController controller;
    controller.prepare(48'000.0);
    controller.setRetuneMilliseconds(0.0);
    REQUIRE_NEAR(controller.process(-1.0), 1.0, 1.0e-12);
    REQUIRE_NEAR(controller.process(1.2), 1.2, 1.0e-12);
    controller.reset();
    REQUIRE_NEAR(controller.currentRatio(), 1.0, 1.0e-12);
}

int main()
{
    return test::runAll();
}
