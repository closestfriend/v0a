#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>
#include <vector>

namespace v1lab
{
class V1LabAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit V1LabAudioProcessorEditor(V1LabAudioProcessor& processor);
    ~V1LabAudioProcessorEditor() override;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct LabelledControl
    {
        juce::Component* control = nullptr;
        std::unique_ptr<juce::Label> label;
    };

    void timerCallback() override;
    void configureCombo(juce::ComboBox& combo, const juce::StringArray& items,
                        const char* parameter, const juce::String& label, bool developer = false);
    void configureSlider(juce::Slider& slider, const char* parameter,
                         const juce::String& label, bool rotary, bool developer = false);
    void setChoiceParameter(const char* parameter, int choiceIndex);
    void syncPresetToNoteParameters();
    void cycleNote(std::size_t pitchClass);
    void refreshNoteButtons();
    void setDeveloperVisible(bool visible);
    [[nodiscard]] static int presetMode(int pitchClass, int key, int scale) noexcept;
    [[nodiscard]] static juce::String noteLabel(int midi);

    V1LabAudioProcessor& processor_;
    juce::LookAndFeel_V4 lookAndFeel_;

    juce::ComboBox keyBox_;
    juce::ComboBox scaleBox_;
    juce::ComboBox voiceBox_;
    juce::ComboBox trackingBox_;
    juce::Slider retuneSlider_;
    juce::Slider humanizeSlider_;
    juce::ToggleButton bypassButton_ { "BYPASS" };
    juce::TextButton developerButton_ { "DEVELOPER  +" };
    std::array<juce::TextButton, 12> noteButtons_;

    juce::Label diagnosticLabel_;
    juce::Label eventLabel_;

    juce::ComboBox interpolationBox_;
    juce::ComboBox unvoicedBox_;
    juce::ComboBox retuneDomainBox_;
    juce::ComboBox humanizeModeBox_;
    juce::Slider crossfadeSlider_;
    juce::Slider epsilonSlider_;
    juce::Slider minimumF0Slider_;
    juce::Slider maximumF0Slider_;
    juce::Slider unvoicedDurationSlider_;
    juce::Slider hysteresisSlider_;
    juce::Slider confirmationSlider_;
    juce::Slider humanizeOnsetSlider_;
    juce::Slider humanizeRampSlider_;
    juce::Slider humanizeAdditionalSlider_;
    juce::Slider humanizeMultiplierSlider_;

    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments_;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments_;
    std::unique_ptr<ButtonAttachment> bypassAttachment_;
    std::vector<LabelledControl> normalControls_;
    std::vector<LabelledControl> developerControls_;
    bool developerVisible_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(V1LabAudioProcessorEditor)
};
} // namespace v1lab
