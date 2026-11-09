#include "PluginEditor.h"

namespace
{
const juce::Colour kBg (0xff17181b), kPanel (0xff24262b), kInk (0xffe9e4da), kAccent (0xffe0662f), kLower (0xff3fa7c9);
constexpr int kMargin = 14, kHeader = 56, kTabsH = 30, kCell = 78, kCellH = 92, kRowH = kCellH + 30, kKeysH = 70, kFooter = 34;
constexpr int kGridH = 92, kLayerRowsCount = 4, kWidthCells = 17;

const std::vector<std::vector<const char*>> kLayerRows {
    { "VCO-1", "VCO-2", "Mod. VCO" },
    { "Mezcla", "VCF", "VCA" },
    { "ENV-1", "ENV-2", "LFO" },
    { "Asignación", "Arpegiador" },
};
const std::vector<std::vector<const char*>> kFxRows {
    { "Distorsión", "Chorus", "Phaser" },
    { "Delay" },
    { "Matriz" },
};
const std::vector<std::vector<const char*>> kGlobalRows {
    { "Teclado", "Máster" },
    { "Bender", "Portamento", "Ecualizador" },
    { "Deriva", "Reverb" },
};

juce::String utf8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }
// Celdas que ocupa un control (los desplegables con textos largos ocupan dos).
float cellsFor (const s8::ParamDef& d)
{
    const juce::String id (d.id);
    if (id == "key_mode" || id == "condition") return 2.0f;
    if (id.endsWith ("_src") || id.endsWith ("_dst")) return 1.5f;
    return 1.0f;
}
} // namespace

void Saturno8Editor::Control::attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    sAtt.reset(); cAtt.reset(); bAtt.reset();
    if (auto* s = dynamic_cast<juce::Slider*> (comp.get()))
        sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *s);
    else if (auto* c = dynamic_cast<juce::ComboBox*> (comp.get()))
        cAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, *c);
    else if (auto* b = dynamic_cast<juce::Button*> (comp.get()))
        bAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, id, *b);
}

void Saturno8Editor::build (const std::vector<s8::ParamDef>& defs, std::vector<std::unique_ptr<Control>>& out)
{
    for (auto& d : defs)
    {
        auto c = std::make_unique<Control>();
        c->def = &d;
        c->section = utf8 (d.section);
        c->label.setText (utf8 (d.name), juce::dontSendNotification);
        c->label.setJustificationType (juce::Justification::centred);
        c->label.setFont (juce::FontOptions (12.0f));
        c->label.setColour (juce::Label::textColourId, kInk.withAlpha (0.8f));
        addChildComponent (c->label);

        if (d.type == s8::PType::Choice)
        {
            auto box = std::make_unique<juce::ComboBox>();
            box->addItemList (s8::choicesOf (d), 1);
            c->comp = std::move (box);
        }
        else if (d.type == s8::PType::Bool)
            c->comp = std::make_unique<juce::ToggleButton>();
        else
        {
            auto knob = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow);
            knob->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
            knob->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            c->comp = std::move (knob);
        }
        addChildComponent (*c->comp);
        out.push_back (std::move (c));
    }
}

Saturno8Editor::Saturno8Editor (Saturno8Processor& p)
    : AudioProcessorEditor (&p), proc (p),
      grids { { p.steps[0], p.currentStep[0] }, { p.steps[1], p.currentStep[1] } },
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    for (auto& gr : grids) addChildComponent (gr);
    grids[1].setColour (kLower);
    build (s8::layerParamDefs(), layerControls);
    build (s8::layerFxParamDefs(), fxControls);
    build (s8::globalParamDefs(), globalControls);
    for (auto& c : globalControls) c->attach (proc.apvts, c->def->id);

    tabs.addTab ("Capa superior", kPanel, 0);
    tabs.addTab ("Capa inferior", kPanel, 1);
    tabs.addTab ("Efectos sup.", kPanel, 2);
    tabs.addTab ("Efectos inf.", kPanel, 3);
    tabs.addTab ("Global", kPanel, 4);
    addAndMakeVisible (tabs);

    chordButton.setTooltip ("Mantén pulsadas las notas del acorde y pulsa aquí");
    chordButton.onClick = [this] { proc.requestChordLearn (page == 1 ? 1 : 0); };
    addAndMakeVisible (chordButton);
    status.setColour (juce::Label::textColourId, kInk.withAlpha (0.7f));
    addAndMakeVisible (status);
    keyboard.setAvailableRange (24, 108);
    keyboard.setOctaveForMiddleC (4);
    addAndMakeVisible (keyboard);

    setSize (kMargin * 2 + kWidthCells * kCell + 2 * 16 + 4, kHeader + kTabsH + 8 + kLayerRowsCount * kRowH + 24 + kGridH + 8 + kFooter + kKeysH + kMargin);
    tabs.onChange = [this] (int index) { if (index != page) showPage (index); };
    showPage (0);
    timerCallback();
    startTimerHz (10);
}

