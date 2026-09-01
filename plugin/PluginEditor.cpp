#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace v1lab
{
namespace
{
const juce::Colour background { 0xff11151a };
const juce::Colour panel { 0xff1b222a };
const juce::Colour panelEdge { 0xff303b46 };
const juce::Colour text { 0xffe5ebf0 };
const juce::Colour muted { 0xff8d9aa6 };
const juce::Colour accent { 0xff2f9dff };
const juce::Colour bypassColour { 0xff65727e };
const juce::Colour removedColour { 0xff252c33 };
}

V1LabAudioProcessorEditor::V1LabAudioProcessorEditor(V1LabAudioProcessor& processor)
    : AudioProcessorEditor(&processor), processor_(processor)
{
    lookAndFeel_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff202831));
    lookAndFeel_.setColour(juce::ComboBox::textColourId, text);
    lookAndFeel_.setColour(juce::ComboBox::outlineColourId, panelEdge);
    lookAndFeel_.setColour(juce::Slider::thumbColourId, accent);
    lookAndFeel_.setColour(juce::Slider::rotarySliderFillColourId, accent);
    lookAndFeel_.setColour(juce::Slider::rotarySliderOutlineColourId, panelEdge);
    lookAndFeel_.setColour(juce::Slider::textBoxTextColourId, text);
    lookAndFeel_.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff171c22));
    lookAndFeel_.setColour(juce::Slider::textBoxOutlineColourId, panelEdge);
    setLookAndFeel(&lookAndFeel_);

    configureCombo(keyBox_, { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" },
                   parameters::key, "KEY");
    configureCombo(scaleBox_, { "Chromatic", "Major", "Natural Minor", "Custom" },
                   parameters::scale, "SCALE");
    configureCombo(voiceBox_, { "Soprano", "Alto / Tenor", "Low Male", "Instrument", "Bass" },
                   parameters::voiceType, "VOICE TYPE");
    configureCombo(trackingBox_, { "Normal", "Choosy" }, parameters::tracking, "TRACKING");
    configureSlider(retuneSlider_, parameters::retune, "RETUNE SPEED", true);
    configureSlider(humanizeSlider_, parameters::humanize, "HUMANIZE", true);

    addAndMakeVisible(bypassButton_);
    bypassButton_.setColour(juce::ToggleButton::textColourId, text);
    bypassAttachment_ = std::make_unique<ButtonAttachment>(
        processor_.parameters(), parameters::bypass, bypassButton_);

    for (std::size_t index = 0; index < noteButtons_.size(); ++index)
    {
        auto& button = noteButtons_[index];
        button.setButtonText(parameters::noteNames[index]);
        button.setColour(juce::TextButton::textColourOffId, text);
        button.onClick = [this, index] { cycleNote(index); };
        addAndMakeVisible(button);
    }

    diagnosticLabel_.setColour(juce::Label::textColourId, text);
    diagnosticLabel_.setFont(juce::Font(15.0F, juce::Font::plain));
    diagnosticLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(diagnosticLabel_);
    eventLabel_.setColour(juce::Label::textColourId, muted);
    eventLabel_.setFont(juce::Font(12.0F, juce::Font::plain));
    eventLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(eventLabel_);

    developerButton_.setColour(juce::TextButton::buttonColourId, panel);
    developerButton_.setColour(juce::TextButton::textColourOffId, muted);
    developerButton_.onClick = [this] { setDeveloperVisible(!developerVisible_); };
    addAndMakeVisible(developerButton_);

    configureCombo(interpolationBox_, { "Linear", "Hermite", "Lagrange" },
                   parameters::interpolation, "INTERPOLATION", true);
    configureSlider(crossfadeSlider_, parameters::spliceCrossfade, "SPLICE CROSSFADE", false, true);
    configureSlider(epsilonSlider_, parameters::trackingEpsilon, "TRACKING EPSILON", false, true);
    configureSlider(minimumF0Slider_, parameters::minimumF0, "DETECTOR MIN F0", false, true);
    configureSlider(maximumF0Slider_, parameters::maximumF0, "DETECTOR MAX F0", false, true);
    configureCombo(unvoicedBox_, { "Immediate Unity", "Hold Last", "Fade to Unity" },
                   parameters::unvoicedMode, "UNVOICED MODE", true);
    configureSlider(unvoicedDurationSlider_, parameters::unvoicedDuration,
                    "UNVOICED HOLD / FADE", false, true);
    configureSlider(hysteresisSlider_, parameters::targetHysteresis,
                    "TARGET HYSTERESIS", false, true);
    configureSlider(confirmationSlider_, parameters::confirmationPeriods,
                    "CONFIRMATION PERIODS", false, true);
    retuneDomainBox_.addItem("Period Ratio (baseline)", 1);
    retuneDomainBox_.setSelectedId(1, juce::dontSendNotification);
    retuneDomainBox_.setEnabled(false);
    addAndMakeVisible(retuneDomainBox_);
    auto retuneDomainLabel = std::make_unique<juce::Label>();
    retuneDomainLabel->setText("RETUNE DOMAIN", juce::dontSendNotification);
    retuneDomainLabel->setColour(juce::Label::textColourId, muted);
    retuneDomainLabel->setFont(juce::Font(10.0F, juce::Font::bold));
    addAndMakeVisible(*retuneDomainLabel);
    developerControls_.push_back({ &retuneDomainBox_, std::move(retuneDomainLabel) });
    configureCombo(humanizeModeBox_, { "Additive", "Multiplicative" }, parameters::humanizeMode,
                   "HUMANIZE TAU MODE", true);
    configureSlider(humanizeOnsetSlider_, parameters::humanizeOnset, "HUMANIZE ONSET", false, true);
    configureSlider(humanizeRampSlider_, parameters::humanizeRamp, "HUMANIZE RAMP", false, true);
    configureSlider(humanizeAdditionalSlider_, parameters::humanizeAdditional,
                    "HUMANIZE ADDITIONAL TAU", false, true);
    configureSlider(humanizeMultiplierSlider_, parameters::humanizeMultiplier,
                    "HUMANIZE MAX MULTIPLIER", false, true);

    scaleBox_.onChange = [this] {
        if (scaleBox_.getSelectedItemIndex() >= 0 && scaleBox_.getSelectedItemIndex() < 3)
            syncPresetToNoteParameters();
    };
    setDeveloperVisible(false);
    refreshNoteButtons();
    startTimerHz(20);
}

