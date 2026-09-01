#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace v0a
{
struct PitchDetectorConfig
{
    double sampleRate = 48'000.0;
    double minimumF0 = 60.0;
    double maximumF0 = 800.0;
    std::size_t decimationFactor = 8;
    std::size_t hopAnalysisSamples = 4;
    double periodicityEpsilon = 0.30;
    double energyThreshold = 1.0e-5;
    double octaveImprovement = 0.02;
};

struct PitchDetection
{
    bool updated = false;
    bool voiced = false;
    bool usedDoubledLag = false;
    int integerLag = 0;
    double periodSamples = 0.0;
    double frequency = 0.0;
    double periodicity = 1.0;
    double confidence = 0.0;
    double energy = 0.0;
};

class PitchDetector
{
public:
    void prepare(const PitchDetectorConfig& config);
    void reset() noexcept;
    [[nodiscard]] PitchDetection processSample(float input) noexcept;
    [[nodiscard]] std::size_t requiredHistorySamples() const noexcept;
    [[nodiscard]] bool setFrequencyBounds(double minimumF0, double maximumF0) noexcept;
    [[nodiscard]] bool setPeriodicityEpsilon(double epsilon) noexcept;

private:
    [[nodiscard]] float history(std::size_t back) const noexcept;
    [[nodiscard]] double differenceMetric(int lag, double* meanEnergy = nullptr) const noexcept;
    void analyze() noexcept;

    PitchDetectorConfig config_ {};
    double analysisRate_ = 6'000.0;
    int minimumLag_ = 8;
    int maximumLag_ = 100;
    int preparedMinimumLag_ = 8;
    int preparedMaximumLag_ = 100;
    std::vector<float> history_;
    std::vector<double> scores_;
    std::uint64_t analysisWritePosition_ = 0;
    std::size_t decimationCounter_ = 0;
    std::size_t hopCounter_ = 0;
    double decimationAccumulator_ = 0.0;
    PitchDetection current_ {};
};
} // namespace v0a
