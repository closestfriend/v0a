#include "dsp/PitchCorrectionEngine.h"

#include "dsp/PitchMath.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace v0a
{
namespace
{
[[nodiscard]] std::size_t nextPowerOfTwo(std::size_t value)
{
    std::size_t result = 1;
    while (result <= value)
        result *= 2;
    return result;
}
}

void PitchCorrectionEngine::prepare(const PitchCorrectionConfig& config)
{
    if (!std::isfinite(config.sampleRate) || config.sampleRate <= 0.0
        || !std::isfinite(config.minimumF0) || config.minimumF0 <= 0.0
        || !std::isfinite(config.maximumF0) || config.maximumF0 <= config.minimumF0)
    {
        throw std::invalid_argument("invalid pitch correction configuration");
    }

    config_ = config;
    PitchDetectorConfig detectorConfig {
        .sampleRate = config.sampleRate,
        .minimumF0 = config.minimumF0,
        .maximumF0 = config.maximumF0,
        .decimationFactor = 8,
        .hopAnalysisSamples = 4,
        .periodicityEpsilon = config.periodicityEpsilon,
        .energyThreshold = config.energyThreshold,
        .octaveImprovement = config.octaveImprovement,
    };
    detector_.prepare(detectorConfig);
    humanize_.prepare(config.sampleRate);
    humanize_.setAmount(config.humanizeAmount);
    humanize_.setMode(config.humanizeMode);
    humanize_.setTiming(config.humanizeOnsetMilliseconds,
                        config.humanizeRampMilliseconds);
    humanize_.setMaximumMultiplier(config.humanizeMaximumMultiplier);
    humanize_.setMaximumAdditionalMilliseconds(
        config.humanizeMaximumAdditionalMilliseconds);
    retune_.prepare(config.sampleRate);
    baseRetuneMilliseconds_ = std::max(0.0, config.retuneMilliseconds);
    retune_.setRetuneMilliseconds(baseRetuneMilliseconds_);
    setUnvoicedMode(config.unvoicedMode, config.unvoicedDurationMilliseconds);
    targetController_.setParameters(config.targetHysteresisCents,
                                    config.targetConfirmationPeriods);

    const double maximumPeriod = config.sampleRate / config.minimumF0;
    const double minimumPeriod = config.sampleRate / config.maximumF0;
    if (static_cast<double>(config.spliceCrossfadeSamples) >= minimumPeriod)
        throw std::invalid_argument("splice crossfade must be shorter than the minimum period");

    latencySamples_ = std::max(
        detector_.requiredHistorySamples(),
        static_cast<std::size_t>(std::ceil(2.0 * maximumPeriod + 8.0)));
    currentPeriodSamples_ = maximumPeriod;
    CycleResamplerConfig resamplerConfig {
        .periodSamples = maximumPeriod,
        .rate = 1.0,
        .bufferCapacity = nextPowerOfTwo(latencySamples_
                                       + static_cast<std::size_t>(std::ceil(maximumPeriod)) + 8),
        .safetyDelaySamples = static_cast<double>(latencySamples_),
        .interpolation = config.interpolation,
        .spliceCrossfadeSamples = config.spliceCrossfadeSamples,
    };
    resampler_ = std::make_unique<CycleResampler>(resamplerConfig);
    delayedControls_.assign(latencySamples_, DelayedControl { maximumPeriod, 1.0 });
    delayedControlPosition_ = 0;
    diagnostics_ = {};
}

void PitchCorrectionEngine::reset() noexcept
{
    detector_.reset();
    humanize_.reset();
    targetController_.reset();
    retune_.reset();
    if (resampler_ != nullptr)
        resampler_->reset();
    diagnostics_ = {};
    currentPeriodSamples_ = config_.sampleRate / config_.minimumF0;
    lastVoicedRatio_ = 1.0;
    unvoicedSamples_ = 0;
    currentlyVoiced_ = false;
    std::fill(delayedControls_.begin(), delayedControls_.end(),
              DelayedControl { currentPeriodSamples_, 1.0 });
    delayedControlPosition_ = 0;
}

float PitchCorrectionEngine::processSample(float input) noexcept
{
    if (resampler_ == nullptr)
        return input;

    const PitchDetection detection = detector_.processSample(input);
    diagnostics_.detection = detection;
    if (detection.updated)
    {
        if (detection.voiced)
        {
            currentlyVoiced_ = true;
            unvoicedSamples_ = 0;
            currentPeriodSamples_ = detection.periodSamples;
            const double detectedMidi = frequencyToMidi(detection.frequency);
            const auto memoryless = quantizer_.quantize(detectedMidi, matrix_);
            diagnostics_.quantization = targetController_.process(
                memoryless, detectedMidi, 32.0 / detection.periodSamples);
            diagnostics_.rawRatio = diagnostics_.quantization.hasTarget
                                  && !diagnostics_.quantization.bypassed
                ? diagnostics_.quantization.ratio : 1.0;
            lastVoicedRatio_ = diagnostics_.rawRatio;
        }
        else
        {
            if (currentlyVoiced_)
                unvoicedSamples_ = 0;
            currentlyVoiced_ = false;
            diagnostics_.quantization = {};
        }
    }

    if (!currentlyVoiced_)
    {
        if (unvoicedMode_ == UnvoicedMode::HoldLastRatio
            && unvoicedSamples_ < unvoicedDurationSamples_)
        {
            diagnostics_.rawRatio = lastVoicedRatio_;
        }
        else if (unvoicedMode_ == UnvoicedMode::FadeToUnity
                 && unvoicedDurationSamples_ > 0
                 && unvoicedSamples_ < unvoicedDurationSamples_)
        {
            const double progress = static_cast<double>(unvoicedSamples_)
                                  / static_cast<double>(unvoicedDurationSamples_);
            diagnostics_.rawRatio = lastVoicedRatio_ + progress * (1.0 - lastVoicedRatio_);
        }
        else
        {
            diagnostics_.rawRatio = 1.0;
            retune_.reset();
        }
        ++unvoicedSamples_;
    }
    if (bypassed_)
    {
        diagnostics_.rawRatio = 1.0;
        retune_.reset();
    }

    const bool humanizeActive = diagnostics_.detection.voiced
                             && diagnostics_.quantization.hasTarget
                             && !diagnostics_.quantization.bypassed;
    diagnostics_.effectiveRetuneMilliseconds = humanize_.effectiveTauMilliseconds(
        baseRetuneMilliseconds_, diagnostics_.quantization.targetMidi, humanizeActive);
    retune_.setRetuneMilliseconds(diagnostics_.effectiveRetuneMilliseconds);
    diagnostics_.smoothedRatio = retune_.process(diagnostics_.rawRatio);
    DelayedControl applied { currentPeriodSamples_, diagnostics_.smoothedRatio };
    if (!delayedControls_.empty())
    {
        applied = delayedControls_[delayedControlPosition_];
        delayedControls_[delayedControlPosition_] = {
            currentPeriodSamples_, diagnostics_.smoothedRatio
        };
        ++delayedControlPosition_;
        if (delayedControlPosition_ == delayedControls_.size())
            delayedControlPosition_ = 0;
    }
    diagnostics_.appliedRatio = applied.ratio;
    diagnostics_.appliedPeriodSamples = applied.periodSamples;
    static_cast<void>(resampler_->setPeriodAndRate(applied.periodSamples, applied.ratio));
    const float output = resampler_->processSample(input);
    diagnostics_.resamplerStats = resampler_->stats();
    return output;
}

void PitchCorrectionEngine::loadScale(int keyPitchClass, ScalePreset scale) noexcept
{
    matrix_.loadPreset(keyPitchClass, scale);
    targetController_.reset();
}

void PitchCorrectionEngine::setNoteMode(int pitchClass, NoteMode mode) noexcept
{
    matrix_.setMode(pitchClass, mode);
    targetController_.reset();
}

void PitchCorrectionEngine::setRetuneMilliseconds(double milliseconds) noexcept
{
    baseRetuneMilliseconds_ = std::isfinite(milliseconds) ? std::max(0.0, milliseconds) : 0.0;
    retune_.setRetuneMilliseconds(baseRetuneMilliseconds_);
}

void PitchCorrectionEngine::setHumanizeAmount(double normalizedAmount) noexcept
{
    humanize_.setAmount(normalizedAmount);
}

void PitchCorrectionEngine::setHumanizeMode(HumanizeTauMode mode) noexcept
{
    humanize_.setMode(mode);
}

void PitchCorrectionEngine::setHumanizeTiming(double onsetMilliseconds,
                                              double rampMilliseconds) noexcept
{
    humanize_.setTiming(onsetMilliseconds, rampMilliseconds);
}

void PitchCorrectionEngine::setHumanizeMaximumMultiplier(double multiplier) noexcept
{
    humanize_.setMaximumMultiplier(multiplier);
}

void PitchCorrectionEngine::setHumanizeMaximumAdditionalMilliseconds(double milliseconds) noexcept
{
    humanize_.setMaximumAdditionalMilliseconds(milliseconds);
}

bool PitchCorrectionEngine::setDetectorFrequencyBounds(double minimumF0,
                                                       double maximumF0) noexcept
{
    return detector_.setFrequencyBounds(minimumF0, maximumF0);
}

bool PitchCorrectionEngine::setTrackingEpsilon(double epsilon) noexcept
{
    return detector_.setPeriodicityEpsilon(epsilon);
}

bool PitchCorrectionEngine::setInterpolation(InterpolationMethod interpolation) noexcept
{
    return resampler_ != nullptr && resampler_->setInterpolation(interpolation);
}

bool PitchCorrectionEngine::setSpliceCrossfadeSamples(std::size_t samples) noexcept
{
    return resampler_ != nullptr && resampler_->setSpliceCrossfadeSamples(samples);
}

void PitchCorrectionEngine::setUnvoicedMode(UnvoicedMode mode,
                                            double durationMilliseconds) noexcept
{
    unvoicedMode_ = mode;
    const double duration = std::isfinite(durationMilliseconds)
                          ? std::max(0.0, durationMilliseconds) : 0.0;
    unvoicedDurationSamples_ = static_cast<std::uint64_t>(
        std::llround(0.001 * duration * config_.sampleRate));
}

void PitchCorrectionEngine::setTargetPersistence(double hysteresisCents,
                                                 int confirmationPeriods) noexcept
{
    targetController_.setParameters(hysteresisCents, confirmationPeriods);
}

void PitchCorrectionEngine::setBypassed(bool bypassed) noexcept
{
    bypassed_ = bypassed;
}

const NoteMatrix& PitchCorrectionEngine::noteMatrix() const noexcept
{
    return matrix_;
}

const PitchCorrectionDiagnostics& PitchCorrectionEngine::diagnostics() const noexcept
{
    return diagnostics_;
}

std::size_t PitchCorrectionEngine::latencySamples() const noexcept
{
    return latencySamples_;
}
} // namespace v0a
