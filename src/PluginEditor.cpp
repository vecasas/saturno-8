#include "PluginEditor.h"

namespace
{
using namespace s8ui;
constexpr int kMargin = 16, kHeader = 74, kTabsH = 28, kSecHeader = 20, kLabelH = 15, kCtrlH = 104, kValueH = 16;
constexpr int kSecH = kSecHeader + kLabelH + kCtrlH + kValueH + 6, kRowGap = 8, kSecGap = 8, kPad = 6;
constexpr int kGridH = 92, kFooter = 36, kKeysH = 74;

const std::vector<std::vector<const char*>> kLayerRows {
    { "VCO-1", "VCO-2", "Mod. VCO", "Mezcla" },
    { "VCF", "VCA", "ENV-1", "ENV-2" },
    { "LFO", "Asignación", "Arpegiador" },
};
const std::vector<std::vector<const char*>> kFxRows {
    { "Distorsión", "Chorus", "Phaser" },
    { "Delay" },
    { "Matriz" },
};
const std::vector<std::vector<const char*>> kGlobalRows {
    { "Teclado", "Máster", "Ecualizador" },
    { "Bender", "Portamento" },
    { "Deriva", "Reverb" },
};

juce::String utf8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }
juce::String upper (const juce::String& s)
{
    return s.toUpperCase().replace (utf8 ("\xc3\xb3"), utf8 ("\xc3\x93")).replace (utf8 ("\xc3\xa1"), utf8 ("\xc3\x81"))
            .replace (utf8 ("\xc3\xad"), utf8 ("\xc3\x8d")).replace (utf8 ("\xc3\xa9"), utf8 ("\xc3\x89"));
}
} // namespace

// ------------------------------------------------------------------ controles
void SaturnoPanel::Control::attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    sAtt.reset(); cAtt.reset(); bAtt.reset();
    param = apvts.getParameter (id);
    if (auto* s = dynamic_cast<juce::Slider*> (comp.get()))
        sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *s);
    else if (auto* c = dynamic_cast<juce::ComboBox*> (comp.get()))
        cAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, *c);
    else if (auto* k = dynamic_cast<ChoiceKeys*> (comp.get()))
        k->attach (*param);
    else if (auto* b = dynamic_cast<juce::Button*> (comp.get()))
        bAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, id, *b);
    refreshValue();
}

void SaturnoPanel::Control::refreshValue()
{
    if (kind == Kind::Fader && param != nullptr)
        value.setText (param->getCurrentValueAsText(), juce::dontSendNotification);
}

void SaturnoPanel::build (const std::vector<s8::ParamDef>& defs, std::vector<std::unique_ptr<Control>>& out)
{
    for (auto& d : defs)
    {
        auto c = std::make_unique<Control>();
        auto* ctl = c.get();
        c->def = &d;
        c->section = utf8 (d.section);
        const auto colour = sectionColour (c->section);
        c->label.setText (utf8 (d.name), juce::dontSendNotification);
        c->label.setJustificationType (juce::Justification::centred);
        c->label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        c->label.setColour (juce::Label::textColourId, kInk.withAlpha (0.85f));
        c->label.setMinimumHorizontalScale (0.6f);
        addChildComponent (c->label);
        c->value.setJustificationType (juce::Justification::centred);
        c->value.setFont (juce::FontOptions (10.5f));
        c->value.setColour (juce::Label::textColourId, kInkDim);
        c->value.setMinimumHorizontalScale (0.6f);
        addChildComponent (c->value);

        const auto choices = d.type == s8::PType::Choice ? s8::choicesOf (d) : juce::StringArray();
        if (d.type == s8::PType::Choice && choices.size() <= 5)
        {
            auto keys = std::make_unique<ChoiceKeys> (choices, juce::Colour (0xff3a3c43));
            c->kind = Kind::Keys;
            c->width = keys->preferredWidth();
            c->comp = std::move (keys);
        }
        else if (d.type == s8::PType::Choice)
        {
            auto box = std::make_unique<juce::ComboBox>();
            box->addItemList (choices, 1);
            c->kind = Kind::Lcd;
            c->width = juce::String (d.id).endsWith ("_dst") || juce::String (d.id).endsWith ("_src") ? 112 : 96;
            if (juce::String (d.id) == "key_mode" || juce::String (d.id) == "condition") c->width = 150;
            c->comp = std::move (box);
        }
        else if (d.type == s8::PType::Bool)
        {
            auto b = std::make_unique<juce::ToggleButton>();
            b->setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff3a3c43));
            c->kind = Kind::Switch;
            c->width = 50;
            c->comp = std::move (b);
        }
        else
        {
            auto s = std::make_unique<juce::Slider> (juce::Slider::LinearVertical, juce::Slider::NoTextBox);
            s->setColour (juce::Slider::trackColourId, colour);
            s->setPopupDisplayEnabled (true, false, this);
            s->setDoubleClickReturnValue (true, (double) d.def);
            s->onValueChange = [ctl] { ctl->refreshValue(); };
            c->kind = Kind::Fader;
            c->width = 50;
            c->comp = std::move (s);
        }
        addChildComponent (*c->comp);
        out.push_back (std::move (c));
    }
}

