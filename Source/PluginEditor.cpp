#include "PluginEditor.h"

namespace
{
    const juce::Colour bgCol  { 0xff050507 };
    const juce::Colour panel  { 0xff0a0a0d };
    const juce::Colour lineC  { 0xff1a1a20 };
    const juce::Colour txt    { 0xffd8d8de };
    const juce::Colour dimTxt { 0xff9a9aa6 };
    const juce::Colour purple { 0xff8b3dff };

    void drawSpaced (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r,
                     juce::Justification j, float size, float kern, juce::Colour c)
    {
        g.setColour (c);
        g.setFont (juce::Font (juce::FontOptions (size).withKerningFactor (kern)));
        g.drawText (t, r, j, false);
    }

    void glowArc (juce::Graphics& g, const juce::Path& p, float th)
    {
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (purple.withAlpha (0.07f));
            g.strokePath (p, juce::PathStrokeType (th + (float) i * 3.f,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        }
        g.setColour (purple);
        g.strokePath (p, juce::PathStrokeType (th, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }
}

// ---------------------------------------------------------------------------
ExtuneLookAndFeel::ExtuneLookAndFeel()
{
    setColour (juce::ComboBox::textColourId, txt);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0e0e12));
    setColour (juce::PopupMenu::textColourId, txt);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3a1d78));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void ExtuneLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                          float pos, float startAngle, float endAngle,
                                          juce::Slider&)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = b.getCentre();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // outer ring
    g.setColour (juce::Colour (0xff17171d));
    g.drawEllipse (c.x - R, c.y - R, 2.f * R, 2.f * R, 1.f);

    // arc track + glowing value arc
    const float ar = R * 0.86f;
    const float th = juce::jmax (2.5f, R * 0.075f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, ar, ar, 0.f, startAngle, endAngle, true);
    g.setColour (purple.withAlpha (0.13f));
    g.strokePath (track, juce::PathStrokeType (th, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (c.x, c.y, ar, ar, 0.f, startAngle, angle, true);
    glowArc (g, val, th);

    // knob body
    const float br = R * 0.68f;
    juce::ColourGradient grad (juce::Colour (0xff2a2a31), c.x - br * 0.5f, c.y - br * 0.8f,
                               juce::Colour (0xff050506), c.x + br * 0.6f, c.y + br * 0.9f, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - br, c.y - br, 2.f * br, 2.f * br);
    g.setColour (juce::Colour (0x18ffffff));
    g.drawEllipse (c.x - br, c.y - br, 2.f * br, 2.f * br, 1.f);

    // indicator
    const auto p1 = c.getPointOnCircumference (br * 0.74f, angle);
    const auto p2 = c.getPointOnCircumference (br * 0.97f, angle);
    g.setColour (purple.brighter (0.2f));
    g.drawLine (p1.x, p1.y, p2.x, p2.y, juce::jmax (2.f, R * 0.045f));
}

void ExtuneLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool,
                                      int, int, int, int, juce::ComboBox&)
{
    const auto r = juce::Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (0.5f);
    g.setColour (juce::Colour (0xff0d0d11));
    g.fillRoundedRectangle (r, 11.f);
    g.setColour (juce::Colour (0xff25252d));
    g.drawRoundedRectangle (r, 11.f, 1.f);

    const float cx = (float) w - 20.f, cy = (float) h * 0.5f;
    juce::Path chev;
    chev.startNewSubPath (cx - 5.f, cy - 2.5f);
    chev.lineTo (cx, cy + 2.5f);
    chev.lineTo (cx + 5.f, cy - 2.5f);
    g.setColour (txt);
    g.strokePath (chev, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

void ExtuneLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (14, 1, box.getWidth() - 42, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font ExtuneLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (15.f));
}

// ---------------------------------------------------------------------------
ExtuneEditor::ExtuneEditor (ExtuneProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    presetBox.addItemList ({ "Default", "Natural", "Hard Tune", "Subtle" }, 1);
    presetBox.setSelectedId (1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.applyPreset (presetBox.getSelectedId() - 1); };

    keyBox.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    scaleBox.addItemList ({ "Chromatic", "Major", "Minor" }, 1);

    for (auto* s : { &speedSlider, &amountSlider, &humanizeSlider, &mixSlider })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                juce::MathConstants<float>::pi * 2.8f, true);
    }

    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (+1); };

    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &keyBox, &scaleBox,
                                                              &prevBtn, &nextBtn,
                                                              &speedSlider, &amountSlider,
                                                              &humanizeSlider, &mixSlider })
        addAndMakeVisible (c);

    keyAtt      = std::make_unique<ComboAtt>  (proc.apvts, "key",      keyBox);
    scaleAtt    = std::make_unique<ComboAtt>  (proc.apvts, "scale",    scaleBox);
    speedAtt    = std::make_unique<SliderAtt> (proc.apvts, "speed",    speedSlider);
    amountAtt   = std::make_unique<SliderAtt> (proc.apvts, "amount",   amountSlider);
    humanizeAtt = std::make_unique<SliderAtt> (proc.apvts, "humanize", humanizeSlider);
    mixAtt      = std::make_unique<SliderAtt> (proc.apvts, "mix",      mixSlider);

    setSize (768, 512);
    startTimerHz (30);
}

