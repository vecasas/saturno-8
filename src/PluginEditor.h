#pragma once
#include "PluginProcessor.h"

// Fase 2: panel funcional por secciones (la interfaz definitiva llega en la fase 6).
class Saturno8Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Saturno8Editor (Saturno8Processor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Control
    {
        juce::String section;
        juce::Label label;
        std::unique_ptr<juce::Component> comp;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bAtt;
    };

    Saturno8Processor& proc;
    std::vector<std::unique_ptr<Control>> controls;
    juce::StringArray sections;
    std::vector<juce::Rectangle<int>> sectionBounds;
    juce::Slider masterKnob;
    juce::AudioProcessorValueTreeState::SliderAttachment masterAttach;
    juce::TextButton chordButton { "Aprender acorde" };
    juce::Label status;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Saturno8Editor)
};
