#include "dsp/PitchDetector.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace v0a
{
namespace
{
[[nodiscard]] std::size_t nextPowerOfTwo(std::size_t value)
{
    std::size_t result = 1;
    while (result < value)
        result *= 2;
    return result;
}
}

void PitchDetector::prepare(const PitchDetectorConfig& config)
{
    if (!std::isfinite(config.sampleRate) || config.sampleRate <= 0.0
        || !std::isfinite(config.minimumF0) || config.minimumF0 <= 0.0
        || !std::isfinite(config.maximumF0) || config.maximumF0 <= config.minimumF0
        || config.decimationFactor == 0 || config.hopAnalysisSamples == 0
        || !std::isfinite(config.periodicityEpsilon) || config.periodicityEpsilon <= 0.0
        || !std::isfinite(config.energyThreshold) || config.energyThreshold < 0.0
        || !std::isfinite(config.octaveImprovement) || config.octaveImprovement < 0.0)
    {
        throw std::invalid_argument("invalid pitch detector configuration");
    }

    config_ = config;
    analysisRate_ = config.sampleRate / static_cast<double>(config.decimationFactor);
    minimumLag_ = std::max(2, static_cast<int>(std::ceil(analysisRate_ / config.maximumF0)));
    maximumLag_ = static_cast<int>(std::floor(analysisRate_ / config.minimumF0));
    if (maximumLag_ <= minimumLag_ + 2)
        throw std::invalid_argument("pitch detector F0 range has too few analysis lags");

    preparedMinimumLag_ = minimumLag_;
    preparedMaximumLag_ = maximumLag_;
    history_.assign(nextPowerOfTwo(static_cast<std::size_t>(3 * preparedMaximumLag_ + 8)), 0.0F);
    scores_.assign(static_cast<std::size_t>(preparedMaximumLag_ + 2), 1.0);
    reset();
}

void PitchDetector::reset() noexcept
{
    std::fill(history_.begin(), history_.end(), 0.0F);
    std::fill(scores_.begin(), scores_.end(), 1.0);
    analysisWritePosition_ = 0;
    decimationCounter_ = 0;
    hopCounter_ = 0;
    decimationAccumulator_ = 0.0;
    current_ = {};
}

PitchDetection PitchDetector::processSample(float input) noexcept
{
    current_.updated = false;
    decimationAccumulator_ += std::isfinite(input) ? static_cast<double>(input) : 0.0;
    ++decimationCounter_;
    if (decimationCounter_ < config_.decimationFactor)
        return current_;

    const float analysisSample = static_cast<float>(
        decimationAccumulator_ / static_cast<double>(config_.decimationFactor));
    decimationAccumulator_ = 0.0;
    decimationCounter_ = 0;
    history_[static_cast<std::size_t>(analysisWritePosition_) & (history_.size() - 1)]
        = analysisSample;
    ++analysisWritePosition_;
    ++hopCounter_;

    if (analysisWritePosition_ < static_cast<std::uint64_t>(3 * maximumLag_ + 1)
        || hopCounter_ < config_.hopAnalysisSamples)
    {
        return current_;
    }

    hopCounter_ = 0;
    analyze();
    current_.updated = true;
    return current_;
}

std::size_t PitchDetector::requiredHistorySamples() const noexcept
{
    return static_cast<std::size_t>(3 * preparedMaximumLag_ + 1) * config_.decimationFactor;
}

bool PitchDetector::setFrequencyBounds(double minimumF0, double maximumF0) noexcept
{
    if (!std::isfinite(minimumF0) || !std::isfinite(maximumF0)
        || minimumF0 <= 0.0 || maximumF0 <= minimumF0
        || minimumF0 < config_.minimumF0 || maximumF0 > config_.maximumF0)
    {
        return false;
    }
    const int minimumLag = std::max(2, static_cast<int>(std::ceil(analysisRate_ / maximumF0)));
    const int maximumLag = static_cast<int>(std::floor(analysisRate_ / minimumF0));
    if (minimumLag < preparedMinimumLag_ || maximumLag > preparedMaximumLag_
        || maximumLag <= minimumLag + 2)
    {
        return false;
    }
    minimumLag_ = minimumLag;
    maximumLag_ = maximumLag;
    return true;
}

bool PitchDetector::setPeriodicityEpsilon(double epsilon) noexcept
{
    if (!std::isfinite(epsilon) || epsilon <= 0.0 || epsilon > 1.0)
        return false;
    config_.periodicityEpsilon = epsilon;
    return true;
}

float PitchDetector::history(std::size_t back) const noexcept
{
    const auto position = analysisWritePosition_ - 1U - static_cast<std::uint64_t>(back);
    return history_[static_cast<std::size_t>(position) & (history_.size() - 1)];
}

double PitchDetector::differenceMetric(int lag, double* meanEnergy) const noexcept
{
    double difference = 0.0;
    double energy = 0.0;
    const int comparisonLength = 2 * lag;
    for (int index = 0; index < comparisonLength; ++index)
    {
        const double recent = history(static_cast<std::size_t>(index));
        const double delayed = history(static_cast<std::size_t>(index + lag));
        const double delta = recent - delayed;
        difference += delta * delta;
        energy += recent * recent + delayed * delayed;
    }
    if (meanEnergy != nullptr)
        *meanEnergy = energy / static_cast<double>(2 * comparisonLength);
    return energy > std::numeric_limits<double>::epsilon() ? difference / energy : 1.0;
}

void PitchDetector::analyze() noexcept
{
    for (int lag = minimumLag_; lag <= maximumLag_; ++lag)
        scores_[static_cast<std::size_t>(lag)] = differenceMetric(lag);

    const auto scoreBegin = scores_.begin() + minimumLag_;
    const auto scoreEnd = scores_.begin() + maximumLag_ + 1;
    const auto globalMinimum = std::min_element(scoreBegin, scoreEnd);
    const double bestScore = *globalMinimum;
    const double fundamentalBand = std::min(config_.periodicityEpsilon,
                                            bestScore + 0.5 * config_.periodicityEpsilon);

    int selectedLag = 0;
    for (int lag = minimumLag_ + 1; lag < maximumLag_; ++lag)
    {
        const double score = scores_[static_cast<std::size_t>(lag)];
        if (score <= fundamentalBand
            && score <= scores_[static_cast<std::size_t>(lag - 1)]
            && score < scores_[static_cast<std::size_t>(lag + 1)])
        {
            selectedLag = lag;
            break;
        }
    }

    if (selectedLag == 0)
        selectedLag = static_cast<int>(std::distance(scores_.begin(), globalMinimum));

    bool doubled = false;
    const int doubledLag = 2 * selectedLag;
    if (doubledLag <= maximumLag_
        && scores_[static_cast<std::size_t>(doubledLag)] + config_.octaveImprovement
             < scores_[static_cast<std::size_t>(selectedLag)])
    {
        selectedLag = doubledLag;
        doubled = true;
    }

    double refinedLag = static_cast<double>(selectedLag);
    if (selectedLag > minimumLag_ && selectedLag < maximumLag_)
    {
        const double left = scores_[static_cast<std::size_t>(selectedLag - 1)];
        const double center = scores_[static_cast<std::size_t>(selectedLag)];
        const double right = scores_[static_cast<std::size_t>(selectedLag + 1)];
        const double curvature = left - 2.0 * center + right;
        if (std::isfinite(curvature) && std::abs(curvature) > 1.0e-12)
        {
            const double offset = std::clamp(0.5 * (left - right) / curvature, -1.0, 1.0);
            refinedLag += offset;
        }
    }

    double energy = 0.0;
    const double periodicity = differenceMetric(selectedLag, &energy);
    const bool voiced = energy >= config_.energyThreshold
                     && periodicity <= config_.periodicityEpsilon;

    current_.voiced = voiced;
    current_.usedDoubledLag = doubled;
    current_.integerLag = selectedLag;
    current_.periodSamples = voiced
        ? refinedLag * static_cast<double>(config_.decimationFactor) : 0.0;
    current_.frequency = voiced ? config_.sampleRate / current_.periodSamples : 0.0;
    current_.periodicity = periodicity;
    current_.confidence = std::clamp(1.0 - periodicity, 0.0, 1.0);
    current_.energy = energy;
}
} // namespace v0a
