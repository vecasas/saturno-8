#include "PluginEditor.h"

Saturno8Editor::Saturno8Editor (Saturno8Processor& p)
    : AudioProcessorEditor (&p), proc (p),
      masterAttach (p.apvts, "master", masterKnob)
{
    masterKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    masterKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    addAndMakeVisible (masterKnob);
    setSize (720, 360);
}

void Saturno8Editor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff17181b));
    g.setColour (juce::Colour (0xffe9e4da));
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawText ("SATURNO 8", getLocalBounds().removeFromTop (80), juce::Justification::centred);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText ("Fase 1: esqueleto. Toca cualquier nota MIDI.", getLocalBounds().removeFromBottom (40),
                juce::Justification::centred);
}

void Saturno8Editor::resized()
{
    masterKnob.setBounds (getLocalBounds().withSizeKeepingCentre (110, 130));
}
