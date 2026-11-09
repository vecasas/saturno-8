#pragma once
#include <JuceHeader.h>
#include "Params.h"
#include <string_view>
#include <unordered_map>

// Saturno 8: sintetizador polifónico analógico modelado (diseño propio).
// Dos capas con arpegiador o secuenciador, efectos, matriz de modulación, presets,
// MIDI learn y sobremuestreo.
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

    // Programas del DAW = presets de fábrica.
    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentProgram); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
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

    // Presets: de fábrica (en el código) y del usuario (Documentos/Saturno 8/Presets/*.s8preset).
    static juce::File userPresetFolder();
    static juce::StringArray factoryNames();
    static juce::Array<juce::File> userPresets();
    void loadFactoryPreset (int index);
    bool loadUserPreset (const juce::File&);
    bool saveUserPreset (const juce::String& name);
    void stepPreset (int delta);
    juce::String currentPresetName() const { return presetName; }
    std::atomic<bool> presetChanged { false };

    // MIDI learn: el siguiente CC que llegue controlará el parámetro.
    void startMidiLearn (juce::RangedAudioParameter*);
    void forgetMidi (juce::RangedAudioParameter*);
    int ccFor (const juce::RangedAudioParameter*) const;
    bool isLearning (const juce::RangedAudioParameter* p) const { return learnTarget.load() == p; }
    std::atomic<bool> midiMapChanged { false };

private:
    juce::ValueTree makeState();
    void applyState (juce::ValueTree);
    void applyQuality (int newFactor);
    void handleMidi (const juce::MidiMessage&, bool& controlsChanged);

    double hostRate = 48000;
    int maxBlock = 512, factor = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversamplers[2];   // x2 y x4
    juce::AudioBuffer<float> work;
    juce::String presetName { "Inicial" };
    int currentProgram = 0;
    std::array<std::atomic<juce::RangedAudioParameter*>, 128> ccMap {};
    std::atomic<juce::RangedAudioParameter*> learnTarget { nullptr };

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