// ------------------------------------------------------------------ panel
SaturnoPanel::SaturnoPanel (Saturno8Processor& p)
    : proc (p),
      grids { { p.steps[0], p.currentStep[0] }, { p.steps[1], p.currentStep[1] } },
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    build (s8::layerParamDefs(), layerControls);
    build (s8::layerFxParamDefs(), fxControls);
    build (s8::globalParamDefs(), globalControls);
    for (auto& c : globalControls) c->attach (proc.apvts, c->def->id);

    for (auto& gr : grids) addChildComponent (gr);
    grids[0].setColour (sectionColour ("Arpegiador"));
    grids[1].setColour (kLowerTint);

    tabs.addTab ("Capa superior", kPanelLight, 0);
    tabs.addTab ("Capa inferior", kPanelLight, 1);
    tabs.addTab ("Efectos sup.", kPanelLight, 2);
    tabs.addTab ("Efectos inf.", kPanelLight, 3);
    tabs.addTab ("Global", kPanelLight, 4);
    tabs.onChange = [this] (int index) { if (index != page) showPage (index); };
    addAndMakeVisible (tabs);

    chordButton.setTooltip ("Mantén pulsadas las notas del acorde y pulsa aquí");
    chordButton.onClick = [this] { proc.requestChordLearn (page == 1 ? 1 : 0); };
    addAndMakeVisible (chordButton);

    keyboard.setAvailableRange (24, 108);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour (0xffe8743b).withAlpha (0.7f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour (0xffe8743b).withAlpha (0.25f));
    addAndMakeVisible (keyboard);

    // Tamaño base: la fila más ancha de todas las páginas.
    width = 2 * kMargin + juce::jmax (rowWidth (kLayerRows, layerControls), rowWidth (kFxRows, fxControls), rowWidth (kGlobalRows, globalControls));
    height = kHeader + kTabsH + 10 + 3 * (kSecH + kRowGap) + kGridH + 30 + kFooter + kKeysH + kMargin;
    setSize (width, height);
    showPage (0);
    startTimerHz (10);
}

SaturnoPanel::~SaturnoPanel()
{
    setLookAndFeel (nullptr);
}

int SaturnoPanel::rowWidth (const Rows& rows, const std::vector<std::unique_ptr<Control>>& controls) const
{
    int best = 0;
    for (auto& row : rows)
    {
        int w = 0;
        for (auto* sName : row)
        {
            int sec = 2 * kPad;
            for (auto& c : controls) if (c->section == utf8 (sName)) sec += c->width + 4;
            w += sec + kSecGap;
        }
        best = juce::jmax (best, w - kSecGap);
    }
    return best;
}

