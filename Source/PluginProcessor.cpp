#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::StringArray kRoots { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const juce::StringArray kChordNames { "Maj", "min", "7", "maj7", "min7", "sus2", "sus4", "dim", "aug", "5", "maj9", "min9" };
const juce::StringArray kWaves { "Saw", "Square", "Pulse 25%", "Saw + Square" };
const juce::StringArray kSources { "Internal chord", "MIDI input" };
const juce::StringArray kBandNames { "8", "12", "16", "24", "32" };
constexpr int kBandCounts[] = { 8, 12, 16, 24, 32 };

const std::vector<std::vector<int>>& chordTable()
{
    static const std::vector<std::vector<int>> t {
        { 0, 4, 7 },          // Maj
        { 0, 3, 7 },          // min
        { 0, 4, 7, 10 },      // 7
        { 0, 4, 7, 11 },      // maj7
        { 0, 3, 7, 10 },      // min7
        { 0, 2, 7 },          // sus2
        { 0, 5, 7 },          // sus4
        { 0, 3, 6 },          // dim
        { 0, 4, 8 },          // aug
        { 0, 7, 12 },         // 5 (power chord)
        { 0, 4, 7, 11, 14 },  // maj9
        { 0, 3, 7, 10, 14 }   // min9
    };
    return t;
}

//==============================================================================
struct Preset
{
    const char* name;
    int bands;        // index into kBandCounts
    float fmin, fmax, sharp, attack, release, formant;
    int wave;
    float detune, sib;
    int source, root, chordType, octave;
};

const Preset kPresets[] = {
    // name                      bands fmin  fmax   sharp att   rel   form wave det  sib  src root type oct
    { "Classic Robot (C maj)",      2, 100.f, 7000.f,  1.0f,  2.f,  50.f,  0.f, 0,  6.f, 0.4f, 0, 0,  0,  0 },
    { "Choir Pad (A min)",          3,  90.f, 7500.f,  0.8f, 10.f, 140.f,  0.f, 3, 22.f, 0.3f, 0, 9,  1,  0 },
    { "Hi-Fi Talk (F maj7)",        4, 100.f, 9000.f,  1.2f,  1.5f, 30.f,  0.f, 0, 10.f, 0.6f, 0, 5,  3,  0 },
    { "Dark Whisper (D min7)",      3,  80.f, 5000.f,  0.9f,  4.f,  80.f, -3.f, 1,  8.f, 0.2f, 0, 2,  4, -1 },
    { "Bright Pop (G sus4)",        3, 120.f,10000.f,  1.0f,  2.f,  45.f,  2.f, 3, 14.f, 0.5f, 0, 7,  6,  0 },
    { "Alien Voice (C aug)",        3, 100.f, 8000.f,  1.5f,  2.f,  25.f,  7.f, 2,  4.f, 0.3f, 0, 0,  8,  0 },
    { "Power Rock (E5)",            2, 100.f, 6500.f,  0.9f,  2.f,  40.f,  0.f, 0, 15.f, 0.5f, 0, 4,  9, -1 },
    { "Dream (C maj9)",             3,  90.f, 7000.f,  0.8f, 12.f, 180.f,  0.f, 3, 18.f, 0.25f,0, 0, 10,  0 },
    { "MIDI Played Carrier",        3, 100.f, 8000.f,  1.0f,  2.f,  40.f,  0.f, 3, 12.f, 0.4f, 1, 0,  0,  0 },
};
constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));

