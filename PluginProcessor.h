#pragma once
#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp.h"

class ExtuneProcessor : public juce::AudioProcessor
{
public:
    ExtuneProcessor();
    ~ExtuneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EXTUNE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void applyPreset (int index);

    juce::AudioProcessorValueTreeState apvts;

    // For the UI
    static constexpr int waveSize = 160;
    std::array<std::atomic<float>, waveSize> wave;
    std::atomic<int>   wavePos { 0 };
    std::atomic<float> levelL { 0.f }, levelR { 0.f };
    std::atomic<float> detectedMidi { -1.f };
    std::atomic<float> targetMidi   { -1.f };

private:
    static constexpr int hopSize = 256;
    static constexpr int dryMask = 16383;

    float quantise (float midi, int key, int scale) const;

    YinDetector  yin;
    PitchShifter shifter[2];
    std::vector<float> dryBuf[2];

    double sr = 44100.0;
    int    hopCounter = 0;
    int    dryW = 0, dryDelay = 0;
    int    waveCounter = 0;
    float  wavePeak = 0.f;
    float  targetCorr = 0.f;   // semitones
    float  smoothTime = 20.f;  // ms
    float  corr = 0.f;         // smoothed, semitones

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExtuneProcessor)
};
