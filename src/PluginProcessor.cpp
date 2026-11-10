#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

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
    else if (sid == "fx_delay_ms" || sid == "rv_predelay")
        toText = [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; };
    else if (sid == "eq_low" || sid == "eq_high")
        toText = [] (float v, int) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; };
    else if (sid.endsWith ("_amt"))
        toText = [] (float v, int) { return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
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
        for (auto& d : s8::layerFxParamDefs())
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
        for (auto& d : s8::layerFxParamDefs())
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

void Saturno8Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostRate = sampleRate;
    maxBlock = juce::jmax (1, samplesPerBlock);
    for (int k = 0; k < 2; ++k)
    {
        oversamplers[k] = std::make_unique<juce::dsp::Oversampling<float>> (
            2, k + 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversamplers[k]->initProcessing ((size_t) maxBlock);
    }
    work.setSize (2, maxBlock);
    factor = 0;   // se aplica en el primer bloque
    keyboardState.reset();
}

void Saturno8Processor::applyQuality (int newFactor)
{
    // Cambiar de calidad reinicia el motor a la nueva frecuencia interna (puede cortar las notas).
    factor = newFactor;
    synth.setSampleRate ((float) (hostRate * factor));
    synth.reset();
    lastAssign[0] = lastAssign[1] = -1;
    setLatencySamples (factor == 1 ? 0 : juce::roundToInt (oversamplers[factor == 2 ? 0 : 1]->getLatencyInSamples()));
}

void Saturno8Processor::handleMidi (const juce::MidiMessage& msg, bool& controlsChanged)
{
    if (msg.isNoteOn()) synth.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
    else if (msg.isNoteOff()) synth.noteOff (msg.getNoteNumber());
    else if (msg.isSustainPedalOn()) synth.setSustain (true);
    else if (msg.isSustainPedalOff()) synth.setSustain (false);
    else if (msg.isAllNotesOff() || msg.isAllSoundOff()) synth.allNotesOff();
    else if (msg.isPitchWheel()) { synth.setPitchBend ((float) (msg.getPitchWheelValue() - 8192) / 8192.0f); controlsChanged = true; }
    else if (msg.isChannelPressure()) { synth.setAftertouch ((float) msg.getChannelPressureValue() / 127.0f); controlsChanged = true; }
    else if (msg.isController())
    {
        const int cc = msg.getControllerNumber();
        if (cc == 1) { synth.setModWheel ((float) msg.getControllerValue() / 127.0f); controlsChanged = true; }
        // MIDI learn: el primer CC que llega se asigna al control que espera.
        if (auto* target = learnTarget.exchange (nullptr))
        {
            for (auto& m : ccMap) if (m.load() == target) m.store (nullptr);
            ccMap[(size_t) cc].store (target);
            midiMapChanged.store (true);
        }
        if (auto* p = ccMap[(size_t) cc].load())
            p->setValueNotifyingHost ((float) msg.getControllerValue() / 127.0f);
    }
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

    static constexpr std::string_view slots[4][3] { { "mod1_src", "mod1_dst", "mod1_amt" }, { "mod2_src", "mod2_dst", "mod2_amt" },
                                                    { "mod3_src", "mod3_dst", "mod3_amt" }, { "mod4_src", "mod4_dst", "mod4_amt" } };
    for (size_t k = 0; k < 4; ++k)
        p.matrix[k] = { (s8::ModSrc) n (slots[k][0]), (s8::ModDst) n (slots[k][1]), f (slots[k][2]) };

    auto& fx = (&layer == &synth.upper ? synth.upperFx : synth.lowerFx).p;
    fx.driveOn = b ("fx_drive_on"); fx.drive = f ("fx_drive"); fx.driveTone = f ("fx_drive_tone"); fx.driveMix = f ("fx_drive_mix");
    fx.chorusOn = b ("fx_chorus_on"); fx.chorusMode = n ("fx_chorus_mode"); fx.chorusMix = f ("fx_chorus_mix");
    fx.phaserOn = b ("fx_phaser_on"); fx.phaserRate = f ("fx_phaser_rate"); fx.phaserDepth = f ("fx_phaser_depth");
    fx.phaserFeedback = f ("fx_phaser_fb"); fx.phaserMix = f ("fx_phaser_mix");
    fx.delayOn = b ("fx_delay_on"); fx.delaySync = b ("fx_delay_sync"); fx.delayDiv = n ("fx_delay_div"); fx.delayMs = f ("fx_delay_ms");
    fx.delayFeedback = f ("fx_delay_fb"); fx.delayDamp = f ("fx_delay_damp"); fx.delayMix = f ("fx_delay_mix"); fx.pingPong = b ("fx_delay_pp");

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
    auto& gf = synth.globalFx.p;
    gf.eqOn = b ("eq_on"); gf.eqLow = f ("eq_low"); gf.eqHigh = f ("eq_high");
    gf.reverbOn = b ("rv_on"); gf.reverbSize = f ("rv_size"); gf.reverbDamp = f ("rv_damp");
    gf.reverbPredelay = f ("rv_predelay"); gf.reverbMix = f ("rv_mix");
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

    const int quality = 1 << juce::jlimit (0, 2, (int) std::lround (global.at ("quality")->load()));
    if (quality != factor) applyQuality (quality);

    const int n = buffer.getNumSamples();
    float minGain = 1.0f;
    auto midiIt = midi.cbegin();
    // Calcula un trozo de "count" muestras internas (ya sobremuestreadas) que empieza en "offset" del bloque.
    auto render = [&] (float* L, float* R, int count, int offset)
    {
        for (int i = 0; i < count; ++i)
        {
            bool controlsChanged = i == 0;
            for (; midiIt != midi.cend() && ((*midiIt).samplePosition - offset) * factor <= i; ++midiIt)
                handleMidi ((*midiIt).getMessage(), controlsChanged);
            if (controlsChanged) synth.updatePlay();
            float l = 0, r = 0;
            synth.process (l, r);
            minGain = std::min (minGain, synth.lastReduction());
            L[i] = l; R[i] = r;
        }
    };

    if (n > work.getNumSamples()) work.setSize (2, n, false, false, true);   // bloque mayor de lo anunciado
    work.clear();
    // Por trozos de como mucho maxBlock muestras (lo que admite el sobremuestreo preparado).
    for (int offset = 0; offset < n; offset += maxBlock)
    {
        const int len = juce::jmin (maxBlock, n - offset);
        float* chans[2] { work.getWritePointer (0) + offset, work.getWritePointer (1) + offset };
        if (factor == 1)
            render (chans[0], chans[1], len, offset);
        else
        {
            auto& os = *oversamplers[factor == 2 ? 0 : 1];
            juce::dsp::AudioBlock<float> block (chans, 2, (size_t) len);
            auto up = os.processSamplesUp (block);
            render (up.getChannelPointer (0), up.getChannelPointer (1), (int) up.getNumSamples(), offset);
            os.processSamplesDown (block);
        }
    }
    for (; midiIt != midi.cend(); ++midiIt) { bool dummy = false; handleMidi ((*midiIt).getMessage(), dummy); }
    if (buffer.getNumChannels() > 1)
    {
        buffer.copyFrom (0, 0, work, 0, 0, n);
        buffer.copyFrom (1, 0, work, 1, 0, n);
    }
    else
    {
        buffer.copyFrom (0, 0, work, 0, 0, n);
        buffer.applyGain (0, 0, n, 0.5f);
        buffer.addFrom (0, 0, work, 1, 0, n, 0.5f);
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

juce::ValueTree Saturno8Processor::makeState()
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
    state.setProperty ("preset", presetName, nullptr);
    return state;
}

void Saturno8Processor::applyState (juce::ValueTree tree)
{
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
    if (tree.hasProperty ("preset")) presetName = tree.getProperty ("preset").toString();
    // Los parámetros que falten (estados de versiones anteriores) vuelven a su valor por defecto.
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            bool found = false;
            for (auto child : tree)
                if (child.hasType ("PARAM") && child.getProperty ("id").toString() == rp->getParameterID()) { found = true; break; }
            if (! found) tree.appendChild (juce::ValueTree ("PARAM", { { "id", rp->getParameterID() }, { "value", rp->convertFrom0to1 (rp->getDefaultValue()) } }), nullptr);
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

void Saturno8Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = makeState();
    juce::StringArray map;
    for (int cc = 0; cc < 128; ++cc)
        if (auto* p = ccMap[(size_t) cc].load()) map.add (juce::String (cc) + "=" + p->getParameterID());
    state.setProperty ("midimap", map.joinIntoString (";"), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void Saturno8Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            for (auto& m : ccMap) m.store (nullptr);
            for (auto& item : juce::StringArray::fromTokens (tree.getProperty ("midimap").toString(), ";", ""))
            {
                const int cc = item.upToFirstOccurrenceOf ("=", false, false).getIntValue();
                if (auto* p = apvts.getParameter (item.fromFirstOccurrenceOf ("=", false, false)); p != nullptr && cc >= 0 && cc < 128)
                    ccMap[(size_t) cc].store (p);
            }
            midiMapChanged.store (true);
            applyState (tree);
        }
}

// ------------------------------------------------------------------ presets
juce::File Saturno8Processor::userPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Saturno 8").getChildFile ("Presets");
}

juce::StringArray Saturno8Processor::factoryNames()
{
    juce::StringArray names;
    int count = 0;
    auto* list = s8::factoryPresets (count);
    for (int i = 0; i < count; ++i) names.add (juce::String (juce::CharPointer_UTF8 (list[i].name)));
    return names;
}

juce::Array<juce::File> Saturno8Processor::userPresets()
{
    auto files = userPresetFolder().findChildFiles (juce::File::findFiles, false, "*.s8preset");
    files.sort();
    return files;
}

void Saturno8Processor::loadFactoryPreset (int index)
{
    int count = 0;
    auto* list = s8::factoryPresets (count);
    if (index < 0 || index >= count) return;
    currentProgram = index;

    // Todo a su valor por defecto, salvo lo que no es sonido (MIDI, calidad, tempo).
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = rp->getParameterID();
            if (id == "quality" || id == "tempo" || id == "master") continue;
            rp->setValueNotifyingHost (rp->getDefaultValue());
        }
    for (auto& st : steps) st.reset();
    std::vector<int> chord { 0, 4, 7 };

    for (auto& token : juce::StringArray::fromTokens (juce::String (juce::CharPointer_UTF8 (list[index].values)), " ", ""))
    {
        const auto key = token.upToFirstOccurrenceOf ("=", false, false);
        const auto val = token.fromFirstOccurrenceOf ("=", false, false);
        if (key == "chord")
        {
            chord.clear();
            for (auto& t : juce::StringArray::fromTokens (val, ",", "")) chord.push_back (t.getIntValue());
        }
        else if (key == "useq" || key == "lseq")
        {
            auto& data = steps[key == "useq" ? 0 : 1];
            auto items = juce::StringArray::fromTokens (val, ",", "");
            for (int i = 0; i < juce::jmin (items.size(), s8::kSeqSteps); ++i)
            {
                s8::Step st;
                auto parts = juce::StringArray::fromTokens (items[i], ":", "");
                st.note = parts[0] == "-" ? 0 : parts[0].getIntValue();
                st.on = ! parts.contains ("-");
                st.tie = parts.contains ("L");
                st.accent = parts.contains ("A");
                data.set (i, st);
            }
        }
        else if (auto* p = apvts.getParameter (key))
            p->setValueNotifyingHost (p->convertTo0to1 (val.getFloatValue()));
    }
    {
        const juce::SpinLock::ScopedLockType lock (chordLock);
        for (int l = 0; l < 2; ++l) { chordCopy[l].assign (chord.begin(), chord.end()); pendingChord[l].assign (chord.begin(), chord.end()); }
        chordRestore.store (true);
    }
    presetName = juce::String (juce::CharPointer_UTF8 (list[index].name));
    presetChanged.store (true);
}

