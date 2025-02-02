/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AutoFreezeAudioProcessorEditor::AutoFreezeAudioProcessorEditor (AutoFreezeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 300);
    startTimerHz(30); // refresh at 30 Hz
}

AutoFreezeAudioProcessorEditor::~AutoFreezeAudioProcessorEditor()
{
}

//==============================================================================
void AutoFreezeAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f));
    g.drawFittedText ("RMS: " + juce::String(displayRms, 2), getLocalBounds(), juce::Justification::centred, 1);
}

void AutoFreezeAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}

void AutoFreezeAudioProcessorEditor::timerCallback()
{
    displayRms = audioProcessor.getRms();
    repaint();
}
