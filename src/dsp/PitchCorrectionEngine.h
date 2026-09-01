#pragma once

#include "dsp/CycleResampler.h"
#include "dsp/HumanizeController.h"
#include "dsp/NoteMatrix.h"
#include "dsp/NoteQuantizer.h"
#include "dsp/PitchDetector.h"
#include "dsp/RetuneController.h"
#include "dsp/TargetController.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace v0a
{
enum class UnvoicedMode
{
    ImmediateUnity,
    HoldLastRatio,
    FadeToUnity
};

struct PitchCorrectionConfig
{
    double sampleRate = 48'000.0;
    double minimumF0 = 60.0;
    double maximumF0 = 800.0;
    double periodicityEpsilon = 0.30;
    double energyThreshold = 1.0e-5;
    double octaveImprovement = 0.02;
    double retuneMilliseconds = 0.0;
    double humanizeAmount = 0.0;
    HumanizeTauMode humanizeMode = HumanizeTauMode::Additive;
    double humanizeOnsetMilliseconds = 100.0;
    double humanizeRampMilliseconds = 500.0;
    double humanizeMaximumMultiplier = 4.0;
    double humanizeMaximumAdditionalMilliseconds = 100.0;
    UnvoicedMode unvoicedMode = UnvoicedMode::ImmediateUnity;
    double unvoicedDurationMilliseconds = 0.0;
    double targetHysteresisCents = 0.0;
    int targetConfirmationPeriods = 0;
    InterpolationMethod interpolation = InterpolationMethod::Linear;
    std::size_t spliceCrossfadeSamples = 0;
};

struct PitchCorrectionDiagnostics
{
    PitchDetection detection {};
    QuantizationResult quantization {};
    double rawRatio = 1.0;
    double smoothedRatio = 1.0;
    double appliedRatio = 1.0;
    double appliedPeriodSamples = 0.0;
    double effectiveRetuneMilliseconds = 0.0;
    CycleResamplerStats resamplerStats {};
};

class PitchCorrectionEngine
{
public:
    PitchCorrectionEngine() = default;
    ~PitchCorrectionEngine() = default;
    PitchCorrectionEngine(PitchCorrectionEngine&&) noexcept = default;
    PitchCorrectionEngine& operator=(PitchCorrectionEngine&&) noexcept = default;
    PitchCorrectionEngine(const PitchCorrectionEngine&) = delete;
    PitchCorrectionEngine& operator=(const PitchCorrectionEngine&) = delete;

    void prepare(const PitchCorrectionConfig& config);
    void reset() noexcept;
    [[nodiscard]] float processSample(float input) noexcept;

    void loadScale(int keyPitchClass, ScalePreset scale) noexcept;
    void setNoteMode(int pitchClass, NoteMode mode) noexcept;
    void setRetuneMilliseconds(double milliseconds) noexcept;
    void setHumanizeAmount(double normalizedAmount) noexcept;
    void setHumanizeMode(HumanizeTauMode mode) noexcept;
    void setHumanizeTiming(double onsetMilliseconds, double rampMilliseconds) noexcept;
    void setHumanizeMaximumMultiplier(double multiplier) noexcept;
    void setHumanizeMaximumAdditionalMilliseconds(double milliseconds) noexcept;
    [[nodiscard]] bool setDetectorFrequencyBounds(double minimumF0, double maximumF0) noexcept;
    [[nodiscard]] bool setTrackingEpsilon(double epsilon) noexcept;
    [[nodiscard]] bool setInterpolation(InterpolationMethod interpolation) noexcept;
    [[nodiscard]] bool setSpliceCrossfadeSamples(std::size_t samples) noexcept;
    void setUnvoicedMode(UnvoicedMode mode, double durationMilliseconds) noexcept;
    void setTargetPersistence(double hysteresisCents, int confirmationPeriods) noexcept;
    void setBypassed(bool bypassed) noexcept;
    [[nodiscard]] const NoteMatrix& noteMatrix() const noexcept;
    [[nodiscard]] const PitchCorrectionDiagnostics& diagnostics() const noexcept;
    [[nodiscard]] std::size_t latencySamples() const noexcept;

private:
    struct DelayedControl
    {
        double periodSamples = 800.0;
        double ratio = 1.0;
    };

    PitchCorrectionConfig config_ {};
    PitchDetector detector_;
    NoteMatrix matrix_;
    NoteQuantizer quantizer_;
    TargetController targetController_;
    HumanizeController humanize_;
    RetuneController retune_;
    std::unique_ptr<CycleResampler> resampler_;
    PitchCorrectionDiagnostics diagnostics_ {};
    std::vector<DelayedControl> delayedControls_;
    std::size_t delayedControlPosition_ = 0;
    std::size_t latencySamples_ = 0;
    double currentPeriodSamples_ = 800.0;
    double baseRetuneMilliseconds_ = 0.0;
    double lastVoicedRatio_ = 1.0;
    std::uint64_t unvoicedSamples_ = 0;
    std::uint64_t unvoicedDurationSamples_ = 0;
    UnvoicedMode unvoicedMode_ = UnvoicedMode::ImmediateUnity;
    bool currentlyVoiced_ = false;
    bool bypassed_ = false;
};
} // namespace v0a