V1LabAudioProcessorEditor::~V1LabAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void V1LabAudioProcessorEditor::configureCombo(juce::ComboBox& combo,
                                               const juce::StringArray& items,
                                               const char* parameter,
                                               const juce::String& labelText,
                                               bool developer)
{
    combo.addItemList(items, 1);
    combo.setColour(juce::ComboBox::textColourId, text);
    addAndMakeVisible(combo);
    comboAttachments_.push_back(std::make_unique<ComboAttachment>(
        processor_.parameters(), parameter, combo));
    auto label = std::make_unique<juce::Label>();
    label->setText(labelText, juce::dontSendNotification);
    label->setColour(juce::Label::textColourId, muted);
    label->setFont(juce::Font(10.0F, juce::Font::bold));
    addAndMakeVisible(*label);
    auto& collection = developer ? developerControls_ : normalControls_;
    collection.push_back({ &combo, std::move(label) });
}

void V1LabAudioProcessorEditor::configureSlider(juce::Slider& slider,
                                                const char* parameter,
                                                const juce::String& labelText,
                                                bool rotary,
                                                bool developer)
{
    slider.setSliderStyle(rotary ? juce::Slider::RotaryHorizontalVerticalDrag
                                 : juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(rotary ? juce::Slider::TextBoxBelow : juce::Slider::TextBoxRight,
                           false, rotary ? 74 : 64, 20);
    addAndMakeVisible(slider);
    sliderAttachments_.push_back(std::make_unique<SliderAttachment>(
        processor_.parameters(), parameter, slider));
    auto label = std::make_unique<juce::Label>();
    label->setText(labelText, juce::dontSendNotification);
    label->setColour(juce::Label::textColourId, muted);
    label->setFont(juce::Font(10.0F, juce::Font::bold));
    label->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(*label);
    auto& collection = developer ? developerControls_ : normalControls_;
    collection.push_back({ &slider, std::move(label) });
}

void V1LabAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(background);
    graphics.setColour(panel);
    graphics.fillRoundedRectangle(18.0F, 68.0F, 724.0F, 142.0F, 8.0F);
    graphics.fillRoundedRectangle(18.0F, 222.0F, 724.0F, 102.0F, 8.0F);
    graphics.fillRoundedRectangle(18.0F, 336.0F, 724.0F, 72.0F, 8.0F);
    if (developerVisible_)
        graphics.fillRoundedRectangle(18.0F, 456.0F, 724.0F, 284.0F, 8.0F);

    graphics.setColour(text);
    graphics.setFont(juce::Font(25.0F, juce::Font::bold));
    graphics.drawText("V1", 20, 16, 38, 34, juce::Justification::centredLeft);
    graphics.setColour(accent);
    graphics.drawText("—LAB", 56, 16, 90, 34, juce::Justification::centredLeft);
    graphics.setColour(muted);
    graphics.setFont(juce::Font(11.0F, juce::Font::plain));
    graphics.drawText("MONOPHONIC CYCLE CORRECTION / EXPERIMENT BUILD",
                      154, 18, 400, 30, juce::Justification::centredLeft);
    graphics.setFont(juce::Font(10.0F, juce::Font::bold));
    graphics.drawText("NOTE MATRIX  ·  TARGET / BYPASS / REMOVE", 28, 226, 360, 18,
                      juce::Justification::centredLeft);
    graphics.drawText("LIVE DIAGNOSTICS", 28, 340, 180, 18, juce::Justification::centredLeft);
    if (developerVisible_)
        graphics.drawText("RECONSTRUCTION VARIABLES  ·  EXPERIMENTAL UNLESS NOTED",
                          28, 460, 430, 18, juce::Justification::centredLeft);
}

