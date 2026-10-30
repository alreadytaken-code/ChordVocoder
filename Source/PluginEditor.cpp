#include "PluginEditor.h"

VocoderEditor::VocoderEditor (VocoderAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    laf.setColourScheme (juce::LookAndFeel_V4::getMidnightColourScheme());
    setLookAndFeel (&laf);

    // preset selector
    presetLabel.setText ("Preset", juce::dontSendNotification);
    presetLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (presetLabel);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (juce::jmax (0, proc.getCurrentProgram()), juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);

    // chord / carrier section
    for (auto id : { "source", "root", "chordType", "wave", "bands" })
        addCombo (id);

    for (auto id : { "octave", "detune", "fmin", "fmax", "sharp", "formant",
                     "attack", "release", "sib", "inGain", "outGain", "mix" })
        addSlider (id);

    setSize (680, 430);
}

VocoderEditor::~VocoderEditor()
{
    setLookAndFeel (nullptr);
}

void VocoderEditor::addCombo (const juce::String& id)
{
    auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id));
    if (param == nullptr)
        return;

    auto c = std::make_unique<ComboCtl>();
    c->label.setText (param->getName (32), juce::dontSendNotification);
    c->label.setJustificationType (juce::Justification::centred);
    c->box.addItemList (param->choices, 1);
    addAndMakeVisible (c->label);
    addAndMakeVisible (c->box);
    c->att = std::make_unique<ComboAtt> (proc.apvts, id, c->box);
    combos.push_back (std::move (c));
}

void VocoderEditor::addSlider (const juce::String& id)
{
    auto* param = proc.apvts.getParameter (id);
    if (param == nullptr)
        return;

    auto s = std::make_unique<SliderCtl>();
    s->label.setText (param->getName (32), juce::dontSendNotification);
    s->label.setJustificationType (juce::Justification::centred);
    s->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    addAndMakeVisible (s->label);
    addAndMakeVisible (s->slider);
    s->att = std::make_unique<SliderAtt> (proc.apvts, id, s->slider);
    sliders.push_back (std::move (s));
}

void VocoderEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d23));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("Chord Vocoder", 16, 10, 250, 30, juce::Justification::centredLeft);
}

void VocoderEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    // top row: title on the left, preset menu on the right
    auto top = area.removeFromTop (30);
    presetBox.setBounds (top.removeFromRight (240));
    presetLabel.setBounds (top.removeFromRight (60));
    area.removeFromTop (14);

    // combo row
    auto comboRow = area.removeFromTop (60);
    if (! combos.empty())
    {
        const int w = comboRow.getWidth() / (int) combos.size();
        for (auto& c : combos)
        {
            auto cell = comboRow.removeFromLeft (w).reduced (6, 0);
            c->label.setBounds (cell.removeFromTop (20));
            c->box.setBounds (cell.removeFromTop (28));
        }
    }
    area.removeFromTop (12);

    // knob grid: 6 columns x 2 rows
    const int cols = 6;
    const int rows = ((int) sliders.size() + cols - 1) / cols;
    const int cellW = area.getWidth() / cols;
    const int cellH = area.getHeight() / juce::jmax (1, rows);

    for (int i = 0; i < (int) sliders.size(); ++i)
    {
        auto cell = juce::Rectangle<int> (area.getX() + (i % cols) * cellW,
                                          area.getY() + (i / cols) * cellH,
                                          cellW, cellH).reduced (4);
        sliders[(size_t) i]->label.setBounds (cell.removeFromTop (20));
        sliders[(size_t) i]->slider.setBounds (cell);
    }
}
