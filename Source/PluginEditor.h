#pragma once
#include "PluginProcessor.h"

class ExtuneLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ExtuneLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isDown,
                       int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
};

class ExtuneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ExtuneEditor (ExtuneProcessor&);
    ~ExtuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }
    void stepPreset (int delta);

    ExtuneProcessor& proc;
    ExtuneLookAndFeel laf;

    juce::ComboBox presetBox, keyBox, scaleBox;
    juce::ArrowButton prevBtn { "prev", 0.5f, juce::Colour (0xffd8d8de) };
    juce::ArrowButton nextBtn { "next", 0.0f, juce::Colour (0xffd8d8de) };
    juce::Slider speedSlider, amountSlider, humanizeSlider, mixSlider;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<ComboAtt>  keyAtt, scaleAtt;
    std::unique_ptr<SliderAtt> speedAtt, amountAtt, humanizeAtt, mixAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExtuneEditor)
};
