#include "PluginProcessor.h"
#include "PluginEditor.h"

Saturno8Processor::Saturno8Processor()
    : AudioProcessor (BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout Saturno8Processor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "master", 1 }, "Master", 0.0f, 1.0f, 0.7f));
    return layout;
}

bool Saturno8Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void Saturno8Processor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    phase = 0.0;
    env = gate = 0.0f;
    note = -1;
}

void Saturno8Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const float master = apvts.getRawParameterValue ("master")->load();
    const float coef = 1.0f - std::exp (-1.0f / (0.01f * (float) sr)); // ~10 ms

    auto midiIt = midi.cbegin();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        while (midiIt != midi.cend() && (*midiIt).samplePosition <= i)
        {
            const auto msg = (*midiIt).getMessage();
            if (msg.isNoteOn())
            {
                note = msg.getNoteNumber();
                gate = msg.getFloatVelocity();
                inc = juce::MidiMessage::getMidiNoteInHertz (note) / sr;
            }
            else if (msg.isNoteOff() && msg.getNoteNumber() == note)
                gate = 0.0f;
            ++midiIt;
        }

        env += (gate - env) * coef;
        const float s = (float) (2.0 * phase - 1.0) * env * master * 0.3f;
        phase += inc;
        if (phase >= 1.0) phase -= 1.0;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample (ch, i, s);
    }
}

juce::AudioProcessorEditor* Saturno8Processor::createEditor()
{
    return new Saturno8Editor (*this);
}

void Saturno8Processor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void Saturno8Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Saturno8Processor();
}
