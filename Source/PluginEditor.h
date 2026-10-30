#pragma once

#include "PluginProcessor.h"

class VocoderEditor : public juce::AudioProcessorEditor
{
public:
    explicit VocoderEditor (VocoderAudioProcessor&);
    ~VocoderEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct SliderCtl
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<SliderAtt> att;
    };

    struct ComboCtl
    {
        juce::Label label;
        juce::ComboBox box;
        std::unique_ptr<ComboAtt> att;
    };

    void addCombo (const juce::String& id);
    void addSlider (const juce::String& id);

    VocoderAudioProcessor& proc;
    juce::LookAndFeel_V4 laf;

    juce::Label presetLabel;
    juce::ComboBox presetBox;

    std::vector<std::unique_ptr<ComboCtl>> combos;
    std::vector<std::unique_ptr<SliderCtl>> sliders;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocoderEditor)
};
