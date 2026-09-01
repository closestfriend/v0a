#pragma once

#include "Parameters.h"
#include "dsp/PitchCorrectionEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace v1lab
{
struct DiagnosticSnapshot
{
    float inputF0 = 0.0F;
    float detectedMidi = 0.0F;
    int targetMidi = -1;
    bool voiced = false;
    float confidence = 0.0F;
    float periodicity = 1.0F;
    std::uint64_t repeats = 0;
    std::uint64_t deletes = 0;
};

class V1LabAudioProcessor final : public juce::AudioProcessor
{
public:
    V1LabAudioProcessor();
    ~V1LabAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    void processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destination) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    [[nodiscard]] DiagnosticSnapshot diagnosticSnapshot() const noexcept;
    [[nodiscard]] juce::AudioProcessorValueTreeState& parameters() noexcept { return parameters_; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void processInternal(juce::AudioBuffer<float>& buffer, bool hostBypassed) noexcept;
    void applyParameters(bool hostBypassed) noexcept;
    void publishDiagnostics() noexcept;
    void cacheParameterPointers();

    juce::AudioProcessorValueTreeState parameters_;
    v0a::PitchCorrectionEngine engine_;

    std::atomic<float>* key_ = nullptr;
    std::atomic<float>* scale_ = nullptr;
    std::atomic<float>* retune_ = nullptr;
    std::atomic<float>* humanize_ = nullptr;
    std::atomic<float>* voiceType_ = nullptr;
    std::atomic<float>* tracking_ = nullptr;
    std::atomic<float>* bypass_ = nullptr;
    std::array<std::atomic<float>*, 12> notes_ {};
    std::atomic<float>* interpolation_ = nullptr;
    std::atomic<float>* spliceCrossfade_ = nullptr;
    std::atomic<float>* trackingEpsilon_ = nullptr;
    std::atomic<float>* minimumF0_ = nullptr;
    std::atomic<float>* maximumF0_ = nullptr;
    std::atomic<float>* unvoicedMode_ = nullptr;
    std::atomic<float>* unvoicedDuration_ = nullptr;
    std::atomic<float>* targetHysteresis_ = nullptr;
    std::atomic<float>* confirmationPeriods_ = nullptr;
    std::atomic<float>* humanizeMode_ = nullptr;
    std::atomic<float>* humanizeOnset_ = nullptr;
    std::atomic<float>* humanizeRamp_ = nullptr;
    std::atomic<float>* humanizeAdditional_ = nullptr;
    std::atomic<float>* humanizeMultiplier_ = nullptr;

    int cachedKey_ = -1;
    int cachedScale_ = -1;
    int cachedVoiceType_ = -1;
    int cachedTracking_ = -1;
    int cachedInterpolation_ = -1;
    int cachedSpliceCrossfade_ = -1;
    int cachedUnvoicedMode_ = -1;
    int cachedConfirmationPeriods_ = -1;
    std::array<int, 12> cachedNotes_ {};
    float cachedMinimumF0_ = 65.0F;
    float cachedMaximumF0_ = 800.0F;
    float cachedTrackingEpsilon_ = 0.30F;
    float cachedUnvoicedDuration_ = -1.0F;
    float cachedTargetHysteresis_ = -1.0F;

    std::atomic<float> diagnosticF0_ { 0.0F };
    std::atomic<float> diagnosticMidi_ { 0.0F };
    std::atomic<int> diagnosticTarget_ { -1 };
    std::atomic<bool> diagnosticVoiced_ { false };
    std::atomic<float> diagnosticConfidence_ { 0.0F };
    std::atomic<float> diagnosticPeriodicity_ { 1.0F };
    std::atomic<std::uint64_t> diagnosticRepeats_ { 0 };
    std::atomic<std::uint64_t> diagnosticDeletes_ { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(V1LabAudioProcessor)
};
} // namespace v1lab
