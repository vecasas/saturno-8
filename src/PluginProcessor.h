#pragma once
#include <JuceHeader.h>
#include "Params.h"
#include <string_view>
#include <unordered_map>

// Saturno 8: sintetizador polifónico analógico modelado (diseño propio).
// Fase 2: motor de voz de una capa con 16 voces y modos de asignación.
class Saturno8Processor : public juce::AudioProcessor
{
public:
    Saturno8Processor();
    ~Saturno8Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::String pid (const char* layerPrefix, const char* id) { return juce::String (layerPrefix) + id; }

    // Teclado de la interfaz (también lo usa la app standalone sin controlador).
    juce::MidiKeyboardState keyboardState;

    // Memoria de acorde: la interfaz lo pide y el hilo de audio lo aplica.
    void requestChordLearn() { chordLearnRequest.store (true); }
    juce::String chordDescription() const;
    std::atomic<int> activeVoices { 0 };

private:
    void readParams();

    s8::Layer upper;
    std::unordered_map<std::string_view, std::atomic<float>*> raw;   // id sin prefijo -> valor
    std::atomic<float>* master = nullptr;
    std::atomic<bool> chordLearnRequest { false };
    mutable juce::SpinLock chordLock;
    std::vector<int> chordCopy { 0, 4, 7 };   // para guardar el estado fuera del hilo de audio
    std::vector<int> pendingChord;
    std::atomic<bool> chordRestore { false };
    int lastAssign = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Saturno8Processor)
};
