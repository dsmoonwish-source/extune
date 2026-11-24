#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int maskChromatic = 0xFFF;
    constexpr int maskMajor = (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11);
    constexpr int maskMinor = (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10);

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        std::vector<std::unique_ptr<RangedAudioParameter>> p;

        p.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { "key", 1 }, "Key",
            StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));

        p.push_back (std::make_unique<AudioParameterChoice> (
            ParameterID { "scale", 1 }, "Scale",
            StringArray { "Chromatic", "Major", "Minor" }, 1));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { "speed", 1 }, "Retune Speed",
            NormalisableRange<float> (0.f, 200.f, 0.1f, 0.5f), 20.f));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { "amount", 1 }, "Pitch Correction",
            NormalisableRange<float> (0.f, 1.f, 0.001f), 1.f));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { "humanize", 1 }, "Humanize",
            NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f));

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { "mix", 1 }, "Mix",
            NormalisableRange<float> (0.f, 1.f, 0.001f), 1.f));

        return { p.begin(), p.end() };
    }
}

ExtuneProcessor::ExtuneProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "EXTUNE", createLayout())
{
    for (auto& w : wave) w.store (0.f);
    for (auto& b : dryBuf) b.assign (dryMask + 1, 0.f);
}

void ExtuneProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& s : shifter) s.reset();
    for (auto& b : dryBuf) std::fill (b.begin(), b.end(), 0.f);
    hopCounter = 0;
    dryW = 0;
    corr = targetCorr = 0.f;

    // average delay of the pitch shifter = grain / 2 (grain = 40 ms)
    dryDelay = (int) (0.02 * sr);
    setLatencySamples (dryDelay);
}

bool ExtuneProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

float ExtuneProcessor::quantise (float midi, int key, int scale) const
{
    const int mask = scale == 1 ? maskMajor : scale == 2 ? maskMinor : maskChromatic;
    const int base = (int) std::floor (midi);

    float best = midi, bestDist = 1e9f;
    for (int n = base - 2; n <= base + 3; ++n)
    {
        const int rel = (((n - key) % 12) + 12) % 12;
        if (mask & (1 << rel))
        {
            const float dist = std::abs ((float) n - midi);
            if (dist < bestDist) { bestDist = dist; best = (float) n; }
        }
    }
    return best;
}

void ExtuneProcessor::applyPreset (int index)
{
    struct P { float speed, amount, humanize, mix; };
    static const P presets[] = {
        {  20.f, 1.0f, 0.5f, 1.f },   // Default
        {  80.f, 0.7f, 0.8f, 1.f },   // Natural
        {   0.f, 1.0f, 0.0f, 1.f },   // Hard Tune
        { 120.f, 0.4f, 0.7f, 1.f }    // Subtle
    };
    const auto& p = presets[juce::jlimit (0, 3, index)];

    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    set ("speed", p.speed);
    set ("amount", p.amount);
    set ("humanize", p.humanize);
    set ("mix", p.mix);
}

void ExtuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n     = buffer.getNumSamples();

    const int   key      = (int) *apvts.getRawParameterValue ("key");
    const int   scale    = (int) *apvts.getRawParameterValue ("scale");
    const float speed    = *apvts.getRawParameterValue ("speed");
    const float amount   = *apvts.getRawParameterValue ("amount");
    const float humanize = *apvts.getRawParameterValue ("humanize");
    const float mix      = *apvts.getRawParameterValue ("mix");

    const float grain = 0.04f * (float) sr;
    float pk[2] = { 0.f, 0.f };

    for (int s = 0; s < n; ++s)
    {
        float mono = 0.f;
        for (int ch = 0; ch < numCh; ++ch)
            mono += buffer.getReadPointer (ch)[s];
        mono /= (float) juce::jmax (1, numCh);

        yin.push (mono);

        if (++hopCounter >= hopSize)
        {
            hopCounter = 0;
            const float f0 = yin.detect (sr);

            if (f0 > 0.f)
            {
                const float midi = 69.f + 12.f * std::log2 (f0 / 440.f);
                const float tgt  = quantise (midi, key, scale);

                // Humanize: dead zone around the note + slower retune
                float raw = tgt - midi;
                const float dz = humanize * 0.2f;
                raw = (raw < 0.f ? -1.f : 1.f) * juce::jmax (0.f, std::abs (raw) - dz);

                targetCorr = raw * amount;
                smoothTime = speed * (1.f + 3.f * humanize);
                detectedMidi.store (midi);
                targetMidi.store (tgt);
            }
            else
            {
                targetCorr = 0.f;
                smoothTime = 30.f;
                detectedMidi.store (-1.f);
                targetMidi.store (-1.f);
            }
        }

        const float tMs  = juce::jmax (0.5f, smoothTime);
        const float coef = 1.f - std::exp (-1.f / ((float) sr * tMs * 0.001f));
        corr += coef * (targetCorr - corr);

        const float ratio = std::pow (2.f, corr / 12.f);

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            const float in = data[s];

            dryBuf[ch][(size_t) dryW] = in;
            const float dry = dryBuf[ch][(size_t) ((dryW - dryDelay) & dryMask)];
            const float wet = shifter[ch].process (in, ratio, grain);

            const float out = dry + mix * (wet - dry);
            data[s] = out;
            pk[ch] = juce::jmax (pk[ch], std::abs (out));
        }
        dryW = (dryW + 1) & dryMask;

        const float o0 = buffer.getReadPointer (0)[s];
        wavePeak = juce::jmax (wavePeak, std::abs (o0));
        if (++waveCounter >= 1024)
        {
            waveCounter = 0;
            const int pos = wavePos.load();
            wave[(size_t) pos].store (wavePeak);
            wavePos.store ((pos + 1) % waveSize);
            wavePeak = 0.f;
        }
    }

    if (numCh == 1) pk[1] = pk[0];
    levelL.store (juce::jmax (pk[0], levelL.load() * 0.85f));
    levelR.store (juce::jmax (pk[1], levelR.load() * 0.85f));
}

juce::AudioProcessorEditor* ExtuneProcessor::createEditor()
{
    return new ExtuneEditor (*this);
}

void ExtuneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ExtuneProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ExtuneProcessor();
}
