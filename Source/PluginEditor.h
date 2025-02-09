/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class AutoFreezeAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    AutoFreezeAudioProcessorEditor (AutoFreezeAudioProcessor&);
    ~AutoFreezeAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    static constexpr float minDisplayDbLevel = -60.0f;
    static constexpr float maxDisplayDbLevel = 0.0f;
    
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    AutoFreezeAudioProcessor& audioProcessor;
    juce::SmoothedValue<float> displayDbLevel;
    
    juce::Rectangle<float> levelTextRect;
    juce::Rectangle<float> meterBoundsRect;
    juce::Rectangle<float> meterLevelRect;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoFreezeAudioProcessorEditor)
};
