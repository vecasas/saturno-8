// Genera una captura PNG de la interfaz sin abrir ventanas (para documentación y revisión).
// Uso: ui_snapshot <salida.png> [escala]
#include <JuceHeader.h>
#include "../src/PluginProcessor.h"
#include "../src/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "ui.png"));
    const float scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 1.0f;

    Saturno8Processor proc;
    proc.prepareToPlay (48000, 512);
    // Un acorde de prueba: comprueba también que el procesador suena.
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    for (int n : { 60, 64, 67 }) midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
    float peak = 0;
    for (int b = 0; b < 20; ++b) { proc.processBlock (buf, midi); midi.clear(); peak = std::max (peak, buf.getMagnitude (0, 512)); }
    std::printf ("Pico de audio: %.3f, voces activas: %d\n", peak, proc.activeVoices.load());
    if (peak <= 0.01f || proc.activeVoices.load() != 3) return 1;
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
    juce::FileOutputStream os (out);
    os.setPosition (0); os.truncate();
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("%s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    return 0;
}