//==============================================================================
inline double polyBlep (double t, double dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

inline double renderOsc (int wave, double t, double dt)
{
    const double saw = 2.0 * t - 1.0 - polyBlep (t, dt);

    auto pulse = [&] (double width)
    {
        double v = (t < width ? 1.0 : -1.0);
        v += polyBlep (t, dt);
        v -= polyBlep (std::fmod (t + 1.0 - width, 1.0), dt);
        return v;
    };

    switch (wave)
    {
        case 0:  return saw;
        case 1:  return pulse (0.5);
        case 2:  return pulse (0.25);
        default: return 0.6 * saw + 0.4 * pulse (0.5);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "source", 1 }, "Chord Source", kSources, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "root", 1 }, "Chord Root", kRoots, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "chordType", 1 }, "Chord Type", kChordNames, 0));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { "octave", 1 }, "Octave", -2, 2, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "wave", 1 }, "Carrier Wave", kWaves, 0));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "detune", 1 }, "Detune (cents)", NormalisableRange<float> (0.f, 50.f), 10.f));

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "bands", 1 }, "Bands", kBandNames, 3));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "fmin", 1 }, "Low Freq (Hz)", NormalisableRange<float> (60.f, 800.f, 0.f, 0.5f), 120.f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "fmax", 1 }, "High Freq (Hz)", NormalisableRange<float> (2000.f, 14000.f, 0.f, 0.5f), 8000.f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "sharp", 1 }, "Sharpness", NormalisableRange<float> (0.5f, 2.f), 1.f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "formant", 1 }, "Formant (st)", NormalisableRange<float> (-12.f, 12.f), 0.f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "attack", 1 }, "Attack (ms)", NormalisableRange<float> (0.5f, 50.f, 0.f, 0.4f), 3.f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "release", 1 }, "Release (ms)", NormalisableRange<float> (5.f, 500.f, 0.f, 0.4f), 50.f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "sib", 1 }, "Sibilance", NormalisableRange<float> (0.f, 1.f), 0.4f));

    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "inGain", 1 }, "Input Gain (dB)", NormalisableRange<float> (-24.f, 24.f), 0.f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "outGain", 1 }, "Output Gain (dB)", NormalisableRange<float> (-24.f, 12.f), 0.f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "mix", 1 }, "Dry/Wet", NormalisableRange<float> (0.f, 1.f), 1.f));
    return l;
}
} // namespace

//==============================================================================
VocoderAudioProcessor::VocoderAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pBands     = apvts.getRawParameterValue ("bands");
    pFmin      = apvts.getRawParameterValue ("fmin");
    pFmax      = apvts.getRawParameterValue ("fmax");
    pSharp     = apvts.getRawParameterValue ("sharp");
    pFormant   = apvts.getRawParameterValue ("formant");
    pAttack    = apvts.getRawParameterValue ("attack");
    pRelease   = apvts.getRawParameterValue ("release");
    pWave      = apvts.getRawParameterValue ("wave");
    pDetune    = apvts.getRawParameterValue ("detune");
    pSib       = apvts.getRawParameterValue ("sib");
    pSource    = apvts.getRawParameterValue ("source");
    pRoot      = apvts.getRawParameterValue ("root");
    pChordType = apvts.getRawParameterValue ("chordType");
    pOctave    = apvts.getRawParameterValue ("octave");
    pInGain    = apvts.getRawParameterValue ("inGain");
    pOutGain   = apvts.getRawParameterValue ("outGain");
    pMix       = apvts.getRawParameterValue ("mix");
}

bool VocoderAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void VocoderAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& b : bands)
    {
        for (auto& f : b.mod)  f.reset();
        for (auto& f : b.carL) f.reset();
        for (auto& f : b.carR) f.reset();
        b.env = 0.0;
    }
    for (int i = 0; i < maxNotes; ++i)
    {
        phaseA[(size_t) i] = std::fmod (i * 0.13, 1.0);
        phaseB[(size_t) i] = std::fmod (0.5 + i * 0.07, 1.0);
    }
    sibHP[0].setHighpass (sr, 5500.0, 0.707);
    sibHP[1].setHighpass (sr, 5500.0, 0.707);
    sibHP[0].reset();
    sibHP[1].reset();
    sibEnv = 0.0;
    lastNb = -1; // force filter refresh
}

