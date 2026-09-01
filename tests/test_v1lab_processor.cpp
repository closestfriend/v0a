#include "TestHarness.h"
#include "Parameters.h"
#include "PluginProcessor.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <atomic>
#include <cstdlib>
#include <new>
#include <numbers>

namespace
{
std::atomic<bool> countAllocations { false };
std::atomic<std::size_t> allocationCount { 0 };
}

void* operator new(std::size_t size)
{
    if (countAllocations.load(std::memory_order_relaxed))
        allocationCount.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size))
        return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept
{
    std::free(memory);
}

namespace
{
void setParameter(v1lab::V1LabAudioProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters().getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void processSine(v1lab::V1LabAudioProcessor& processor, double frequency, int frames)
{
    juce::AudioBuffer<float> buffer(2, 128);
    juce::MidiBuffer midi;
    double phase = 0.0;
    int remaining = frames;
    while (remaining > 0)
    {
        const int count = std::min(remaining, buffer.getNumSamples());
        buffer.clear();
        for (int sample = 0; sample < count; ++sample)
        {
            phase += frequency / 48'000.0;
            phase -= std::floor(phase);
            const float value = static_cast<float>(0.7 * std::sin(2.0 * std::numbers::pi * phase));
            buffer.setSample(0, sample, value);
            buffer.setSample(1, sample, value);
        }
        processor.processBlock(buffer, midi);
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < count; ++sample)
                REQUIRE(std::isfinite(buffer.getSample(channel, sample)));
        remaining -= count;
    }
}
} // namespace

TEST_CASE("JUCE processor reports measured latency and automatically corrects a sine")
{
    v1lab::V1LabAudioProcessor processor;
    processor.prepareToPlay(48'000.0, 128);
    REQUIRE(processor.getLatencySamples() == 4'112);
    processSine(processor, 230.0, 96'000);
    const auto state = processor.diagnosticSnapshot();
    REQUIRE(state.voiced);
    REQUIRE_NEAR(state.inputF0, 230.0, 1.0);
    REQUIRE(state.targetMidi == 58);
}

TEST_CASE("JUCE Note Matrix parameters alter the target without a second DSP path")
{
    v1lab::V1LabAudioProcessor processor;
    processor.prepareToPlay(48'000.0, 128);
    setParameter(processor, v1lab::parameters::scale, 3.0F);
    for (const char* note : v1lab::parameters::notes)
        setParameter(processor, note, 2.0F);
    setParameter(processor, v1lab::parameters::notes[9], 0.0F);
    processSine(processor, 230.0, 48'000);
    REQUIRE(processor.diagnosticSnapshot().targetMidi == 57);

    setParameter(processor, v1lab::parameters::notes[9], 2.0F);
    setParameter(processor, v1lab::parameters::notes[10], 0.0F);
    processSine(processor, 230.0, 48'000);
    REQUIRE(processor.diagnosticSnapshot().targetMidi == 58);
}

TEST_CASE("plugin bypass retains the same impulse timeline")
{
    v1lab::V1LabAudioProcessor processor;
    processor.prepareToPlay(48'000.0, 1);
    setParameter(processor, v1lab::parameters::bypass, 1.0F);
    juce::AudioBuffer<float> buffer(2, 1);
    juce::MidiBuffer midi;
    int impulseOutput = -1;
    for (int index = 0; index <= processor.getLatencySamples() + 2; ++index)
    {
        const float input = index == 0 ? 1.0F : 0.0F;
        buffer.setSample(0, 0, input);
        buffer.setSample(1, 0, input);
        processor.processBlock(buffer, midi);
        if (std::abs(buffer.getSample(0, 0)) > 0.5F)
            impulseOutput = index;
    }
    REQUIRE(impulseOutput == processor.getLatencySamples());
}

TEST_CASE("a warmed processBlock performs no C++ heap allocations")
{
    v1lab::V1LabAudioProcessor processor;
    processor.prepareToPlay(48'000.0, 128);
    juce::AudioBuffer<float> buffer(2, 128);
    juce::MidiBuffer midi;
    buffer.clear();
    processor.processBlock(buffer, midi);

    allocationCount.store(0, std::memory_order_relaxed);
    countAllocations.store(true, std::memory_order_relaxed);
    processor.processBlock(buffer, midi);
    countAllocations.store(false, std::memory_order_relaxed);
    REQUIRE(allocationCount.load(std::memory_order_relaxed) == 0);
}

int main()
{
    return test::runAll();
}
