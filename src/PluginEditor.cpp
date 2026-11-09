#include "PluginEditor.h"

namespace
{
const juce::Colour kBg (0xff17181b), kPanel (0xff24262b), kInk (0xffe9e4da), kAccent (0xffe0662f);
constexpr int kMargin = 14, kHeader = 56, kCell = 78, kCellH = 92, kKeysH = 70, kFooter = 34;
// Secciones por fila del panel.
const std::vector<std::vector<const char*>> kRows {
    { "VCO-1", "VCO-2", "Mod. VCO" },
    { "Mezcla", "VCF", "VCA" },
    { "ENV-1", "ENV-2", "LFO" },
    { "Asignación" },
};
} // namespace

Saturno8Editor::Saturno8Editor (Saturno8Processor& p)
    : AudioProcessorEditor (&p), proc (p),
      masterAttach (p.apvts, "master", masterKnob),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    for (auto& d : s8::layerParamDefs())
    {
        auto c = std::make_unique<Control>();
        c->section = juce::String (juce::CharPointer_UTF8 (d.section));
        sections.addIfNotAlreadyThere (c->section);
        const auto id = Saturno8Processor::pid ("u_", d.id);
        c->label.setText (juce::String (juce::CharPointer_UTF8 (d.name)), juce::dontSendNotification);
        c->label.setJustificationType (juce::Justification::centred);
        c->label.setFont (juce::FontOptions (12.0f));
        c->label.setColour (juce::Label::textColourId, kInk.withAlpha (0.8f));
        addAndMakeVisible (c->label);

        if (d.type == s8::PType::Choice)
        {
            auto box = std::make_unique<juce::ComboBox>();
            box->addItemList (s8::choicesOf (d), 1);
            c->cAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *box);
            c->comp = std::move (box);
        }
        else if (d.type == s8::PType::Bool)
        {
            auto btn = std::make_unique<juce::ToggleButton>();
            c->bAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, *btn);
            c->comp = std::move (btn);
        }
        else
        {
            auto knob = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow);
            knob->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
            knob->setColour (juce::Slider::rotarySliderFillColourId, kAccent);
            knob->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            c->sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *knob);
            c->comp = std::move (knob);
        }
        addAndMakeVisible (*c->comp);
        controls.push_back (std::move (c));
    }

    masterKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    masterKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    masterKnob.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    addAndMakeVisible (masterKnob);

    chordButton.setTooltip ("Mantén pulsadas las notas del acorde y pulsa aquí");
    chordButton.onClick = [this] { proc.requestChordLearn(); };
    addAndMakeVisible (chordButton);
    status.setColour (juce::Label::textColourId, kInk.withAlpha (0.7f));
    addAndMakeVisible (status);
    keyboard.setAvailableRange (24, 108);
    keyboard.setOctaveForMiddleC (4);
    addAndMakeVisible (keyboard);

    // Ancho: la fila más ancha (en celdas) + márgenes.
    int maxCells = 0;
    for (auto& row : kRows)
    {
        int cells = 0;
        for (auto* s : row)
            for (auto& c : controls) cells += c->section == juce::String (juce::CharPointer_UTF8 (s)) ? 1 : 0;
        maxCells = std::max (maxCells, cells);
    }
    setSize (kMargin * 2 + maxCells * kCell + 3 * 16 + 4, kHeader + (int) kRows.size() * (kCellH + 30) + kFooter + kKeysH + kMargin);
    timerCallback();
    startTimerHz (10);
}

void Saturno8Editor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kInk);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("SATURNO 8", kMargin, 12, 300, 32, juce::Justification::left);
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (kInk.withAlpha (0.55f));
    g.drawText ("Fase 2: motor de voz (una capa, 16 voces)", kMargin + 170, 22, 400, 18, juce::Justification::left);

    for (size_t i = 0; i < sectionBounds.size(); ++i)
    {
        auto r = sectionBounds[i].toFloat();
        g.setColour (kPanel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (kAccent);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (sections[(int) i].toUpperCase().replace (juce::CharPointer_UTF8 ("\xc3\xb3"), juce::CharPointer_UTF8 ("\xc3\x93")), r.removeFromTop (20).reduced (8, 0), juce::Justification::left);
    }
}

void Saturno8Editor::resized()
{
    sectionBounds.assign ((size_t) sections.size(), {});
    int y = kHeader;
    for (auto& row : kRows)
    {
        int x = kMargin;
        for (auto* sName : row)
        {
            const juce::String section = juce::String (juce::CharPointer_UTF8 (sName));
            const int idx = sections.indexOf (section);
            int cx = x + 4;
            for (auto& c : controls)
            {
                if (c->section != section) continue;
                c->label.setBounds (cx, y + 20, kCell, 16);
                auto cell = juce::Rectangle<int> (cx, y + 38, kCell, kCellH - 14);
                if (dynamic_cast<juce::Slider*> (c->comp.get()) != nullptr) c->comp->setBounds (cell.reduced (6, 0));
                else if (dynamic_cast<juce::ComboBox*> (c->comp.get()) != nullptr) c->comp->setBounds (cell.withSizeKeepingCentre (kCell - 6, 24));
                else c->comp->setBounds (cell.withSizeKeepingCentre (26, 26));
                cx += kCell;
            }
            if (idx >= 0) sectionBounds[(size_t) idx] = { x, y, cx - x + 4, kCellH + 24 };
            x = cx + 12;
        }
        y += kCellH + 30;
    }
    masterKnob.setBounds (getWidth() - kMargin - 44, 8, 44, 44);
    chordButton.setBounds (kMargin, y, 140, 26);
    status.setBounds (kMargin + 150, y, getWidth() - 2 * kMargin - 150, 26);
    keyboard.setBounds (kMargin, y + kFooter, getWidth() - 2 * kMargin, kKeysH);
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 50.0f);   // 50 teclas blancas: Do 1 a Do 8
}

void Saturno8Editor::timerCallback()
{
    status.setText ("Acorde: " + proc.chordDescription() + "   ·   Voces activas: " + juce::String (proc.activeVoices.load()),
                    juce::dontSendNotification);
}