bool Saturno8Processor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    auto tree = juce::ValueTree::fromXml (*xml);
    tree.setProperty ("preset", file.getFileNameWithoutExtension(), nullptr);
    // La calidad, el tempo y el volumen general no cambian al cargar un preset.
    for (auto id : { "quality", "tempo", "master" })
        if (auto* p = apvts.getParameter (id))
            for (auto child : tree)
                if (child.getProperty ("id").toString() == id) child.setProperty ("value", p->convertFrom0to1 (p->getValue()), nullptr);
    applyState (tree);
    currentProgram = -1;
    presetChanged.store (true);
    return true;
}

bool Saturno8Processor::saveUserPreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto folder = userPresetFolder();
    if (! folder.createDirectory()) return false;
    presetName = clean;
    auto state = makeState();
    auto xml = state.createXml();
    if (xml == nullptr || ! xml->writeTo (folder.getChildFile (clean + ".s8preset"))) return false;
    currentProgram = -1;
    presetChanged.store (true);
    return true;
}

void Saturno8Processor::stepPreset (int delta)
{
    const auto factory = factoryNames();
    const auto user = userPresets();
    const int total = factory.size() + user.size();
    int index = currentProgram;
    if (index < 0)
        for (int i = 0; i < user.size(); ++i) if (user[i].getFileNameWithoutExtension() == presetName) index = factory.size() + i;
    index = ((index < 0 ? 0 : index + delta) % total + total) % total;
    if (index < factory.size()) loadFactoryPreset (index);
    else loadUserPreset (user[index - factory.size()]);
}

void Saturno8Processor::setCurrentProgram (int index) { loadFactoryPreset (index); }
int Saturno8Processor::getNumPrograms() { return factoryNames().size(); }
const juce::String Saturno8Processor::getProgramName (int index) { return factoryNames()[index]; }

// ------------------------------------------------------------------ MIDI learn
void Saturno8Processor::startMidiLearn (juce::RangedAudioParameter* p) { learnTarget.store (p); }
void Saturno8Processor::forgetMidi (juce::RangedAudioParameter* p)
{
    for (auto& m : ccMap) if (m.load() == p) m.store (nullptr);
    midiMapChanged.store (true);
}
int Saturno8Processor::ccFor (const juce::RangedAudioParameter* p) const
{
    for (int cc = 0; cc < 128; ++cc) if (ccMap[(size_t) cc].load() == p) return cc;
    return -1;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Saturno8Processor();
}
