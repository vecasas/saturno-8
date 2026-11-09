#pragma once
#include <JuceHeader.h>
#include "Params.h"
#include <string_view>
#include <unordered_map>

// Saturno 8: sintetizador polifónico analógico modelado (diseño propio).
// Fase 5: dos capas con arpegiador o secuenciador, efectos por capa y globales, y matriz de modulación.
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
    double getTailLengthSeconds() const override { return 8.0; }

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
    static constexpr const char* kPrefix[2] { "u_", "l_" };   // capa superior, capa inferior

    // Teclado de la interfaz (también lo usa la app standalone sin controlador).
    juce::MidiKeyboardState keyboardState;

    // Memoria de acorde: la interfaz lo pide y el hilo de audio lo aplica (0 = superior, 1 = inferior).
    void requestChordLearn (int layer) { chordLearnRequest.store (layer); }
    juce::String chordDescription (int layer) const;
    std::atomic<int> activeVoices[2] { 0, 0 };
    std::atomic<float> limiterReduction { 1.0f };

    // Pasos del secuenciador de cada capa (los edita la interfaz sin bloqueos).
    s8::StepData steps[2];
    std::atomic<int> currentStep[2] { -1, -1 };

private:
    using RawMap = std::unordered_map<std::string_view, std::atomic<float>*>;
    void readLayer (s8::Layer&, const RawMap&, int& lastAssign);
    void readGlobal();

    s8::Synth synth;
    RawMap raw[2], global;                  // id sin prefijo -> valor
    std::atomic<int> chordLearnRequest { -1 };
    mutable juce::SpinLock chordLock;
    std::vector<int> chordCopy[2];          // para guardar el estado fuera del hilo de audio
    std::vector<int> pendingChord[2];
    std::atomic<bool> chordRestore { false };
    int lastAssign[2] { -1, -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Saturno8Processor)
};
