#pragma once
#include "PluginProcessor.h"
#include "StepGrid.h"

// Panel funcional por secciones con pestañas para cada capa y los controles globales
// (la interfaz definitiva llega en la fase 6).
class Saturno8Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Saturno8Editor (Saturno8Processor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void showPageForSnapshot (int p) { showPage (p); }

private:
    void timerCallback() override;
    void showPage (int page);

    struct Control
    {
        const s8::ParamDef* def = nullptr;
        juce::String section;
        juce::Label label;
        std::unique_ptr<juce::Component> comp;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bAtt;
        void attach (juce::AudioProcessorValueTreeState&, const juce::String& id);
    };
    using Rows = std::vector<std::vector<const char*>>;

    void build (const std::vector<s8::ParamDef>&, std::vector<std::unique_ptr<Control>>&);
    void layoutRows (const Rows&, std::vector<std::unique_ptr<Control>>&, int top);

    Saturno8Processor& proc;
    int page = 0;   // 0 = capa superior, 1 = capa inferior, 2 = global
    std::vector<std::unique_ptr<Control>> layerControls, globalControls;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> sectionBoxes;
    struct Tabs : juce::TabbedButtonBar
    {
        Tabs() : juce::TabbedButtonBar (TabsAtTop) {}
        std::function<void (int)> onChange;
        void currentTabChanged (int index, const juce::String&) override { if (onChange) onChange (index); }
    } tabs;
    StepGrid grids[2];
    juce::TextButton chordButton { "Aprender acorde" };
    juce::Label status;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Saturno8Editor)
};
