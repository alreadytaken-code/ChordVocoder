#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>

//==============================================================================
// Simple double-precision biquad (transposed direct form II)
struct Biquad
{
    double b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void setBandpass (double fs, double f, double q)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f / fs;
        const double alpha = std::sin (w0) / (2.0 * q);
        const double cw = std::cos (w0);
        const double a0 = 1.0 + alpha;
        b0 = alpha / a0;
        b1 = 0.0;
        b2 = -alpha / a0;
        a1 = -2.0 * cw / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void setHighpass (double fs, double f, double q)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f / fs;
        const double alpha = std::sin (w0) / (2.0 * q);
        const double cw = std::cos (w0);
        const double a0 = 1.0 + alpha;
        b0 = ((1.0 + cw) * 0.5) / a0;
        b1 = -(1.0 + cw) / a0;
        b2 = b0;
        a1 = -2.0 * cw / a0;
        a2 = (1.0 - alpha) / a0;
    }

    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0.0; }
};

//==============================================================================
class VocoderAudioProcessor : public juce::AudioProcessor
{
public:
    VocoderAudioProcessor();
    ~VocoderAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Chord Vocoder"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static constexpr int maxBands = 32;
    static constexpr int maxNotes = 8;

    struct Band
    {
        Biquad mod[2], carL[2], carR[2];
        double env = 0.0;
    };

    void updateFilters (int numBands, float fmin, float fmax, float sharp, float formant);
    void setParam (const char* id, float plainValue);

    std::array<Band, maxBands> bands;
    Biquad sibHP[2];
    double sibEnv = 0.0;

    // carrier oscillators
    std::array<double, maxNotes> phaseA {}, phaseB {};

    // held MIDI notes (used when Chord Source = MIDI)
    std::array<int, 16> heldNotes {};
    int numHeld = 0;

    juce::Random rng;
    double sr = 44100.0;
    int currentProgram = 0;

    // cached filter settings
    int lastNb = -1;
    float lastFmin = -1.f, lastFmax = -1.f, lastSharp = -1.f, lastFormant = -100.f;

    // parameter pointers
    std::atomic<float> *pBands, *pFmin, *pFmax, *pSharp, *pFormant, *pAttack, *pRelease,
        *pWave, *pDetune, *pSib, *pSource, *pRoot, *pChordType, *pOctave, *pInGain,
        *pOutGain, *pMix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocoderAudioProcessor)
};