void SaturnoPanel::showPage (int newPage)
{
    page = juce::jlimit (0, 4, newPage);
    if (tabs.getCurrentTabIndex() != page) tabs.setCurrentTabIndex (page, false);
    const bool isLayer = page < 2, isFx = page == 2 || page == 3;
    const int layer = isFx ? page - 2 : page;
    auto show = [&] (std::vector<std::unique_ptr<Control>>& list, bool visible, bool attachToLayer)
    {
        for (auto& c : list)
        {
            if (visible && attachToLayer) c->attach (proc.apvts, Saturno8Processor::pid (Saturno8Processor::kPrefix[layer], c->def->id));
            if (auto* s = dynamic_cast<juce::Slider*> (c->comp.get()))
                s->setColour (juce::Slider::trackColourId, attachToLayer && layer == 1 ? kLowerTint : sectionColour (c->section));
            c->comp->setVisible (visible);
            c->label.setVisible (visible);
            c->value.setVisible (visible && c->kind == Kind::Fader);
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

void SaturnoPanel::paint (juce::Graphics& g)
{
    // Fondo grafito con un leve degradado y textura de líneas finas
    juce::ColourGradient bg (juce::Colour (0xff24252a), 0, 0, juce::Colour (0xff151619), 0, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();
    g.setColour (juce::Colours::white.withAlpha (0.012f));
    for (int y = 0; y < getHeight(); y += 3) g.drawHorizontalLine (y, 0, (float) getWidth());

    // Cabecera: logotipo con anillo y pantalla
    auto head = juce::Rectangle<float> (0, 0, (float) getWidth(), (float) kHeader);
    juce::ColourGradient metal (juce::Colour (0xff3b3d43), 0, 0, juce::Colour (0xff1e1f23), 0, head.getBottom(), false);
    g.setGradientFill (metal);
    g.fillRect (head);
    g.setColour (juce::Colours::black);
    g.drawHorizontalLine (kHeader - 1, 0, (float) getWidth());
    // franja de colores bajo la cabecera
    const juce::Colour stripe[] { juce::Colour (0xffe8743b), juce::Colour (0xffd8423a), juce::Colour (0xffe6b43c),
                                  juce::Colour (0xff7fbf5a), juce::Colour (0xff4fa3d9), juce::Colour (0xffb46ad6) };
    const float sw = (float) getWidth() / 6.0f;
    for (int i = 0; i < 6; ++i) { g.setColour (stripe[i]); g.fillRect ((float) i * sw, (float) kHeader - 4, sw, 3.0f); }

    // Planeta con anillo
    const juce::Point<float> c ((float) kMargin + 26, (float) kHeader * 0.5f - 2);
    g.setColour (kInk);
    g.fillEllipse (c.x - 14, c.y - 14, 28, 28);
    juce::Path ring;
    ring.addCentredArc (c.x, c.y, 26, 8, -0.35f, 0, juce::MathConstants<float>::twoPi, true);
    g.setColour (juce::Colour (0xffe8743b));
    g.strokePath (ring, juce::PathStrokeType (3.0f));
    g.setColour (kInk);
    g.fillEllipse (c.x - 14, c.y - 14, 28, 14);   // la mitad de arriba tapa el anillo por detrás

    g.setFont (juce::FontOptions (34.0f, juce::Font::bold));
    juce::GlyphArrangement ga;
    const juce::String logo ("SATURNO");
    float x = c.x + 40;
    for (auto ch : logo)
    {
        g.setColour (kInk);
        g.drawSingleLineText (juce::String::charToString (ch), (int) x, (int) c.y + 12);
        x += juce::GlyphArrangement::getStringWidth (juce::FontOptions (34.0f, juce::Font::bold), juce::String::charToString (ch)) + 4;
    }
    g.setColour (juce::Colour (0xffe8743b));
    g.drawSingleLineText ("8", (int) x + 8, (int) c.y + 12);
    g.setColour (kInkDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawSingleLineText (utf8 ("SINTETIZADOR POLIFÓNICO DE DOS CAPAS · 16 + 16 VOCES"), (int) c.x + 42, (int) c.y + 28);

    // Pantalla
    auto lcd = juce::Rectangle<float> ((float) getWidth() - kMargin - 340, 12, 340, (float) kHeader - 28);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (lcd, 4.0f);
    g.setColour (kLcdBg);
    g.fillRoundedRectangle (lcd.reduced (3), 3.0f);
    g.setColour (kLcdInk);
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::bold));
    g.drawText (lcdLine1, lcd.reduced (12, 6).removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (kLcdInk.withAlpha (0.75f));
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    g.drawText (lcdLine2, lcd.reduced (12, 6).removeFromBottom (16), juce::Justification::centredLeft);

    // Secciones
    for (auto& [name, box] : sectionBoxes)
    {
        auto r = box.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0, 2), 5.0f);
        g.setColour (kPanelLight);
        g.fillRoundedRectangle (r, 5.0f);
        auto strip = r.removeFromTop ((float) kSecHeader);
        const auto col = (page == 1 || page == 3) && name != "Secuencia" ? kLowerTint : sectionColour (name);
        g.setColour (col);
        g.fillRoundedRectangle (strip, 5.0f);
        g.fillRect (strip.withTrimmedTop (8));
        g.setColour (juce::Colour (0xff141414));
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (upper (name), strip.reduced (8, 0), juce::Justification::centredLeft);
        if (name == "Secuencia")
        {
            g.setFont (juce::FontOptions (10.5f));
            g.drawText (utf8 ("Modo Secuenciador · se transporta desde la tecla más grave · LED: paso o silencio · arrastra la nota · L: ligadura · A: acento"),
                        strip.reduced (8, 0).withTrimmedLeft (90), juce::Justification::centredLeft);
        }
    }
}

void SaturnoPanel::layoutRows (const Rows& rows, std::vector<std::unique_ptr<Control>>& controls, int top)
{
    int y = top;
    for (auto& row : rows)
    {
        int x = kMargin;
        for (auto* sName : row)
        {
            const juce::String section = utf8 (sName);
            int cx = x + kPad;
            for (auto& c : controls)
            {
                if (c->section != section) continue;
                const int w = c->width;
                const int cy = y + kSecHeader + 2;
                c->label.setBounds (cx - 2, cy, w + 4, kLabelH);
                auto area = juce::Rectangle<int> (cx, cy + kLabelH, w, kCtrlH);
                switch (c->kind)
                {
                    case Kind::Fader:  c->comp->setBounds (area.withSizeKeepingCentre (40, kCtrlH)); break;
                    case Kind::Switch: c->comp->setBounds (area.withSizeKeepingCentre (w - 6, 24)); break;
                    case Kind::Keys:   c->comp->setBounds (area.reduced (0, 2)); break;
                    case Kind::Lcd:    c->comp->setBounds (area.withSizeKeepingCentre (w - 4, 26)); break;
                }
                c->value.setBounds (cx - 4, cy + kLabelH + kCtrlH, w + 8, kValueH);
                cx += w + 4;
            }
            sectionBoxes.push_back ({ section, { x, y, cx - x - 4 + kPad, kSecH } });
            x = cx - 4 + kPad + kSecGap;
        }
        y += kSecH + kRowGap;
    }
}

void SaturnoPanel::resized()
{
    sectionBoxes.clear();
    tabs.setBounds (kMargin, kHeader + 6, 560, kTabsH);
    const int top = kHeader + kTabsH + 14;
    if (page < 2) layoutRows (kLayerRows, layerControls, top);
    else if (page < 4) layoutRows (kFxRows, fxControls, top);
    else layoutRows (kGlobalRows, globalControls, top);

    const int gridTop = top + 3 * (kSecH + kRowGap);
    if (page < 2)
    {
        sectionBoxes.push_back ({ "Secuencia", { kMargin, gridTop, getWidth() - 2 * kMargin, kGridH + 26 } });
        grids[page].setBounds (kMargin + 6, gridTop + kSecHeader + 3, getWidth() - 2 * kMargin - 12, kGridH);
    }
    const int y = gridTop + kGridH + 34;
    chordButton.setBounds (kMargin, y, 150, 26);
    keyboard.setBounds (kMargin, y + kFooter, getWidth() - 2 * kMargin, kKeysH);
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 50.0f);   // 50 teclas blancas: Do 1 a Do 8
}

void SaturnoPanel::timerCallback()
{
    static const char* pageNames[] { "CAPA SUPERIOR", "CAPA INFERIOR", "EFECTOS SUPERIOR", "EFECTOS INFERIOR", "GLOBAL" };
    juce::String l1 (pageNames[page]);
    if (page < 2) l1 << "  ACORDE " << proc.chordDescription (page).toUpperCase();
    juce::String l2;
    l2 << "VOCES " << juce::String (proc.activeVoices[0].load()).paddedLeft ('0', 2) << "+"
       << juce::String (proc.activeVoices[1].load()).paddedLeft ('0', 2);
    if (auto* t = proc.apvts.getParameter ("tempo")) l2 << "  " << t->getCurrentValueAsText();
    const float red = proc.limiterReduction.load();
    if (red < 0.99f) l2 << "  LIM " << juce::String (juce::Decibels::gainToDecibels (red), 1) << "dB";
    if (l1 != lcdLine1 || l2 != lcdLine2) { lcdLine1 = l1; lcdLine2 = l2; repaint (getWidth() - kMargin - 340, 12, 340, kHeader - 28); }

    if (page < 2)
        if (auto* len = proc.apvts.getRawParameterValue (Saturno8Processor::pid (Saturno8Processor::kPrefix[page], "seq_len")))
            grids[page].setLength ((int) len->load());
}

// ------------------------------------------------------------------ editor escalable
Saturno8Editor::Saturno8Editor (Saturno8Processor& p) : AudioProcessorEditor (&p), panel (p)
{
    addAndMakeVisible (panel);
    const int w = panel.baseWidth(), h = panel.baseHeight();
    setResizable (true, true);
    setResizeLimits (w / 2, h / 2, w * 2, h * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) w / (double) h);
    setSize (juce::roundToInt (w * 0.8), juce::roundToInt (h * 0.8));
}

void Saturno8Editor::resized()
{
    const float scale = (float) getWidth() / (float) panel.baseWidth();
    panel.setTransform (juce::AffineTransform::scale (scale));
}
