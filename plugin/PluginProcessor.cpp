#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "dsp/PitchMath.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace v1lab
{
namespace
{
juce::ParameterID parameterId(const char* id)
{
    return { id, 1 };
}

int choiceValue(const std::atomic<float>* value) noexcept
{
    return static_cast<int>(std::lround(value->load(std::memory_order_relaxed)));
}

float floatValue(const std::atomic<float>* value) noexcept
{
    return value->load(std::memory_order_relaxed);
}
} // namespace

V1LabAudioProcessor::V1LabAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters_(*this, nullptr, "V1LabParameters", createParameterLayout())
{
    cachedNotes_.fill(-1);
    cacheParameterPointers();
}

juce::AudioProcessorValueTreeState::ParameterLayout V1LabAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        parameterId(parameters::key), "Key",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        parameterId(parameters::scale), "Scale",
        juce::StringArray { "Chromatic", "Major", "Natural Minor", "Custom" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        parameterId(parameters::retune), "Retune Speed",
        juce::NormalisableRange<float>(0.0F, 400.0F, 0.1F), 0.0F, "ms"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        parameterId(parameters::humanize), "Humanize",
        juce::NormalisableRange<float>(0.0F, 100.0F, 0.1F), 0.0F, "%"));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        parameterId(parameters::voiceType), "Voice Type",
        juce::StringArray { "Soprano", "Alto / Tenor", "Low Male", "Instrument", "Bass" }, 1));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        parameterId(parameters::tracking), "Tracking",
        juce::StringArray { "Normal", "Choosy" }, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        parameterId(parameters::bypass), "Plugin Bypass", false));

    for (std::size_t index = 0; index < parameters::notes.size(); ++index)
    {
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            parameterId(parameters::notes[index]),
            juce::String(parameters::noteNames[index]) + " Mode",
            juce::StringArray { "Target", "Bypass", "Remove" }, 0));
    }

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        parameterId(parameters::interpolation), "Interpolation",
        juce::StringArray { "Linear", "Hermite", "Lagrange" }, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        parameterId(parameters::spliceCrossfade), "Splice Crossfade", 0, 16, 0, "samples"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        parameterId(parameters::trackingEpsilon), "Tracking Epsilon",
        juce::NormalisableRange<float>(0.05F, 0.50F, 0.001F), 0.30F));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        parameterId(parameters::minimumF0), "Detector Minimum F0",
        juce::NormalisableRange<float>(35.0F, 400.0F, 1.0F), 65.0F, "Hz"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        parameterId(parameters::maximumF0), "Detector Maximum F0",
        juce::NormalisableRange<float>(200.0F, 1200.0F, 1.0F), 800.0F, "Hz"));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        parameterId(parameters::unvoicedMode), "Unvoiced Mode",
        juce::StringArray { "Immediate Unity", "Hold Last", "Fade to Unity" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        parameterId(parameters::unvoicedDuration), "Unvoiced Hold / Fade",
        juce::NormalisableRange<float>(0.0F, 200.0F, 1.0F), 0.0F, "ms"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        parameterId(parameters::targetHysteresis), "Target Hysteresis",
        juce::NormalisableRange<float>(0.0F, 100.0F, 0.1F), 0.0F, "cents"));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        parameterId(parameters::confirmationPeriods), "Confirmation Periods", 0, 8, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        parameterId(parameters::humanizeMode), "Humanize Tau Mode",
        juce::StringArray { "Additive", "Multiplicative" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        parameterId(parameters::humanizeOnset), "Humanize Onset",
        juce::NormalisableRange<float>(0.0F, 2000.0F, 1.0F), 100.0F, "ms"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        parameterId(parameters::humanizeRamp), "Humanize Ramp",
        juce::NormalisableRange<float>(0.0F, 3000.0F, 1.0F), 500.0F, "ms"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        parameterId(parameters::humanizeAdditional), "Humanize Additional Tau",
        juce::NormalisableRange<float>(0.0F, 500.0F, 1.0F), 100.0F, "ms"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        parameterId(parameters::humanizeMultiplier), "Humanize Maximum Multiplier",
        juce::NormalisableRange<float>(1.0F, 10.0F, 0.1F), 4.0F, "x"));
    return layout;
}

