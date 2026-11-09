// Genera una captura PNG de la interfaz sin abrir ventanas (para documentación y revisión).
// Uso: ui_snapshot <salida.png> [página 0 superior|1 inferior|2 efectos sup.|3 efectos inf.|4 global] [escala]
#include <JuceHeader.h>
#include "../src/PluginProcessor.h"
#include "../src/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "ui.png"));
    const int page = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 1.0f;

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
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
    juce::FileOutputStream os (out);
    os.setPosition (0); os.truncate();
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("%s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    return 0;
}
