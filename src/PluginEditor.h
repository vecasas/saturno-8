#pragma once
#include "PluginProcessor.h"
#include "StepGrid.h"
#include "ui/ChoiceKeys.h"

// Panel de Saturno 8 a tamaño base; el editor lo escala del 50 % al 200 %.
class SaturnoPanel : public juce::Component, private juce::Timer
{
public:
    explicit SaturnoPanel (Saturno8Processor&);
    ~SaturnoPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void showPage (int page);
    int baseWidth() const { return width; }
    int baseHeight() const { return height; }

private:
    void timerCallback() override;
    void mouseDown (const juce::MouseEvent&) override;
    void showPresetMenu();
    void savePresetDialog();

    enum class Kind { Fader, Switch, Keys, Lcd };
    struct Control
    {
        const s8::ParamDef* def = nullptr;
        Kind kind = Kind::Fader;
        juce::String section;
        int width = 48;
        juce::Label label, value;
        std::unique_ptr<juce::Component> comp;
        juce::RangedAudioParameter* param = nullptr;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bAtt;
        void attach (juce::AudioProcessorValueTreeState&, const juce::String& id);
        void refreshValue();
    };
    using Rows = std::vector<std::vector<const char*>>;

    void build (const std::vector<s8::ParamDef>&, std::vector<std::unique_ptr<Control>>&);
    int rowWidth (const Rows&, const std::vector<std::unique_ptr<Control>>&) const;
    void layoutRows (const Rows&, std::vector<std::unique_ptr<Control>>&, int top);

    Saturno8Processor& proc;
    s8ui::LookAndFeel lnf;
    int page = 0, width = 1200, height = 800;
    std::vector<std::unique_ptr<Control>> layerControls, fxControls, globalControls;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> sectionBoxes;
    struct Tabs : juce::TabbedButtonBar
    {
        Tabs() : juce::TabbedButtonBar (TabsAtTop) {}
        std::function<void (int)> onChange;
        void currentTabChanged (int index, const juce::String&) override { if (onChange) onChange (index); }
    } tabs;
    StepGrid grids[2];
    juce::TextButton chordButton { "Aprender acorde" };
    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, presetButton, savePreset { "Guardar" };
    std::unique_ptr<juce::AlertWindow> saveWindow;
    juce::String lcdLine1, lcdLine2;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaturnoPanel)
};

class Saturno8Editor : public juce::AudioProcessorEditor
{
public:
    explicit Saturno8Editor (Saturno8Processor&);
    void resized() override;
    void showPageForSnapshot (int p) { panel.showPage (p); }

private:
    SaturnoPanel panel;
    juce::TooltipWindow tooltips { this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Saturno8Editor)
};