void VocoderAudioProcessor::updateFilters (int nb, float fmin, float fmax, float sharp, float formant)
{
    fmax = juce::jmax (fmax, fmin * 1.5f);
    const double nyq = 0.45 * sr;
    const double bwOct = std::log2 ((double) fmax / (double) fmin) / (double) (nb - 1);
    const double natQ = 1.0 / (2.0 * std::sinh (0.5 * std::log (2.0) * bwOct));
    const double q = juce::jmax (0.5, natQ * (double) sharp * 0.8);
    const double shift = std::pow (2.0, (double) formant / 12.0);

    for (int i = 0; i < nb; ++i)
    {
        const double f = (double) fmin * std::pow ((double) fmax / (double) fmin, (double) i / (double) (nb - 1));
        const double fm = juce::jmin (f, nyq);
        const double fc = juce::jmin (f * shift, nyq);
        auto& b = bands[(size_t) i];
        for (int s = 0; s < 2; ++s)
        {
            b.mod[s].setBandpass (sr, fm, q);
            b.carL[s].setBandpass (sr, fc, q);
            b.carR[s].setBandpass (sr, fc, q);
        }
    }
}

void VocoderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (numCh == 0 || numSamples == 0)
        return;

    // ---- MIDI: track held notes ----
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();
        if (m.isNoteOn())
        {
            const int n = m.getNoteNumber();
            bool found = false;
            for (int i = 0; i < numHeld; ++i)
                if (heldNotes[(size_t) i] == n) found = true;
            if (! found && numHeld < (int) heldNotes.size())
                heldNotes[(size_t) numHeld++] = n;
        }
        else if (m.isNoteOff())
        {
            const int n = m.getNoteNumber();
            for (int i = 0; i < numHeld; ++i)
                if (heldNotes[(size_t) i] == n)
                {
                    heldNotes[(size_t) i] = heldNotes[(size_t) (numHeld - 1)];
                    --numHeld;
                    break;
                }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            numHeld = 0;
    }
    midi.clear();

    // ---- parameters ----
    const int bandIdx = juce::jlimit (0, 4, (int) pBands->load());
    const int nb = kBandCounts[bandIdx];
    const float fmin = pFmin->load(), fmax = pFmax->load(), sharp = pSharp->load(), formant = pFormant->load();
    if (nb != lastNb || fmin != lastFmin || fmax != lastFmax || sharp != lastSharp || formant != lastFormant)
    {
        updateFilters (nb, fmin, fmax, sharp, formant);
        lastNb = nb; lastFmin = fmin; lastFmax = fmax; lastSharp = sharp; lastFormant = formant;
    }

    const double attCoef = std::exp (-1.0 / (0.001 * (double) pAttack->load() * sr));
    const double relCoef = std::exp (-1.0 / (0.001 * (double) pRelease->load() * sr));
    const int wave = juce::jlimit (0, 3, (int) pWave->load());
    const double detune = (double) pDetune->load();
    const float sibAmt = pSib->load();
    const float inGain = juce::Decibels::decibelsToGain (pInGain->load());
    const float outGain = juce::Decibels::decibelsToGain (pOutGain->load());
    const float mix = pMix->load();

    // ---- chord -> note frequencies ----
    int notes[maxNotes];
    int numNotes = 0;
    if ((int) pSource->load() == 0)
    {
        const auto& chord = chordTable()[(size_t) juce::jlimit (0, (int) chordTable().size() - 1, (int) pChordType->load())];
        const int base = 48 + 12 * (int) pOctave->load() + (int) pRoot->load();
        for (int iv : chord)
            if (numNotes < maxNotes) notes[numNotes++] = base + iv;
    }
    else
    {
        for (int i = 0; i < numHeld && numNotes < maxNotes; ++i)
            notes[numNotes++] = heldNotes[(size_t) i];
    }

    double dA[maxNotes], dB[maxNotes];
    const double up = std::pow (2.0, detune / 1200.0), down = 1.0 / up;
    for (int i = 0; i < numNotes; ++i)
    {
        const double f = 440.0 * std::pow (2.0, (notes[i] - 69) / 12.0);
        dA[i] = juce::jmin (0.45, f * up / sr);
        dB[i] = juce::jmin (0.45, f * down / sr);
    }
    const double carrierScale = numNotes > 0 ? 1.0 / std::sqrt ((double) numNotes) : 0.0;
    const double makeup = 12.0 / std::sqrt ((double) nb);

    float* chL = buffer.getWritePointer (0);
    float* chR = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        const float dryL = chL[n];
        const float dryR = chR != nullptr ? chR[n] : dryL;
        const double mod = 0.5 * (double) (dryL + dryR) * (double) inGain;

        // carrier (stereo)
        double cL = 0.0, cR = 0.0;
        for (int i = 0; i < numNotes; ++i)
        {
            const double a = renderOsc (wave, phaseA[(size_t) i], dA[i]);
            const double b = renderOsc (wave, phaseB[(size_t) i], dB[i]);
            cL += 0.75 * a + 0.25 * b;
            cR += 0.75 * b + 0.25 * a;
            phaseA[(size_t) i] += dA[i]; if (phaseA[(size_t) i] >= 1.0) phaseA[(size_t) i] -= 1.0;
            phaseB[(size_t) i] += dB[i]; if (phaseB[(size_t) i] >= 1.0) phaseB[(size_t) i] -= 1.0;
        }
        cL *= carrierScale;
        cR *= carrierScale;

        // vocoder bands
        double oL = 0.0, oR = 0.0;
        for (int i = 0; i < nb; ++i)
        {
            auto& b = bands[(size_t) i];
            const double mb = b.mod[1].process (b.mod[0].process (mod));
            const double r = std::abs (mb);
            const double c = r > b.env ? attCoef : relCoef;
            b.env = c * b.env + (1.0 - c) * r;

            oL += b.carL[1].process (b.carL[0].process (cL)) * b.env;
            oR += b.carR[1].process (b.carR[0].process (cR)) * b.env;
        }

        // unvoiced / sibilance: high-passed voice envelope drives white noise
        const double hp = sibHP[1].process (sibHP[0].process (mod));
        const double hr = std::abs (hp);
        const double sc = hr > sibEnv ? attCoef : relCoef;
        sibEnv = sc * sibEnv + (1.0 - sc) * hr;
        const double nL = (double) (rng.nextFloat() * 2.0f - 1.0f) * sibEnv * (double) sibAmt * 2.0;
        const double nR = (double) (rng.nextFloat() * 2.0f - 1.0f) * sibEnv * (double) sibAmt * 2.0;

        const float wetL = (float) std::tanh ((oL * makeup + nL) * (double) outGain);
        const float wetR = (float) std::tanh ((oR * makeup + nR) * (double) outGain);

        if (chR != nullptr)
        {
            chL[n] = dryL * (1.f - mix) + wetL * mix;
            chR[n] = dryR * (1.f - mix) + wetR * mix;
        }
        else
        {
            chL[n] = dryL * (1.f - mix) + 0.5f * (wetL + wetR) * mix;
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* VocoderAudioProcessor::createEditor()
{
    return new VocoderEditor (*this);
}

int VocoderAudioProcessor::getNumPrograms() { return kNumPresets; }

const juce::String VocoderAudioProcessor::getProgramName (int index)
{
    if (index >= 0 && index < kNumPresets)
        return kPresets[index].name;
    return {};
}

void VocoderAudioProcessor::setParam (const char* id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void VocoderAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= kNumPresets)
        return;

    currentProgram = index;
    const auto& p = kPresets[index];
    setParam ("bands", (float) p.bands);
    setParam ("fmin", p.fmin);
    setParam ("fmax", p.fmax);
    setParam ("sharp", p.sharp);
    setParam ("attack", p.attack);
    setParam ("release", p.release);
    setParam ("formant", p.formant);
    setParam ("wave", (float) p.wave);
    setParam ("detune", p.detune);
    setParam ("sib", p.sib);
    setParam ("source", (float) p.source);
    setParam ("root", (float) p.root);
    setParam ("chordType", (float) p.chordType);
    setParam ("octave", (float) p.octave);
}

void VocoderAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VocoderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocoderAudioProcessor();
}