void V1LabAudioProcessorEditor::resized()
{
    constexpr int margin = 28;
    constexpr int comboWidth = 126;
    int x = margin;
    for (std::size_t index = 0; index < 4; ++index)
    {
        auto& item = normalControls_[index];
        item.label->setBounds(x, 76, comboWidth, 16);
        item.control->setBounds(x, 94, comboWidth, 30);
        x += comboWidth + 12;
    }
    normalControls_[4].label->setBounds(586, 74, 70, 18);
    normalControls_[4].control->setBounds(578, 90, 88, 104);
    normalControls_[5].label->setBounds(666, 74, 70, 18);
    normalControls_[5].control->setBounds(658, 90, 78, 104);
    bypassButton_.setBounds(28, 150, 126, 34);

    const int noteY = 250;
    const int noteWidth = 54;
    for (std::size_t index = 0; index < noteButtons_.size(); ++index)
        noteButtons_[index].setBounds(29 + static_cast<int>(index) * 58, noteY, noteWidth, 54);

    diagnosticLabel_.setBounds(30, 358, 525, 40);
    eventLabel_.setBounds(550, 358, 178, 40);
    developerButton_.setBounds(18, 420, 724, 28);

    if (developerVisible_)
    {
        constexpr int columns = 4;
        constexpr int itemWidth = 165;
        constexpr int itemHeight = 62;
        for (std::size_t index = 0; index < developerControls_.size(); ++index)
        {
            const int column = static_cast<int>(index) % columns;
            const int row = static_cast<int>(index) / columns;
            const int itemX = 30 + column * 176;
            const int itemY = 482 + row * itemHeight;
            auto& item = developerControls_[index];
            item.label->setBounds(itemX, itemY, itemWidth, 16);
            item.control->setBounds(itemX, itemY + 18, itemWidth, 32);
        }
    }
}

void V1LabAudioProcessorEditor::timerCallback()
{
    const auto state = processor_.diagnosticSnapshot();
    const juce::String voiced = state.voiced ? "VOICED" : "UNVOICED";
    const juce::String inputNote = state.voiced
        ? noteLabel(static_cast<int>(std::lround(state.detectedMidi))) : "—";
    const juce::String targetNote = state.targetMidi >= 0 ? noteLabel(state.targetMidi) : "—";
    diagnosticLabel_.setText(
        voiced + "    IN  " + juce::String(state.inputF0, 2) + " Hz  /  " + inputNote
        + "    →    TARGET  " + targetNote
        + "    CONF  " + juce::String(state.confidence, 3)
        + "    PER  " + juce::String(state.periodicity, 3),
        juce::dontSendNotification);
    diagnosticLabel_.setColour(juce::Label::textColourId, state.voiced ? text : muted);
    eventLabel_.setText("REPEAT " + juce::String(state.repeats)
                        + "    DELETE " + juce::String(state.deletes),
                        juce::dontSendNotification);
    refreshNoteButtons();
}

