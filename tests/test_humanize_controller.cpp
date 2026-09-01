#include "TestHarness.h"
#include "dsp/HumanizeController.h"

using v0a::HumanizeController;
using v0a::HumanizeTauMode;

TEST_CASE("Humanize is inactive before sustain onset and when amount is zero")
{
    HumanizeController controller;
    controller.prepare(1'000.0);
    controller.setAmount(1.0);
    controller.setTiming(100.0, 100.0);
    for (int sample = 0; sample < 99; ++sample)
        REQUIRE_NEAR(controller.effectiveTauMilliseconds(20.0, 60, true), 20.0, 1.0e-12);

    controller.reset();
    controller.setAmount(0.0);
    for (int sample = 0; sample < 500; ++sample)
        REQUIRE_NEAR(controller.effectiveTauMilliseconds(20.0, 60, true), 20.0, 1.0e-12);
}

TEST_CASE("multiplicative Humanize cannot create smoothing at Retune zero")
{
    HumanizeController controller;
    controller.prepare(1'000.0);
    controller.setAmount(1.0);
    controller.setTiming(10.0, 10.0);
    controller.setMode(HumanizeTauMode::Multiplicative);
    controller.setMaximumMultiplier(5.0);
    double effective = 0.0;
    for (int sample = 0; sample < 100; ++sample)
        effective = controller.effectiveTauMilliseconds(0.0, 60, true);
    REQUIRE_NEAR(effective, 0.0, 1.0e-12);
}

TEST_CASE("additive Humanize produces an explicit sustained-note tau at Retune zero")
{
    HumanizeController controller;
    controller.prepare(1'000.0);
    controller.setAmount(1.0);
    controller.setTiming(10.0, 10.0);
    controller.setMode(HumanizeTauMode::Additive);
    controller.setMaximumAdditionalMilliseconds(80.0);
    double effective = 0.0;
    for (int sample = 0; sample < 100; ++sample)
        effective = controller.effectiveTauMilliseconds(0.0, 60, true);
    REQUIRE_NEAR(effective, 80.0, 1.0e-9);
}

TEST_CASE("target changes and unvoiced samples restart sustain timing")
{
    HumanizeController controller;
    controller.prepare(1'000.0);
    controller.setAmount(1.0);
    controller.setTiming(10.0, 10.0);
    controller.setMode(HumanizeTauMode::Additive);
    controller.setMaximumAdditionalMilliseconds(80.0);
    for (int sample = 0; sample < 100; ++sample)
        static_cast<void>(controller.effectiveTauMilliseconds(0.0, 60, true));
    REQUIRE_NEAR(controller.effectiveTauMilliseconds(0.0, 61, true), 0.0, 1.0e-12);
    for (int sample = 0; sample < 100; ++sample)
        static_cast<void>(controller.effectiveTauMilliseconds(0.0, 61, true));
    REQUIRE_NEAR(controller.effectiveTauMilliseconds(0.0, 61, false), 0.0, 1.0e-12);
}

int main()
{
    return test::runAll();
}
