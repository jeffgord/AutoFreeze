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
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    // Add level text
    g.setColour(juce::Colours::wheat);
    g.setFont(juce::FontOptions (15.0f));
    g.drawText(juce::String(displayDbLevel, 2), levelTextRect, juce::Justification::centred, 1);
    
    // Draw meter level
    g.setColour(juce::Colours::thistle);
    g.fillRect(meterLevelRect);
    
    // Draw meter bounding box
    g.setColour(juce::Colours::wheat);
    g.drawRect(meterBoundsRect);
}

void AutoFreezeAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    
    // Position the meter
    int meterWidth = 50;
    int meterHeight = 200;
    int meterX = (getWidth() - meterWidth) / 2;
    int meterY = (getHeight() - meterHeight) / 2;
    meterBoundsRect.setBounds(meterX, meterY, meterWidth, meterHeight);
    
    // Position the level text
    int levelTextWidth = 100;
    int levelTextHeight = 20;
    int levelTextX = meterBoundsRect.getCentreX() - levelTextWidth / 2;
    int levelTextY = meterBoundsRect.getY() - levelTextHeight;
    levelTextRect.setBounds(levelTextX, levelTextY, levelTextWidth, levelTextHeight);
    
}

void AutoFreezeAudioProcessorEditor::timerCallback()
{
    displayDbLevel = juce::jlimit(minDisplayDbLevel, maxDisplayDbLevel, audioProcessor.getDbLevel());
    
    // Calculate meter level position
    int meterLevelWidth = meterBoundsRect.getWidth();
    int meterLevelHeight = juce::jmap(displayDbLevel, minDisplayDbLevel, maxDisplayDbLevel, 0.0f, meterBoundsRect.getHeight());
    int meterLevelX = meterBoundsRect.getX();
    int meterLevelY = meterBoundsRect.getBottom() - meterLevelHeight;
    meterLevelRect.setBounds(meterLevelX, meterLevelY, meterLevelWidth, meterLevelHeight);
    
    repaint();
}