void V1LabAudioProcessor::cacheParameterPointers()
{
    key_ = parameters_.getRawParameterValue(parameters::key);
    scale_ = parameters_.getRawParameterValue(parameters::scale);
    retune_ = parameters_.getRawParameterValue(parameters::retune);
    humanize_ = parameters_.getRawParameterValue(parameters::humanize);
    voiceType_ = parameters_.getRawParameterValue(parameters::voiceType);
    tracking_ = parameters_.getRawParameterValue(parameters::tracking);
    bypass_ = parameters_.getRawParameterValue(parameters::bypass);
    for (std::size_t index = 0; index < notes_.size(); ++index)
        notes_[index] = parameters_.getRawParameterValue(parameters::notes[index]);
    interpolation_ = parameters_.getRawParameterValue(parameters::interpolation);
    spliceCrossfade_ = parameters_.getRawParameterValue(parameters::spliceCrossfade);
    trackingEpsilon_ = parameters_.getRawParameterValue(parameters::trackingEpsilon);
    minimumF0_ = parameters_.getRawParameterValue(parameters::minimumF0);
    maximumF0_ = parameters_.getRawParameterValue(parameters::maximumF0);
    unvoicedMode_ = parameters_.getRawParameterValue(parameters::unvoicedMode);
    unvoicedDuration_ = parameters_.getRawParameterValue(parameters::unvoicedDuration);
    targetHysteresis_ = parameters_.getRawParameterValue(parameters::targetHysteresis);
    confirmationPeriods_ = parameters_.getRawParameterValue(parameters::confirmationPeriods);
    humanizeMode_ = parameters_.getRawParameterValue(parameters::humanizeMode);
    humanizeOnset_ = parameters_.getRawParameterValue(parameters::humanizeOnset);
    humanizeRamp_ = parameters_.getRawParameterValue(parameters::humanizeRamp);
    humanizeAdditional_ = parameters_.getRawParameterValue(parameters::humanizeAdditional);
    humanizeMultiplier_ = parameters_.getRawParameterValue(parameters::humanizeMultiplier);
}

void V1LabAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    v0a::PitchCorrectionConfig config;
    config.sampleRate = sampleRate;
    config.minimumF0 = 35.0;
    config.maximumF0 = 1200.0;
    config.retuneMilliseconds = 0.0;
    config.humanizeAmount = 0.0;
    engine_.prepare(config);
    setLatencySamples(static_cast<int>(engine_.latencySamples()));

    cachedKey_ = -1;
    cachedScale_ = -1;
    cachedVoiceType_ = -1;
    cachedTracking_ = -1;
    cachedInterpolation_ = -1;
    cachedSpliceCrossfade_ = -1;
    cachedUnvoicedMode_ = -1;
    cachedConfirmationPeriods_ = -1;
    cachedNotes_.fill(-1);
    cachedMinimumF0_ = floatValue(minimumF0_);
    cachedMaximumF0_ = floatValue(maximumF0_);
    cachedTrackingEpsilon_ = floatValue(trackingEpsilon_);
    cachedUnvoicedDuration_ = -1.0F;
    cachedTargetHysteresis_ = -1.0F;
    applyParameters(false);
}

void V1LabAudioProcessor::releaseResources()
{
}

bool V1LabAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo())
        && output == layouts.getMainInputChannelSet();
}

juce::AudioProcessorEditor* V1LabAudioProcessor::createEditor()
{
    return new V1LabAudioProcessorEditor(*this);
}

void V1LabAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    processInternal(buffer, false);
}

void V1LabAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    processInternal(buffer, true);
}

void V1LabAudioProcessor::processInternal(juce::AudioBuffer<float>& buffer,
                                          bool hostBypassed) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    applyParameters(hostBypassed);
    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();
    if (channels == 0)
        return;
    float* left = buffer.getWritePointer(0);
    for (int sample = 0; sample < samples; ++sample)
    {
        const float output = engine_.processSample(left[sample]);
        for (int channel = 0; channel < channels; ++channel)
            buffer.getWritePointer(channel)[sample] = output;
    }
    publishDiagnostics();
}