void V1LabAudioProcessorEditor::setChoiceParameter(const char* parameter, int choiceIndex)
{
    if (auto* ranged = processor_.parameters().getParameter(parameter))
    {
        ranged->beginChangeGesture();
        ranged->setValueNotifyingHost(ranged->convertTo0to1(static_cast<float>(choiceIndex)));
        ranged->endChangeGesture();
    }
}

void V1LabAudioProcessorEditor::syncPresetToNoteParameters()
{
    const int key = keyBox_.getSelectedItemIndex();
    const int scale = scaleBox_.getSelectedItemIndex();
    if (key < 0 || scale < 0 || scale >= 3)
        return;
    for (int pitchClass = 0; pitchClass < 12; ++pitchClass)
        setChoiceParameter(parameters::notes[static_cast<std::size_t>(pitchClass)],
                           presetMode(pitchClass, key, scale));
}

void V1LabAudioProcessorEditor::cycleNote(std::size_t pitchClass)
{
    int current = 0;
    if (scaleBox_.getSelectedItemIndex() != 3)
    {
        syncPresetToNoteParameters();
        current = presetMode(static_cast<int>(pitchClass), keyBox_.getSelectedItemIndex(),
                             scaleBox_.getSelectedItemIndex());
    }
    else if (const auto* value = processor_.parameters().getRawParameterValue(
                 parameters::notes[pitchClass]))
    {
        current = static_cast<int>(std::lround(value->load()));
    }
    setChoiceParameter(parameters::notes[pitchClass], (current + 1) % 3);
    setChoiceParameter(parameters::scale, 3);
    refreshNoteButtons();
}

void V1LabAudioProcessorEditor::refreshNoteButtons()
{
    for (std::size_t index = 0; index < noteButtons_.size(); ++index)
    {
        const auto* value = processor_.parameters().getRawParameterValue(parameters::notes[index]);
        const int mode = value != nullptr ? static_cast<int>(std::lround(value->load())) : 2;
        auto& button = noteButtons_[index];
        button.setColour(juce::TextButton::buttonColourId,
                         mode == 0 ? accent : mode == 1 ? bypassColour : removedColour);
        const juce::String suffix = mode == 0 ? "\nT" : mode == 1 ? "\nB" : "\n—";
        button.setButtonText(juce::String(parameters::noteNames[index]) + suffix);
    }
}

void V1LabAudioProcessorEditor::setDeveloperVisible(bool visible)
{
    developerVisible_ = visible;
    developerButton_.setButtonText(visible ? "DEVELOPER  −" : "DEVELOPER  +");
    for (auto& item : developerControls_)
    {
        item.control->setVisible(visible);
        item.label->setVisible(visible);
    }
    setSize(760, visible ? 754 : 460);
    resized();
    repaint();
}

int V1LabAudioProcessorEditor::presetMode(int pitchClass, int key, int scale) noexcept
{
    if (scale == 0)
        return 0;
    constexpr std::array<int, 7> major { 0, 2, 4, 5, 7, 9, 11 };
    constexpr std::array<int, 7> minor { 0, 2, 3, 5, 7, 8, 10 };
    const auto& intervals = scale == 2 ? minor : major;
    const int relative = (pitchClass - key + 12) % 12;
    return std::find(intervals.begin(), intervals.end(), relative) != intervals.end() ? 0 : 2;
}

juce::String V1LabAudioProcessorEditor::noteLabel(int midi)
{
    const int pitchClass = ((midi % 12) + 12) % 12;
    const int octave = static_cast<int>(std::floor(static_cast<double>(midi) / 12.0)) - 1;
    return juce::String(parameters::noteNames[static_cast<std::size_t>(pitchClass)])
         + juce::String(octave);
}
} // namespace v1lab
