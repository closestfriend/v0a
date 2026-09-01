#include "TestHarness.h"
#include "dsp/Interpolators.h"
#include "dsp/RingBuffer.h"

using v0a::InterpolationMethod;
using v0a::RingBuffer;
using v0a::interpolate;

TEST_CASE("ring buffer rejects zero capacity")
{
    bool threw = false;
    try
    {
        const RingBuffer buffer(0);
        static_cast<void>(buffer);
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    REQUIRE(threw);
}

TEST_CASE("linear interpolation returns the stored sample at an integer position")
{
    RingBuffer buffer(4);
    buffer.write(0, 0.0F);
    buffer.write(1, 2.0F);
    buffer.write(2, 4.0F);
    buffer.write(3, 6.0F);

    REQUIRE_NEAR(interpolate(buffer, 1.0, InterpolationMethod::Linear), 2.0, 1.0e-7);
}

TEST_CASE("linear interpolation computes a hand-derived midpoint")
{
    RingBuffer buffer(4);
    buffer.write(0, 0.0F);
    buffer.write(1, 2.0F);
    buffer.write(2, 4.0F);
    buffer.write(3, 6.0F);

    REQUIRE_NEAR(interpolate(buffer, 1.5, InterpolationMethod::Linear), 3.0, 1.0e-7);
}

TEST_CASE("all interpolators preserve a constant signal across the wrap boundary")
{
    RingBuffer buffer(4);
    for (std::int64_t index = 0; index < 4; ++index)
        buffer.write(index, 0.25F);

    for (const auto method : { InterpolationMethod::Linear,
                               InterpolationMethod::CubicHermite,
                               InterpolationMethod::Lagrange4 })
    {
        REQUIRE_NEAR(interpolate(buffer, 3.75, method), 0.25, 1.0e-6);
    }
}

TEST_CASE("signed absolute indices wrap to the matching ring slot")
{
    RingBuffer buffer(4);
    buffer.write(-1, 7.0F);
    REQUIRE_NEAR(buffer.read(3), 7.0, 1.0e-7);
    REQUIRE_NEAR(buffer.read(-1), 7.0, 1.0e-7);
}

TEST_CASE("four-point interpolators reproduce a linear ramp away from overwritten aliases")
{
    RingBuffer buffer(8);
    for (std::int64_t index = 0; index < 8; ++index)
        buffer.write(index, static_cast<float>(index));

    REQUIRE_NEAR(interpolate(buffer, 3.5, InterpolationMethod::CubicHermite), 3.5, 1.0e-6);
    REQUIRE_NEAR(interpolate(buffer, 3.5, InterpolationMethod::Lagrange4), 3.5, 1.0e-6);
}

int main()
{
    return test::runAll();
}
