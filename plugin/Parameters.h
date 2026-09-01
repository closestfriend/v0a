#pragma once

#include <array>

namespace v1lab::parameters
{
inline constexpr auto key = "key";
inline constexpr auto scale = "scale";
inline constexpr auto retune = "retune";
inline constexpr auto humanize = "humanize";
inline constexpr auto voiceType = "voiceType";
inline constexpr auto tracking = "tracking";
inline constexpr auto bypass = "bypass";
inline constexpr auto interpolation = "interpolation";
inline constexpr auto spliceCrossfade = "spliceCrossfade";
inline constexpr auto trackingEpsilon = "trackingEpsilon";
inline constexpr auto minimumF0 = "minimumF0";
inline constexpr auto maximumF0 = "maximumF0";
inline constexpr auto unvoicedMode = "unvoicedMode";
inline constexpr auto unvoicedDuration = "unvoicedDuration";
inline constexpr auto targetHysteresis = "targetHysteresis";
inline constexpr auto confirmationPeriods = "confirmationPeriods";
inline constexpr auto retuneDomain = "retuneDomain";
inline constexpr auto humanizeMode = "humanizeMode";
inline constexpr auto humanizeOnset = "humanizeOnset";
inline constexpr auto humanizeRamp = "humanizeRamp";
inline constexpr auto humanizeAdditional = "humanizeAdditional";
inline constexpr auto humanizeMultiplier = "humanizeMultiplier";
inline constexpr std::array<const char*, 12> notes {
    "noteC", "noteCs", "noteD", "noteDs", "noteE", "noteF",
    "noteFs", "noteG", "noteGs", "noteA", "noteAs", "noteB"
};
inline constexpr std::array<const char*, 12> noteNames {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};
} // namespace v1lab::parameters
