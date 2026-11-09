// Genera una captura PNG de la interfaz sin abrir ventanas (para documentación y revisión).
// Uso: ui_snapshot <salida.png> [página 0 superior|1 inferior|2 global] [escala]
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
    // Un acorde de prueba: comprueba también que el procesador suena.
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int n : { 60, 64, 67 }) midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
    float peak = 0;
    for (int b = 0; b < 20; ++b) { proc.processBlock (buf, midi); midi.clear(); peak = std::max (peak, buf.getMagnitude (0, 512)); }
    std::printf ("Pico de audio: %.3f, voces: %d sup. + %d inf.\n", peak, proc.activeVoices[0].load(), proc.activeVoices[1].load());
    if (peak <= 0.01f || proc.activeVoices[0].load() != 3 || proc.activeVoices[1].load() != 3) return 1;
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    if (auto* s8ed = dynamic_cast<Saturno8Editor*> (ed.get())) s8ed->showPageForSnapshot (page);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
    juce::FileOutputStream os (out);
    os.setPosition (0); os.truncate();
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("%s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    return 0;
}
