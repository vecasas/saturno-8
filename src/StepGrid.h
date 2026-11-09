#pragma once
// Rejilla de 16 pasos del secuenciador de una capa.
// Clic en el LED = paso activo o silencio · arrastrar o rueda en la nota = semitonos (doble clic = 0)
// · L = ligadura con el paso anterior · A = acento.
#include <JuceHeader.h>
#include "seq/Player.h"

class StepGrid : public juce::Component, private juce::Timer
{
public:
    StepGrid (s8::StepData& d, std::atomic<int>& playing) : data (d), current (playing) { startTimerHz (20); }

    void setColour (juce::Colour c) { accent = c; repaint(); }
    void setLength (int n) { if (n != length) { length = n; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        const auto ink = juce::Colour (0xffe9e4da);
        for (int i = 0; i < s8::kSeqSteps; ++i)
        {
            const auto st = data.get (i);
            auto r = cell (i).toFloat().reduced (2);
            const bool inUse = i < length;
            g.setColour (juce::Colour (i == shownStep ? 0xff3a3d44 : 0xff2b2d33).withMultipliedAlpha (inUse ? 1.0f : 0.4f));
            g.fillRoundedRectangle (r, 4.0f);

            // Número de paso y LED
            auto top = r.removeFromTop (rowH);
            g.setColour (ink.withAlpha (0.5f));
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (juce::String (i + 1), top.removeFromLeft (18), juce::Justification::centred);
            const auto led = top.withSizeKeepingCentre (14, 14);
            g.setColour (st.on ? accent : juce::Colour (0xff45474e));
            g.fillEllipse (led);

            // Nota (semitonos)
            auto mid = r.removeFromTop (rowH + 6);
            g.setColour (st.on ? ink : ink.withAlpha (0.35f));
            g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
            g.drawText ((st.note > 0 ? "+" : "") + juce::String (st.note), mid, juce::Justification::centred);

            // Ligadura y acento
            auto bottom = r.removeFromTop (rowH);
            auto tieBox = bottom.removeFromLeft (bottom.getWidth() / 2).reduced (3, 2);
            auto accBox = bottom.reduced (3, 2);
            drawFlag (g, tieBox, "L", st.tie, accent);
            drawFlag (g, accBox, "A", st.accent, juce::Colour (0xffd94a3d));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = stepAt (e.position);
        if (i < 0) return;
        auto st = data.get (i);
        const auto r = cell (i);
        const int y = e.getPosition().y - r.getY();
        dragStep = -1;
        if (y < rowH) st.on = ! st.on;
        else if (y < 2 * rowH + 6) { dragStep = i; dragStartNote = st.note; return; }
        else if (e.getPosition().x < r.getCentreX()) st.tie = ! st.tie;
        else st.accent = ! st.accent;
        data.set (i, st);
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragStep < 0) return;
        auto st = data.get (dragStep);
        st.note = juce::jlimit (-24, 24, dragStartNote - e.getDistanceFromDragStartY() / 6);
        data.set (dragStep, st);
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int i = stepAt (e.position);
        if (i < 0 || e.getPosition().y - cell (i).getY() < rowH || e.getPosition().y - cell (i).getY() >= 2 * rowH + 6) return;
        auto st = data.get (i); st.note = 0; data.set (i, st); repaint();
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int i = stepAt (e.position);
        if (i < 0) return;
        auto st = data.get (i);
        st.note = juce::jlimit (-24, 24, st.note + (w.deltaY > 0 ? 1 : -1));
        data.set (i, st);
        repaint();
    }

private:
    static constexpr int rowH = 22;

    static void drawFlag (juce::Graphics& g, juce::Rectangle<float> r, const char* text, bool on, juce::Colour c)
    {
        g.setColour (on ? c : juce::Colour (0xff3a3c42));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? juce::Colours::black : juce::Colour (0xffe9e4da).withAlpha (0.6f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (text, r, juce::Justification::centred);
    }

    juce::Rectangle<int> cell (int i) const
    {
        const float w = (float) getWidth() / (float) s8::kSeqSteps;
        return { juce::roundToInt (w * (float) i), 0, juce::roundToInt (w), getHeight() };
    }
    int stepAt (juce::Point<float> p) const
    {
        const int i = (int) (p.x / ((float) getWidth() / (float) s8::kSeqSteps));
        return i >= 0 && i < s8::kSeqSteps ? i : -1;
    }
    void timerCallback() override
    {
        const int now = current.load();
        if (now != shownStep) { shownStep = now; repaint(); }
    }

    s8::StepData& data;
    std::atomic<int>& current;
    juce::Colour accent { 0xffe0662f };
    int length = 16, shownStep = -1, dragStep = -1, dragStartNote = 0;
};
