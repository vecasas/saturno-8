#pragma once
// Columna de teclas con LED para elegir una opción de un parámetro de lista.
#include "Theme.h"

namespace s8ui
{
class ChoiceKeys : public juce::Component
{
public:
    explicit ChoiceKeys (const juce::StringArray& options, juce::Colour keyColour)
    {
        for (int i = 0; i < options.size(); ++i)
        {
            auto* b = keys.add (new juce::ToggleButton (options[i]));
            b->setColour (juce::ToggleButton::tickColourId, keyColour);
            b->setClickingTogglesState (false);
            b->onClick = [this, i] { if (attachment != nullptr) attachment->setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (b);
        }
    }

    void attach (juce::RangedAudioParameter& param)
    {
        attachment = std::make_unique<juce::ParameterAttachment> (param, [this] (float v) { show (juce::roundToInt (v)); });
        attachment->sendInitialUpdate();
    }

    void setKeyColour (juce::Colour c)
    {
        for (auto* b : keys) b->setColour (juce::ToggleButton::tickColourId, c);
        repaint();
    }

    // Ancho que necesita la etiqueta más larga.
    int preferredWidth() const
    {
        int w = 0;
        for (auto* b : keys)
            w = juce::jmax (w, juce::GlyphArrangement::getStringWidthInt (juce::FontOptions (11.0f, juce::Font::bold), b->getButtonText()));
        return juce::jmax (58, w + 26);
    }

    void resized() override
    {
        const int n = keys.size();
        const int h = juce::jmin (22, getHeight() / juce::jmax (1, n));
        const int top = (getHeight() - h * n) / 2;
        for (int i = 0; i < n; ++i) keys[i]->setBounds (0, top + i * h, getWidth(), h);
    }

private:
    void show (int index)
    {
        for (int i = 0; i < keys.size(); ++i) keys[i]->setToggleState (i == index, juce::dontSendNotification);
    }

    juce::OwnedArray<juce::ToggleButton> keys;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};
} // namespace s8ui
