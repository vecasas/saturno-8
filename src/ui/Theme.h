#pragma once
// Aspecto de Saturno 8: panel grafito, franjas de color por sección, faders con
// capuchón crema, teclas con LED y una pantalla ámbar. Todo dibujado por código.
#include <JuceHeader.h>

namespace s8ui
{
const juce::Colour kPanel (0xff1c1d21), kPanelLight (0xff2a2c32), kInk (0xffece6d9), kInkDim (0xff9d9a93),
                   kLed (0xffff5a2c), kLedOff (0xff4a2a22), kLcdBg (0xff1d1408), kLcdInk (0xffffb547),
                   kLowerTint (0xff57b8d8);

// Color de la franja de cada sección (paleta propia).
inline juce::Colour sectionColour (const juce::String& s)
{
    if (s.startsWith ("VCO") || s == "Mod. VCO" || s == "Mezcla") return juce::Colour (0xffe8743b);
    if (s == "VCF" || s == "VCA") return juce::Colour (0xffd8423a);
    if (s.startsWith ("ENV")) return juce::Colour (0xffe6b43c);
    if (s == "LFO" || s == "Matriz") return juce::Colour (0xff7fbf5a);
    if (s.startsWith ("Asign") || s == "Teclado" || s == "Bender" || s == "Portamento") return juce::Colour (0xff4fa3d9);
    if (s == "Arpegiador" || s == "Secuencia") return juce::Colour (0xffb46ad6);
    if (s == "Deriva") return juce::Colour (0xffc99a6b);
    return juce::Colour (0xff9fb3c8);   // efectos y máster
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, kPanel);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff25262b));
        setColour (juce::PopupMenu::textColourId, kInk);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff4a3a24));
        setColour (juce::PopupMenu::highlightedTextColourId, kLcdInk);
        setColour (juce::ComboBox::textColourId, kLcdInk);
        setColour (juce::ComboBox::backgroundColourId, kLcdBg);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::black);
        setColour (juce::ComboBox::arrowColourId, kLcdInk.withAlpha (0.7f));
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff2b2c31));
        setColour (juce::TooltipWindow::textColourId, kInk);
        setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xff2b2c31));
        setColour (juce::BubbleComponent::outlineColourId, juce::Colours::black);
        setColour (juce::TextButton::buttonColourId, kPanelLight);
        setColour (juce::TextButton::textColourOffId, kInk);
        setColour (juce::Label::textColourId, kInk);
    }

    // Fader vertical: ranura negra, marcas y capuchón crema con línea de color.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        const auto accent = s.findColour (juce::Slider::trackColourId);
        const float cx = (float) x + (float) w * 0.5f;
        const float top = (float) y + 8, bottom = (float) (y + h) - 8;
        g.setColour (kInkDim.withAlpha (0.35f));
        for (int i = 0; i <= 10; ++i)
        {
            const float yy = bottom - (bottom - top) * (float) i / 10.0f;
            const float len = i % 5 == 0 ? 7.0f : 4.0f;
            g.drawLine (cx - 9 - len, yy, cx - 9, yy, 1.0f);
            g.drawLine (cx + 9, yy, cx + 9 + len, yy, 1.0f);
        }
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (cx - 2.5f, top - 2, 5.0f, bottom - top + 4, 2.5f);
        // Recorrido iluminado desde abajo
        g.setColour (accent.withAlpha (0.55f));
        g.fillRoundedRectangle (cx - 1.5f, pos, 3.0f, bottom - pos, 1.5f);

        const juce::Rectangle<float> cap (cx - 11, pos - 8, 22, 16);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (cap.translated (0, 2), 3.0f);
        juce::ColourGradient grad (juce::Colour (0xfff4efe4), cap.getX(), cap.getY(), juce::Colour (0xffb9b2a4), cap.getX(), cap.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (accent.darker (0.2f));
        g.fillRect (cap.getX() + 3, pos - 1.0f, cap.getWidth() - 6, 2.0f);
    }

    // Mando giratorio (se usa poco: cantidad de la matriz).
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const auto accent = s.findColour (juce::Slider::trackColourId);
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4);
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (d, d);
        const float angle = a0 + pos * (a1 - a0);
        juce::Path arc;
        arc.addCentredArc (r.getCentreX(), r.getCentreY(), d * 0.5f, d * 0.5f, 0, a0, a1, true);
        g.setColour (juce::Colours::black);
        g.strokePath (arc, juce::PathStrokeType (3.0f));
        const float mid = a0 + 0.5f * (a1 - a0);
        juce::Path val;
        val.addCentredArc (r.getCentreX(), r.getCentreY(), d * 0.5f, d * 0.5f, 0, std::min (mid, angle), std::max (mid, angle), true);
        g.setColour (accent);
        g.strokePath (val, juce::PathStrokeType (3.0f));
        auto knob = r.reduced (d * 0.16f);
        juce::ColourGradient grad (juce::Colour (0xff3a3c43), knob.getX(), knob.getY(), juce::Colour (0xff15161a), knob.getX(), knob.getBottom(), false);
        g.setGradientFill (grad);
        g.fillEllipse (knob);
        g.setColour (kInk);
        const auto c = knob.getCentre();
        const float rr = knob.getWidth() * 0.5f;
        g.drawLine (c.x + std::sin (angle) * rr * 0.3f, c.y - std::cos (angle) * rr * 0.3f,
                    c.x + std::sin (angle) * rr * 0.9f, c.y - std::cos (angle) * rr * 0.9f, 2.0f);
    }

    // Botón con LED (interruptores y teclas de selección).
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool down) override
    {
        drawKey (g, b.getLocalBounds().toFloat(), b.getToggleState(), over, down, b.getButtonText(),
                 b.findColour (juce::ToggleButton::tickColourId));
    }

    static void drawKey (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool over, bool down,
                         const juce::String& text, juce::Colour keyColour)
    {
        r = r.reduced (1.5f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0, 1.5f), 3.0f);
        auto body = keyColour.withMultipliedBrightness (down ? 0.8f : (over ? 1.1f : 1.0f));
        juce::ColourGradient grad (body.brighter (0.15f), r.getX(), r.getY(), body.darker (0.25f), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 3.0f);
        // LED
        const float ledD = juce::jmin (7.0f, r.getHeight() - 6.0f);
        auto led = juce::Rectangle<float> (r.getX() + 6, r.getCentreY() - ledD * 0.5f, ledD, ledD);
        if (on)
        {
            g.setColour (kLed.withAlpha (0.35f));
            g.fillEllipse (led.expanded (2.5f));
        }
        g.setColour (on ? kLed : kLedOff);
        g.fillEllipse (led);
        if (text.isNotEmpty())
        {
            const bool dark = keyColour.getPerceivedBrightness() > 0.6f;
            g.setColour (dark ? juce::Colour (0xff1a1a1a) : kInk);
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawFittedText (text, r.withTrimmedLeft (ledD + 9).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        }
    }

    // Pantalla ámbar para los desplegables de muchas opciones.
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (kLcdBg);
        g.fillRoundedRectangle (r.reduced (2), 2.0f);
        juce::Path arrow;
        arrow.addTriangle ((float) w - 14, (float) h * 0.42f, (float) w - 6, (float) h * 0.42f, (float) w - 10, (float) h * 0.62f);
        g.setColour (kLcdInk.withAlpha (0.6f));
        g.fillPath (arrow);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.0f, juce::Font::bold); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (4, 1, box.getWidth() - 20, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    // Pestañas: teclas planas con LED.
    void drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool over, bool down) override
    {
        drawKey (g, b.getLocalBounds().toFloat(), b.isFrontTab(), over, down, b.getButtonText(), juce::Colour (0xff3a3c43));
    }
    void drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&) override {}
    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int, int) override {}
    int getTabButtonBestWidth (juce::TabBarButton& b, int) override
    {
        return juce::GlyphArrangement::getStringWidthInt (juce::FontOptions (11.0f, juce::Font::bold), b.getButtonText()) + 34;
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        drawKey (g, b.getLocalBounds().toFloat(), false, over, down, {}, juce::Colour (0xff3a3c43));
    }
};
} // namespace s8ui