void V1LabAudioProcessor::applyParameters(bool hostBypassed) noexcept
{
    const int key = choiceValue(key_);
    const int scale = choiceValue(scale_);
    if (key != cachedKey_ || scale != cachedScale_)
    {
        if (scale >= 0 && scale < 3)
            engine_.loadScale(key, static_cast<v0a::ScalePreset>(scale));
        cachedKey_ = key;
        cachedScale_ = scale;
    }
    if (scale == 3)
    {
        for (std::size_t index = 0; index < notes_.size(); ++index)
        {
            const int mode = choiceValue(notes_[index]);
            if (mode != cachedNotes_[index])
            {
                engine_.setNoteMode(static_cast<int>(index), static_cast<v0a::NoteMode>(mode));
                cachedNotes_[index] = mode;
            }
        }
    }

    engine_.setRetuneMilliseconds(floatValue(retune_));
    engine_.setHumanizeAmount(0.01 * floatValue(humanize_));
    engine_.setHumanizeMode(choiceValue(humanizeMode_) == 0
        ? v0a::HumanizeTauMode::Additive : v0a::HumanizeTauMode::Multiplicative);
    engine_.setHumanizeTiming(floatValue(humanizeOnset_), floatValue(humanizeRamp_));
    engine_.setHumanizeMaximumAdditionalMilliseconds(floatValue(humanizeAdditional_));
    engine_.setHumanizeMaximumMultiplier(floatValue(humanizeMultiplier_));

    const int voice = choiceValue(voiceType_);
    if (voice != cachedVoiceType_)
    {
        constexpr std::array<double, 5> low { 120.0, 65.0, 45.0, 50.0, 35.0 };
        constexpr std::array<double, 5> high { 1200.0, 800.0, 500.0, 1200.0, 350.0 };
        const auto index = static_cast<std::size_t>(std::clamp(voice, 0, 4));
        static_cast<void>(engine_.setDetectorFrequencyBounds(low[index], high[index]));
        cachedVoiceType_ = voice;
    }
    const float minimumF0 = floatValue(minimumF0_);
    const float maximumF0 = floatValue(maximumF0_);
    if (minimumF0 != cachedMinimumF0_ || maximumF0 != cachedMaximumF0_)
    {
        static_cast<void>(engine_.setDetectorFrequencyBounds(minimumF0, maximumF0));
        cachedMinimumF0_ = minimumF0;
        cachedMaximumF0_ = maximumF0;
    }

    const int tracking = choiceValue(tracking_);
    if (tracking != cachedTracking_)
    {
        static_cast<void>(engine_.setTrackingEpsilon(tracking == 0 ? 0.30 : 0.18));
        cachedTracking_ = tracking;
    }
    const float epsilon = floatValue(trackingEpsilon_);
    if (epsilon != cachedTrackingEpsilon_)
    {
        static_cast<void>(engine_.setTrackingEpsilon(epsilon));
        cachedTrackingEpsilon_ = epsilon;
    }

    const int interpolation = choiceValue(interpolation_);
    if (interpolation != cachedInterpolation_)
    {
        static_cast<void>(engine_.setInterpolation(static_cast<v0a::InterpolationMethod>(interpolation)));
        cachedInterpolation_ = interpolation;
    }
    const int crossfade = choiceValue(spliceCrossfade_);
    if (crossfade != cachedSpliceCrossfade_)
    {
        static_cast<void>(engine_.setSpliceCrossfadeSamples(static_cast<std::size_t>(crossfade)));
        cachedSpliceCrossfade_ = crossfade;
    }

    const int unvoicedMode = choiceValue(unvoicedMode_);
    const float unvoicedDuration = floatValue(unvoicedDuration_);
    if (unvoicedMode != cachedUnvoicedMode_ || unvoicedDuration != cachedUnvoicedDuration_)
    {
        engine_.setUnvoicedMode(static_cast<v0a::UnvoicedMode>(unvoicedMode), unvoicedDuration);
        cachedUnvoicedMode_ = unvoicedMode;
        cachedUnvoicedDuration_ = unvoicedDuration;
    }

    const float hysteresis = floatValue(targetHysteresis_);
    const int confirmation = choiceValue(confirmationPeriods_);
    if (hysteresis != cachedTargetHysteresis_ || confirmation != cachedConfirmationPeriods_)
    {
        engine_.setTargetPersistence(hysteresis, confirmation);
        cachedTargetHysteresis_ = hysteresis;
        cachedConfirmationPeriods_ = confirmation;
    }
    engine_.setBypassed(hostBypassed || floatValue(bypass_) >= 0.5F);
}

void V1LabAudioProcessor::publishDiagnostics() noexcept
{
    const auto& state = engine_.diagnostics();
    diagnosticF0_.store(static_cast<float>(state.detection.frequency), std::memory_order_relaxed);
    diagnosticMidi_.store(state.detection.voiced
        ? static_cast<float>(v0a::frequencyToMidi(state.detection.frequency)) : 0.0F,
        std::memory_order_relaxed);
    diagnosticTarget_.store(state.detection.voiced && state.quantization.hasTarget
        ? state.quantization.targetMidi : -1, std::memory_order_relaxed);
    diagnosticVoiced_.store(state.detection.voiced, std::memory_order_relaxed);
    diagnosticConfidence_.store(static_cast<float>(state.detection.confidence),
                                std::memory_order_relaxed);
    diagnosticPeriodicity_.store(static_cast<float>(state.detection.periodicity),
                                 std::memory_order_relaxed);
    diagnosticRepeats_.store(state.resamplerStats.repeatedCycles, std::memory_order_relaxed);
    diagnosticDeletes_.store(state.resamplerStats.deletedCycles, std::memory_order_relaxed);
}

DiagnosticSnapshot V1LabAudioProcessor::diagnosticSnapshot() const noexcept
{
    return {
        .inputF0 = diagnosticF0_.load(std::memory_order_relaxed),
        .detectedMidi = diagnosticMidi_.load(std::memory_order_relaxed),
        .targetMidi = diagnosticTarget_.load(std::memory_order_relaxed),
        .voiced = diagnosticVoiced_.load(std::memory_order_relaxed),
        .confidence = diagnosticConfidence_.load(std::memory_order_relaxed),
        .periodicity = diagnosticPeriodicity_.load(std::memory_order_relaxed),
        .repeats = diagnosticRepeats_.load(std::memory_order_relaxed),
        .deletes = diagnosticDeletes_.load(std::memory_order_relaxed),
    };
}

juce::AudioProcessorParameter* V1LabAudioProcessor::getBypassParameter() const
{
    return parameters_.getParameter(parameters::bypass);
}

void V1LabAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    if (const auto xml = parameters_.copyState().createXml())
        copyXmlToBinary(*xml, destination);
}

void V1LabAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
        parameters_.replaceState(juce::ValueTree::fromXml(*xml));
}

} // namespace v1lab

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new v1lab::V1LabAudioProcessor();
}