void Saturno8Editor::showPage (int newPage)
{
    page = juce::jlimit (0, 4, newPage);
    if (tabs.getCurrentTabIndex() != page) tabs.setCurrentTabIndex (page, false);
    const bool isLayer = page < 2, isFx = page == 2 || page == 3;
    const int layer = isFx ? page - 2 : page;
    const juce::Colour colour = (isLayer || isFx) && layer == 1 ? kLower : kAccent;
    auto show = [&] (std::vector<std::unique_ptr<Control>>& list, bool visible, bool attachToLayer)
    {
        for (auto& c : list)
        {
            if (visible && attachToLayer) c->attach (proc.apvts, Saturno8Processor::pid (Saturno8Processor::kPrefix[layer], c->def->id));
            if (auto* sl = dynamic_cast<juce::Slider*> (c->comp.get())) sl->setColour (juce::Slider::rotarySliderFillColourId, colour);
            c->comp->setVisible (visible);
            c->label.setVisible (visible);
        }
    };
    show (layerControls, isLayer, true);
    show (fxControls, isFx, true);
    show (globalControls, page == 4, false);
    chordButton.setVisible (isLayer);
    grids[0].setVisible (page == 0);
    grids[1].setVisible (page == 1);
    resized();
    repaint();
    timerCallback();
}

void Saturno8Editor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kInk);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("SATURNO 8", kMargin, 12, 300, 32, juce::Justification::left);
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (kInk.withAlpha (0.55f));
    g.drawText (utf8 ("Fase 5: efectos por capa y globales, y matriz de modulación"), kMargin + 170, 22, 500, 18, juce::Justification::left);

    const juce::Colour colour = page == 1 || page == 3 ? kLower : kAccent;
    for (auto& [name, box] : sectionBoxes)
    {
        auto r = box.toFloat();
        g.setColour (kPanel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (colour);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (name.toUpperCase().replace (utf8 ("\xc3\xb3"), utf8 ("\xc3\x93")).replace (utf8 ("\xc3\xa1"), utf8 ("\xc3\x81")),
                    r.removeFromTop (20).reduced (8, 0), juce::Justification::left);
        if (name == "Secuencia")
        {
            g.setColour (kInk.withAlpha (0.5f));
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (utf8 ("Suena con el modo Secuenciador, transportada desde la tecla más grave · LED: paso o silencio · "
                              "arrastra la nota para cambiar los semitonos · L: ligadura · A: acento"),
                        box.withHeight (20).withTrimmedLeft (90).toFloat(), juce::Justification::left);
        }
    }
}

void Saturno8Editor::layoutRows (const Rows& rows, std::vector<std::unique_ptr<Control>>& controls, int top)
{
    int y = top;
    for (auto& row : rows)
    {
        int x = kMargin;
        for (auto* sName : row)
        {
            const juce::String section = utf8 (sName);
            int cx = x + 4;
            for (auto& c : controls)
            {
                if (c->section != section) continue;
                const int w = juce::roundToInt (cellsFor (*c->def) * (float) kCell);
                auto cell = juce::Rectangle<int> (cx, y + 38, w, kCellH - 14);
                c->label.setBounds (cx, y + 20, w, 16);
                if (dynamic_cast<juce::Slider*> (c->comp.get()) != nullptr) c->comp->setBounds (cell.reduced (4, 0));
                else if (dynamic_cast<juce::ComboBox*> (c->comp.get()) != nullptr) c->comp->setBounds (cell.withSizeKeepingCentre (w - 6, 24));
                else c->comp->setBounds (cell.withSizeKeepingCentre (26, 26));
                cx += w;
            }
            sectionBoxes.push_back ({ section, { x, y, cx - x + 4, kCellH + 24 } });
            x = cx + 12;
        }
        y += kRowH;
    }
}

void Saturno8Editor::resized()
{
    sectionBoxes.clear();
    tabs.setBounds (kMargin, kHeader, 640, kTabsH);
    const int top = kHeader + kTabsH + 8;
    if (page < 2) layoutRows (kLayerRows, layerControls, top);
    else if (page < 4) layoutRows (kFxRows, fxControls, top);
    else layoutRows (kGlobalRows, globalControls, top);

    const int gridTop = top + kLayerRowsCount * kRowH;
    if (page < 2)
    {
        sectionBoxes.push_back ({ utf8 ("Secuencia"),
                                  { kMargin, gridTop, getWidth() - 2 * kMargin, kGridH + 24 } });
        grids[page].setBounds (kMargin + 6, gridTop + 22, getWidth() - 2 * kMargin - 12, kGridH - 2);
    }
    const int y = gridTop + kGridH + 32;
    chordButton.setBounds (kMargin, y, 140, 26);
    status.setBounds (kMargin + 150, y, getWidth() - 2 * kMargin - 150, 26);
    keyboard.setBounds (kMargin, y + kFooter, getWidth() - 2 * kMargin, kKeysH);
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 50.0f);   // 50 teclas blancas: Do 1 a Do 8
}

void Saturno8Editor::timerCallback()
{
    juce::String text;
    if (page < 2)
    {
        text << "Acorde: " << proc.chordDescription (page) << "   ·   ";
        if (auto* len = proc.apvts.getRawParameterValue (Saturno8Processor::pid (Saturno8Processor::kPrefix[page], "seq_len")))
            grids[page].setLength ((int) len->load());
    }
    text << "Voces: " << proc.activeVoices[0].load() << " sup. + " << proc.activeVoices[1].load() << " inf.";
    const float red = proc.limiterReduction.load();
    if (red < 0.99f) text << "   ·   Limitador: " << juce::String (juce::Decibels::gainToDecibels (red), 1) << " dB";
    status.setText (text, juce::dontSendNotification);
}
