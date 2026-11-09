#pragma once
#include "PluginProcessor.h"

class Saturno8Editor : public juce::AudioProcessorEditor
{
public:
    explicit Saturno8Editor (Saturno8Processor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Saturno8Processor& proc;
    juce::Slider masterKnob;
    juce::AudioProcessorValueTreeState::SliderAttachment masterAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Saturno8Editor)
};