ExtuneEditor::~ExtuneEditor()
{
    setLookAndFeel (nullptr);
}

void ExtuneEditor::stepPreset (int delta)
{
    const int n = presetBox.getNumItems();
    const int cur = presetBox.getSelectedId() - 1;
    presetBox.setSelectedId (((cur + delta + n) % n) + 1, juce::sendNotificationSync);
}

void ExtuneEditor::resized()
{
    presetBox.setBounds (526, 31, 111, 32);
    prevBtn.setBounds (652, 39, 14, 16);
    nextBtn.setBounds (680, 39, 14, 16);

    speedSlider.setBounds (73, 166, 112, 112);
    amountSlider.setBounds (295, 128, 180, 180);
    humanizeSlider.setBounds (584, 166, 112, 112);

    keyBox.setBounds (47, 388, 110, 34);
    scaleBox.setBounds (178, 388, 101, 34);
    mixSlider.setBounds (600, 374, 72, 72);
}

void ExtuneEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgCol);

    // panel
    const auto panelR = juce::Rectangle<float> (14.f, 14.f, 740.f, 484.f);
    g.setColour (panel);
    g.fillRoundedRectangle (panelR, 26.f);
    g.setColour (juce::Colour (0xff1c1230));
    g.drawRoundedRectangle (panelR, 26.f, 1.5f);

    // dividers
    g.setColour (lineC);
    g.fillRect (24, 79, 720, 1);
    g.fillRect (24, 338, 720, 1);
    g.fillRect (234, 130, 1, 172);
    g.fillRect (535, 130, 1, 172);
    g.fillRect (580, 358, 1, 88);
    g.fillRect (695, 358, 1, 88);

    // title: E X T U N E (X in purple)
    {
        const juce::String letters = "EXTUNE";
        for (int i = 0; i < letters.length(); ++i)
        {
            const bool isX = (i == 1);
            g.setColour (isX ? purple : juce::Colour (0xffb9b9c4));
            g.setFont (juce::Font (juce::FontOptions (32.f).withHorizontalScale (1.35f)));
            g.drawText (letters.substring (i, i + 1), 47 + i * 31, 30, 31, 36,
                        juce::Justification::centred, false);
        }
    }

    // presets header
    drawSpaced (g, "PRESETS", { 440, 34, 74, 24 }, juce::Justification::centredRight, 12.f, 0.25f, dimTxt);

    // gear icon (decorative)
    {
        const float cx = 722.f, cy = 47.f;
        g.setColour (dimTxt);
        g.drawEllipse (cx - 4.5f, cy - 4.5f, 9.f, 9.f, 1.6f);
        for (int i = 0; i < 8; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 8.f;
            const juce::Point<float> c (cx, cy);
            const auto p1 = c.getPointOnCircumference (7.f, a);
            const auto p2 = c.getPointOnCircumference (10.f, a);
            g.drawLine (p1.x, p1.y, p2.x, p2.y, 2.4f);
        }
        g.drawEllipse (cx - 7.f, cy - 7.f, 14.f, 14.f, 1.6f);
    }

    // section labels
    drawSpaced (g, "PITCH CORRECTION", { 234, 100, 300, 22 }, juce::Justification::centred, 15.f, 0.4f, dimTxt);
    drawSpaced (g, "RETUNE SPEED", { 29, 138, 200, 20 }, juce::Justification::centred, 13.f, 0.32f, dimTxt);
    drawSpaced (g, "HUMANIZE", { 540, 138, 200, 20 }, juce::Justification::centred, 13.f, 0.32f, dimTxt);
    drawSpaced (g, "KEY", { 49, 364, 100, 18 }, juce::Justification::centredLeft, 12.f, 0.3f, dimTxt);
    drawSpaced (g, "SCALE", { 180, 364, 100, 18 }, juce::Justification::centredLeft, 12.f, 0.3f, dimTxt);
    drawSpaced (g, "MIX", { 586, 356, 100, 18 }, juce::Justification::centred, 12.f, 0.3f, dimTxt);

    // values
    auto pct = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    drawSpaced (g, juce::String (juce::roundToInt (speedSlider.getValue())) + " ms",
                { 29, 280, 200, 24 }, juce::Justification::centred, 17.f, 0.02f, txt);
    drawSpaced (g, pct (humanizeSlider.getValue()),
                { 540, 280, 200, 24 }, juce::Justification::centred, 17.f, 0.02f, txt);
    drawSpaced (g, pct (amountSlider.getValue()),
                { 285, 250, 200, 30 }, juce::Justification::centred, 24.f, 0.02f, txt);
    drawSpaced (g, pct (mixSlider.getValue()),
                { 586, 446, 100, 20 }, juce::Justification::centred, 14.f, 0.02f, txt);

    // waveform
    {
        const juce::Rectangle<float> r (295.f, 360.f, 280.f, 86.f);
        const float cy = r.getCentreY();
        const int N = ExtuneProcessor::waveSize;
        const int pos = proc.wavePos.load();

        std::vector<float> amp ((size_t) N);
        for (int i = 0; i < N; ++i)
        {
            const float v = proc.wave[(size_t) ((pos + i) % N)].load();
            amp[(size_t) i] = std::pow (juce::jlimit (0.f, 1.f, v * 2.5f), 0.6f);
        }
        std::vector<float> sm (amp);
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int i = 1; i < N - 1; ++i)
                sm[(size_t) i] = 0.25f * amp[(size_t) i - 1] + 0.5f * amp[(size_t) i] + 0.25f * amp[(size_t) i + 1];
            amp = sm;
        }

        juce::Path wave;
        auto xAt = [&] (int i) { return r.getX() + r.getWidth() * (float) i / (float) (N - 1); };
        auto yAmp = [&] (int i)
        {
            const float edge = std::sin (juce::MathConstants<float>::pi * (float) i / (float) (N - 1));
            return amp[(size_t) i] * std::sqrt (edge) * r.getHeight() * 0.48f;
        };

        wave.startNewSubPath (xAt (0), cy);
        for (int i = 0; i < N; ++i) wave.lineTo (xAt (i), cy - yAmp (i) - 0.5f);
        for (int i = N - 1; i >= 0; --i) wave.lineTo (xAt (i), cy + yAmp (i) + 0.5f);
        wave.closeSubPath();

        g.setColour (purple.withAlpha (0.16f));
        g.fillPath (wave);
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (purple.withAlpha (0.06f));
            g.strokePath (wave, juce::PathStrokeType (1.f + (float) i * 2.f));
        }
        g.setColour (purple.withAlpha (0.9f));
        g.strokePath (wave, juce::PathStrokeType (1.3f));
    }

    // level meter (2 columns)
    {
        const float lv[2] = { proc.levelL.load(), proc.levelR.load() };
        const int segs = 14;
        for (int col = 0; col < 2; ++col)
        {
            const float db = 20.f * std::log10 (lv[col] + 1e-6f);
            const float norm = juce::jlimit (0.f, 1.f, juce::jmap (db, -60.f, 0.f, 0.f, 1.f));
            for (int i = 0; i < segs; ++i)
            {
                const bool lit = norm >= (float) (i + 1) / (float) segs;
                const float x = 718.f + (float) col * 12.f;
                const float y = 440.f - (float) (i + 1) * 6.f;
                g.setColour (lit ? purple.withAlpha (0.6f + 0.4f * (float) i / (float) segs)
                                 : juce::Colour (0xff17171d));
                g.fillRoundedRectangle (x, y, 8.f, 4.5f, 1.2f);
            }
        }
    }
}
