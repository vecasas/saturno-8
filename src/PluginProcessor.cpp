#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::String noteName (int n)
{
    static const char* names[] { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };
    return juce::String (names[n % 12]) + juce::String (n / 12 - 1);
}

std::unique_ptr<juce::RangedAudioParameter> makeParam (const s8::ParamDef& d, const char* prefix, const char* layerName)
{
    const juce::ParameterID id { Saturno8Processor::pid (prefix, d.id), 1 };
    juce::String name = juce::String (juce::CharPointer_UTF8 (d.section)) + " " + juce::String (juce::CharPointer_UTF8 (d.name));
    if (layerName != nullptr) name = juce::String (layerName) + " " + name;
    const juce::String sid (d.id);
    switch (d.type)
    {
        case s8::PType::Choice: return std::make_unique<juce::AudioParameterChoice> (id, name, s8::choicesOf (d), (int) d.def);
        case s8::PType::Bool:   return std::make_unique<juce::AudioParameterBool> (id, name, d.def > 0.5f);
        case s8::PType::Int:
            if (sid == "split")
                return std::make_unique<juce::AudioParameterInt> (id, name, (int) d.min, (int) d.max, (int) d.def,
                    juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return noteName (v); }));
            return std::make_unique<juce::AudioParameterInt> (id, name, (int) d.min, (int) d.max, (int) d.def);
        case s8::PType::Float:  break;
    }
    juce::NormalisableRange<float> range (d.min, d.max, 0.0f, d.skew);
    // Texto con unidades; los mandos de 0..1 se muestran de 0 a 10, como en el panel.
    std::function<juce::String (float, int)> toText = [] (float v, int) { return juce::String (v * 10.0f, 1); };
    if (sid.endsWith ("_a") || sid.endsWith ("_d") || sid.endsWith ("_r") || sid == "lfo_delay" || sid == "porta")
        toText = [] (float v, int) { return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s"; };
    else if (sid == "lfo_rate")
        toText = [] (float v, int) { return juce::String (v, v < 10.0f ? 2 : 1) + " Hz"; };
    else if (sid == "vco2_fine" || sid == "bend_range")
        toText = [] (float v, int) { return (v > 0 ? "+" : "") + juce::String (v, 2) + " st"; };
    else if (sid == "arp_gate" || sid == "arp_swing" || sid == "arp_chance")
        toText = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    else if (sid == "tempo")
        toText = [] (float v, int) { return juce::String (v, 1) + " BPM"; };
    else if (sid == "tune")
        toText = [] (float v, int) { return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)) + " cents"; };
    return std::make_unique<juce::AudioParameterFloat> (id, name, range, d.def,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (toText));
}
} // namespace

Saturno8Processor::Saturno8Processor()
    : AudioProcessor (BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int l = 0; l < 2; ++l)
    {
        for (auto& d : s8::layerParamDefs())
            raw[l][d.id] = apvts.getRawParameterValue (pid (kPrefix[l], d.id));
        chordCopy[l] = { 0, 4, 7 };
        chordCopy[l].reserve (s8::kMaxVoices);
        pendingChord[l].reserve (s8::kMaxVoices);
    }
    for (auto& d : s8::globalParamDefs())
        global[d.id] = apvts.getRawParameterValue (d.id);
    synth.upperPlayer.steps = &steps[0];
    synth.lowerPlayer.steps = &steps[1];
}

juce::AudioProcessorValueTreeState::ParameterLayout Saturno8Processor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int l = 0; l < 2; ++l)
    {
        for (auto& d : s8::layerParamDefs())
            layout.add (makeParam (d, kPrefix[l], l == 0 ? "Sup." : "Inf."));
    }
    for (auto& d : s8::globalParamDefs())
        layout.add (makeParam (d, "", nullptr));
    return layout;
}

bool Saturno8Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void Saturno8Processor::prepareToPlay (double sampleRate, int)
{
    synth.setSampleRate ((float) sampleRate);
    synth.reset();
    keyboardState.reset();
}

void Saturno8Processor::readLayer (s8::Layer& layer, const RawMap& m, int& last)
{
    auto& p = layer.p;
    auto& a = layer.a;
    auto f = [&m] (std::string_view id) { return m.at (id)->load(); };
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

    auto& pp = (&layer == &synth.upper ? synth.upperPlayer : synth.lowerPlayer).p;
    pp.mode = (s8::PlayerMode) n ("arp_mode"); pp.pattern = (s8::ArpPattern) n ("arp_pattern"); pp.dir = (s8::ArpDir) n ("arp_dir");
    pp.range = n ("arp_range"); pp.division = n ("arp_div"); pp.gate = f ("arp_gate"); pp.swing = f ("arp_swing");
    pp.chance = f ("arp_chance"); pp.hold = b ("arp_hold"); pp.length = n ("seq_len"); pp.play = (s8::SeqPlay) n ("seq_play");

    // Cambiar de modo de asignación suelta todo para empezar limpio.
    if (assign != last) { if (last >= 0) layer.allNotesOff(); last = assign; }
}

void Saturno8Processor::readGlobal()
{
    auto& g = synth.g;
    auto f = [this] (std::string_view id) { return global.at (id)->load(); };
    auto n = [&f] (std::string_view id) { return (int) std::lround (f (id)); };
    auto b = [&f] (std::string_view id) { return f (id) > 0.5f; };
    g.keyMode = (s8::KeyMode) n ("key_mode"); g.splitPoint = n ("split"); g.balance = f ("balance");
    g.sustainUpper = b ("sus_upper"); g.sustainLower = b ("sus_lower");
    g.bendRange = f ("bend_range"); g.bendDest = (s8::ModDest) n ("bend_dest"); g.bendVcf = f ("bend_vcf");
    g.bendTarget = (s8::Target) n ("bend_target"); g.wheelVco = f ("wheel_vco"); g.wheelVcf = f ("wheel_vcf");
    g.portaOn = b ("porta_on"); g.portamento = f ("porta"); g.portaTarget = (s8::Target) n ("porta_target");
    g.driftOn = b ("drift_on"); g.condition = (s8::Condition) n ("condition");
    g.driftVco = f ("drift_vco"); g.driftVcf = f ("drift_vcf"); g.driftVca = f ("drift_vca"); g.envSlop = f ("env_slop");
    g.volume = f ("master"); g.tune = f ("tune"); g.limiter = b ("limiter");
}

