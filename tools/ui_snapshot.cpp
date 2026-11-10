// Genera una captura PNG de la interfaz sin abrir ventanas (para documentación y revisión).
// Uso: ui_snapshot <salida.png> [página 0 superior|1 inferior|2 efectos sup.|3 efectos inf.|4 global] [escala]
#include <JuceHeader.h>
#include "../src/PluginProcessor.h"
#include "../src/PluginEditor.h"
#include "../src/FactoryPresets.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "ui.png"));
    const int page = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 1.0f;

    // Comprobación de los presets de fábrica: claves válidas y que suenan sin NaN, con x1 y x4.
    {
        int count = 0;
        auto* list = s8::factoryPresets (count);
        Saturno8Processor test;
        for (int q : { 0, 2 })
        {
            test.prepareToPlay (48000, 512);
            if (auto* p = test.apvts.getParameter ("quality")) p->setValueNotifyingHost (p->convertTo0to1 ((float) q));
            for (int i = 0; i < count; ++i)
            {
                for (auto& token : juce::StringArray::fromTokens (juce::String (juce::CharPointer_UTF8 (list[i].values)), " ", ""))
                {
                    const auto key = token.upToFirstOccurrenceOf ("=", false, false);
                    if (key != "chord" && key != "useq" && key != "lseq" && test.apvts.getParameter (key) == nullptr)
                    { std::printf ("Preset '%s': parámetro desconocido %s\n", list[i].name, key.toRawUTF8()); return 1; }
                }
                test.loadFactoryPreset (i);
                juce::AudioBuffer<float> buf (2, 512);
                juce::MidiBuffer midi;
                float peak = 0; bool finite = true;
                for (int b = 0; b < 120; ++b)
                {
                    midi.clear();
                    if (b == 1) for (int n : { 48, 60, 64, 67 }) midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 10);
                    if (b == 80) for (int n : { 48, 60, 64, 67 }) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
                    test.processBlock (buf, midi);
                    peak = std::max (peak, buf.getMagnitude (0, 512));
                    for (int c = 0; c < 2; ++c) for (int k = 0; k < 512; ++k) finite = finite && std::isfinite (buf.getSample (c, k));
                }
                std::printf ("x%d %-26s pico %.2f\n", 1 << q, list[i].name, peak);
                if (! finite || peak < 0.02f || peak > 1.0f) { std::printf ("  ^ fallo\n"); return 1; }
            }
        }
    }

    // Ida y vuelta del estado: preset de fábrica + paso editado + mapa MIDI.
    {
        Saturno8Processor a, b;
        a.loadFactoryPreset (8);
        a.steps[1].set (3, { 5, true, true, false });
        a.startMidiLearn (a.apvts.getParameter ("u_cutoff"));
        juce::AudioBuffer<float> buf (2, 64);
        juce::MidiBuffer midi; midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 64), 0);
        a.prepareToPlay (48000, 64); a.processBlock (buf, midi);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        for (auto* p : a.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (std::abs (rp->getValue() - b.apvts.getParameter (rp->getParameterID())->getValue()) > 1e-6f)
                { std::printf ("Estado: %s no coincide\n", rp->getParameterID().toRawUTF8()); return 1; }
        const auto st = b.steps[1].get (3);
        if (st.note != 5 || ! st.tie || b.steps[0].get (2).note != 12 || b.ccFor (b.apvts.getParameter ("u_cutoff")) != 74
            || b.currentPresetName() != a.currentPresetName())
        { std::printf ("Estado: secuencia, MIDI o nombre no coinciden\n"); return 1; }
        std::printf ("Estado: ida y vuelta correcta (%s, CC 74 -> corte)\n", b.currentPresetName().toRawUTF8());
    }

    Saturno8Processor proc;
    proc.prepareToPlay (48000, 512);
    // Modo doble con la capa inferior una octava abajo, para ver las dos capas en marcha.
    if (auto* p = proc.apvts.getParameter ("key_mode")) p->setValueNotifyingHost (p->convertTo0to1 (1));
    if (auto* p = proc.apvts.getParameter ("l_vco1_range")) p->setValueNotifyingHost (p->convertTo0to1 (0));
    // Arpegiador en la superior y una secuencia de ejemplo en la inferior.
    if (auto* p = proc.apvts.getParameter ("u_arp_mode")) p->setValueNotifyingHost (p->convertTo0to1 (1));
    if (auto* p = proc.apvts.getParameter ("l_arp_mode")) p->setValueNotifyingHost (p->convertTo0to1 (2));
    if (auto* p = proc.apvts.getParameter ("l_seq_len")) p->setValueNotifyingHost (p->convertTo0to1 (12));
    {
        const int notes[] { 0, 0, 12, 0, 7, 0, 10, 12, 0, 3, 5, 7 };
        for (int i = 0; i < 12; ++i)
            proc.steps[1].set (i, { notes[i], i != 5, i == 8, i % 4 == 0 });
    }
    // Unos efectos y una ranura de la matriz, para la captura.
    for (auto* id : { "u_fx_chorus_on", "u_fx_delay_on", "rv_on" })
        if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (1.0f);
    if (auto* p = proc.apvts.getParameter ("u_mod1_src")) p->setValueNotifyingHost (p->convertTo0to1 (4));
    if (auto* p = proc.apvts.getParameter ("u_mod1_dst")) p->setValueNotifyingHost (p->convertTo0to1 (8));
    if (auto* p = proc.apvts.getParameter ("u_mod1_amt")) p->setValueNotifyingHost (p->convertTo0to1 (0.4f));
    // Un acorde de prueba: comprueba también que el procesador suena.
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int n : { 60, 64, 67 }) midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
    float peak = 0;
    for (int b = 0; b < 20; ++b) { proc.processBlock (buf, midi); midi.clear(); peak = std::max (peak, buf.getMagnitude (0, 512)); }
    std::printf ("Pico de audio: %.3f, voces: %d sup. + %d inf.\n", peak, proc.activeVoices[0].load(), proc.activeVoices[1].load());
    if (peak <= 0.01f || proc.activeVoices[0].load() < 1 || proc.activeVoices[1].load() < 1) return 1;
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    if (auto* s8ed = dynamic_cast<Saturno8Editor*> (ed.get())) s8ed->showPageForSnapshot (page);
    ed->setSize (juce::roundToInt ((float) ed->getWidth() / 0.8f), juce::roundToInt ((float) ed->getHeight() / 0.8f));   // al 100 %
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
    juce::FileOutputStream os (out);
    os.setPosition (0); os.truncate();
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("%s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    return 0;
}
