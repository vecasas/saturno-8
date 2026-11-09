#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr const char* kUpper = "u_";

std::unique_ptr<juce::RangedAudioParameter> makeParam (const s8::ParamDef& d, const char* prefix)
{
    const juce::ParameterID id { Saturno8Processor::pid (prefix, d.id), 1 };
    const juce::String name = juce::String (juce::CharPointer_UTF8 (d.section)) + " " + juce::String (juce::CharPointer_UTF8 (d.name));
    switch (d.type)
    {
        case s8::PType::Choice: return std::make_unique<juce::AudioParameterChoice> (id, name, s8::choicesOf (d), (int) d.def);
        case s8::PType::Bool:   return std::make_unique<juce::AudioParameterBool> (id, name, d.def > 0.5f);
        case s8::PType::Int:    return std::make_unique<juce::AudioParameterInt> (id, name, (int) d.min, (int) d.max, (int) d.def);
        case s8::PType::Float:  break;
    }
    juce::NormalisableRange<float> range (d.min, d.max, 0.0f, d.skew);
    // Texto con unidades; los mandos de 0..1 se muestran de 0 a 10, como en el panel.
    const juce::String sid (d.id);
    std::function<juce::String (float, int)> toText = [] (float v, int) { return juce::String (v * 10.0f, 1); };
    if (sid.endsWith ("_a") || sid.endsWith ("_d") || sid.endsWith ("_r") || sid == "lfo_delay")
        toText = [] (float v, int) { return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s"; };
    else if (sid == "lfo_rate")
        toText = [] (float v, int) { return juce::String (v, v < 10.0f ? 2 : 1) + " Hz"; };
    else if (sid == "vco2_fine")
        toText = [] (float v, int) { return (v > 0 ? "+" : "") + juce::String (v, 2) + " st"; };
    return std::make_unique<juce::AudioParameterFloat> (id, name, range, d.def,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (toText));
}
} // namespace

Saturno8Processor::Saturno8Processor()
    : AudioProcessor (BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto& d : s8::layerParamDefs())
        raw[d.id] = apvts.getRawParameterValue (pid (kUpper, d.id));
    master = apvts.getRawParameterValue ("master");
    chordCopy.reserve (s8::kMaxVoices);
    pendingChord.reserve (s8::kMaxVoices);
}

juce::AudioProcessorValueTreeState::ParameterLayout Saturno8Processor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& d : s8::layerParamDefs())
        layout.add (makeParam (d, kUpper));
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
    upper.setSampleRate ((float) sampleRate);
    upper.reset();
    keyboardState.reset();
}

void Saturno8Processor::readParams()
{
    auto& p = upper.p;
    auto& a = upper.a;
    auto f = [this] (std::string_view id) { return raw.at (id)->load(); };
    auto n = [&f] (std::string_view id) { return (int) std::lround (f (id)); };
    auto b = [&f] (std::string_view id) { return f (id) > 0.5f; };
    p.vco1Range = n ("vco1_range"); p.vco1Wave = (s8::Wave1) n ("vco1_wave"); p.crossMod = f ("xmod");
    p.vco2Range = n ("vco2_range"); p.vco2Fine = f ("vco2_fine"); p.vco2Wave = (s8::Wave2) n ("vco2_wave");
    p.vco2Low = b ("vco2_low"); p.sync = b ("sync");
    p.vcoLfo = f ("vco_lfo"); p.vcoEnv = f ("vco_env"); p.vcoModDest = (s8::ModDest) n ("vco_dest");
    p.pulseWidth = f ("pw"); p.pwmAmount = f ("pwm"); p.pwmSrc = (s8::PwmSrc) n ("pwm_src");
    p.vco1Level = f ("vco1_lvl"); p.vco2Level = f ("vco2_lvl"); p.hpf = f ("hpf");
    p.cutoff = f ("cutoff"); p.resonance = f ("reso"); p.slope24 = n ("slope") == 1;
    p.vcfEnv = f ("vcf_env"); p.vcfEnv2 = n ("vcf_env_src") == 1; p.vcfLfo = f ("vcf_lfo"); p.keyFollow = f ("vcf_key");
    p.level = f ("level"); p.vcaLfo = f ("vca_lfo");
    p.a1 = f ("env1_a"); p.d1 = f ("env1_d"); p.s1 = f ("env1_s"); p.r1 = f ("env1_r");
    p.key1 = f ("env1_key"); p.vel1 = f ("env1_vel"); p.env1Invert = b ("env1_inv");
    p.a2 = f ("env2_a"); p.d2 = f ("env2_d"); p.s2 = f ("env2_s"); p.r2 = f ("env2_r");
    p.key2 = f ("env2_key"); p.vel2 = f ("env2_vel");
    p.lfoRate = f ("lfo_rate"); p.lfoDelay = f ("lfo_delay"); p.lfoWave = (s8::LfoWave) n ("lfo_wave");
    const int assign = n ("assign");
    a.mode = (s8::Assign) assign; a.priority = (s8::Priority) n ("priority"); a.legato = n ("trigger") == 0;
    a.unisonVoices = n ("uni_voices"); a.unisonDetune = f ("uni_detune"); a.polyVoices = n ("poly_voices");

    // Cambiar de modo de asignación suelta todo para empezar limpio.
    if (assign != lastAssign) { if (lastAssign >= 0) upper.allNotesOff(); lastAssign = assign; }
}