void Saturno8Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);
    readLayer (synth.upper, raw[0], lastAssign[0]);
    readLayer (synth.lower, raw[1], lastAssign[1]);
    readGlobal();
    synth.updatePlayers();

    // Tempo y posición del DAW; sin DAW (o parado) se usa el tempo propio.
    double bpm = global.at ("tempo")->load(), ppq = 0;
    bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition(); q && pos->getIsPlaying()) { ppq = *q; hostPlaying = true; }
        }
    synth.setTransport (bpm, hostPlaying, ppq);

    if (chordRestore.exchange (false))
    {
        const juce::SpinLock::ScopedTryLockType lock (chordLock);
        if (lock.isLocked()) { synth.upper.setChordIntervals (pendingChord[0]); synth.lower.setChordIntervals (pendingChord[1]); }
        else chordRestore.store (true);
    }
    if (const int l = chordLearnRequest.exchange (-1); l == 0 || l == 1)
    {
        auto& layer = l == 0 ? synth.upper : synth.lower;
        if (layer.learnChord())
        {
            const juce::SpinLock::ScopedTryLockType lock (chordLock);
            if (lock.isLocked()) chordCopy[l].assign (layer.chordIntervals().begin(), layer.chordIntervals().end());
        }
    }

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    float minGain = 1.0f;

    auto midiIt = midi.cbegin();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        bool controlsChanged = i == 0;
        for (; midiIt != midi.cend() && (*midiIt).samplePosition <= i; ++midiIt)
        {
            const auto msg = (*midiIt).getMessage();
            if (msg.isNoteOn()) synth.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff()) synth.noteOff (msg.getNoteNumber());
            else if (msg.isSustainPedalOn()) synth.setSustain (true);
            else if (msg.isSustainPedalOff()) synth.setSustain (false);
            else if (msg.isAllNotesOff() || msg.isAllSoundOff()) synth.allNotesOff();
            else if (msg.isPitchWheel()) { synth.setPitchBend ((float) (msg.getPitchWheelValue() - 8192) / 8192.0f); controlsChanged = true; }
            else if (msg.isControllerOfType (1)) { synth.setModWheel ((float) msg.getControllerValue() / 127.0f); controlsChanged = true; }
        }
        if (controlsChanged) synth.updatePlay();

        float l = 0, r = 0;
        synth.process (l, r);
        minGain = std::min (minGain, synth.lastReduction());
        if (R != nullptr) { L[i] = l; R[i] = r; }
        else L[i] = 0.5f * (l + r);
    }
    currentStep[0].store (synth.upperPlayer.currentStep());
    currentStep[1].store (synth.lowerPlayer.currentStep());
    activeVoices[0].store (synth.upper.activeVoices());
    activeVoices[1].store (synth.lower.activeVoices());
    limiterReduction.store (minGain);
}

juce::String Saturno8Processor::chordDescription (int layer) const
{
    juce::StringArray parts;
    const juce::SpinLock::ScopedLockType lock (chordLock);
    for (int iv : chordCopy[layer]) parts.add (noteName (iv + 12).dropLastCharacters (1));
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
        const juce::SpinLock::ScopedLockType lock (chordLock);
        for (int l = 0; l < 2; ++l)
        {
            juce::StringArray chord;
            for (int iv : chordCopy[l]) chord.add (juce::String (iv));
            state.setProperty (l == 0 ? "chord" : "chord_lower", chord.joinIntoString (","), nullptr);
        }
    }
    for (int l = 0; l < 2; ++l)
    {
        juce::StringArray packed;
        for (int i = 0; i < s8::kSeqSteps; ++i) packed.add (juce::String (steps[l].get (i).pack()));
        state.setProperty (l == 0 ? "steps_upper" : "steps_lower", packed.joinIntoString (","), nullptr);
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
            std::vector<int> chords[2];
            for (int l = 0; l < 2; ++l)
                for (auto& t : juce::StringArray::fromTokens (tree.getProperty (l == 0 ? "chord" : "chord_lower").toString(), ",", ""))
                    if (chords[l].size() < (size_t) s8::kMaxVoices) chords[l].push_back (juce::jlimit (0, 96, t.getIntValue()));
            for (int l = 0; l < 2; ++l)
            {
                steps[l].reset();
                auto packed = juce::StringArray::fromTokens (tree.getProperty (l == 0 ? "steps_upper" : "steps_lower").toString(), ",", "");
                for (int i = 0; i < juce::jmin (packed.size(), s8::kSeqSteps); ++i)
                {
                    auto st = s8::Step::unpack (packed[i].getIntValue());
                    st.note = juce::jlimit (-24, 24, st.note);
                    steps[l].set (i, st);
                }
            }
            apvts.replaceState (tree);
            const juce::SpinLock::ScopedLockType lock (chordLock);
            for (int l = 0; l < 2; ++l)
            {
                if (chords[l].empty()) chords[l] = { 0, 4, 7 };
                chordCopy[l].assign (chords[l].begin(), chords[l].end());
                pendingChord[l].assign (chords[l].begin(), chords[l].end());
            }
            chordRestore.store (true);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Saturno8Processor();
}
