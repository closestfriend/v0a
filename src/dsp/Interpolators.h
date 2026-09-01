#pragma once

#include "dsp/RingBuffer.h"

#include <cmath>
#include <cstdint>

namespace v0a
{
enum class InterpolationMethod
{
    Linear,
    CubicHermite,
    Lagrange4
};

[[nodiscard]] inline float interpolate(const RingBuffer& buffer,
                                       double position,
                                       InterpolationMethod method) noexcept
{
    const auto base = static_cast<std::int64_t>(std::floor(position));
    const auto fraction = static_cast<float>(position - static_cast<double>(base));
    const float y0 = buffer.read(base - 1);
    const float y1 = buffer.read(base);
    const float y2 = buffer.read(base + 1);
    const float y3 = buffer.read(base + 2);

    switch (method)
    {
        case InterpolationMethod::Linear:
            return y1 + fraction * (y2 - y1);

        case InterpolationMethod::CubicHermite:
        {
            const float c0 = y1;
            const float c1 = 0.5F * (y2 - y0);
            const float c2 = y0 - 2.5F * y1 + 2.0F * y2 - 0.5F * y3;
            const float c3 = 0.5F * (y3 - y0) + 1.5F * (y1 - y2);
            return ((c3 * fraction + c2) * fraction + c1) * fraction + c0;
        }

        case InterpolationMethod::Lagrange4:
        {
            const float t = fraction;
            const float l0 = -t * (t - 1.0F) * (t - 2.0F) / 6.0F;
            const float l1 = (t + 1.0F) * (t - 1.0F) * (t - 2.0F) / 2.0F;
            const float l2 = -(t + 1.0F) * t * (t - 2.0F) / 2.0F;
            const float l3 = (t + 1.0F) * t * (t - 1.0F) / 6.0F;
            return l0 * y0 + l1 * y1 + l2 * y2 + l3 * y3;
        }
    }

    return y1;
}
} // namespace v0a