void Saturno8Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);
    readParams();

    if (chordRestore.exchange (false))
    {
        const juce::SpinLock::ScopedTryLockType lock (chordLock);
        if (lock.isLocked()) upper.setChordIntervals (pendingChord);
        else chordRestore.store (true);
    }
    if (chordLearnRequest.exchange (false) && upper.learnChord())
    {
        const juce::SpinLock::ScopedTryLockType lock (chordLock);
        if (lock.isLocked()) chordCopy.assign (upper.chordIntervals().begin(), upper.chordIntervals().end());   // capacidad reservada: sin reservas nuevas
    }

    const float gain = master->load();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    auto midiIt = midi.cbegin();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        for (; midiIt != midi.cend() && (*midiIt).samplePosition <= i; ++midiIt)
        {
            const auto msg = (*midiIt).getMessage();
            if (msg.isNoteOn()) upper.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff()) upper.noteOff (msg.getNoteNumber());
            else if (msg.isSustainPedalOn()) upper.setSustain (true);
            else if (msg.isSustainPedalOff()) upper.setSustain (false);
            else if (msg.isAllNotesOff() || msg.isAllSoundOff()) upper.allNotesOff();
        }

        float l = 0, r = 0;
        upper.process (l, r);
        if (R != nullptr) { L[i] = l * gain; R[i] = r * gain; }
        else L[i] = 0.5f * (l + r) * gain;
    }
    activeVoices.store (upper.activeVoices());
}

juce::String Saturno8Processor::chordDescription() const
{
    static const char* names[] { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };
    juce::StringArray parts;
    const juce::SpinLock::ScopedLockType lock (chordLock);
    for (int iv : chordCopy) parts.add (names[iv % 12]);
    return parts.joinIntoString (" ");
}

juce::AudioProcessorEditor* Saturno8Processor::createEditor()
{
    return new Saturno8Editor (*this);
}

void Saturno8Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    {
        juce::StringArray chord;
        const juce::SpinLock::ScopedLockType lock (chordLock);
        for (int iv : chordCopy) chord.add (juce::String (iv));
        state.setProperty ("chord", chord.joinIntoString (","), nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void Saturno8Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            std::vector<int> chord;
            for (auto& t : juce::StringArray::fromTokens (tree.getProperty ("chord").toString(), ",", ""))
                if (chord.size() < (size_t) s8::kMaxVoices) chord.push_back (juce::jlimit (0, 96, t.getIntValue()));
            apvts.replaceState (tree);
            if (! chord.empty())
            {
                const juce::SpinLock::ScopedLockType lock (chordLock);
                chordCopy.assign (chord.begin(), chord.end());
                pendingChord.assign (chord.begin(), chord.end());
                chordRestore.store (true);
            }
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Saturno8Processor();
}
