/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class AutoFreezeAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    AutoFreezeAudioProcessor();
    ~AutoFreezeAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    //==============================================================================
    
private:
    //==============================================================================
    enum class AutoFreezeState
    {
        BelowThreshold,
        Predelay,
        ReadingFreeze,
        Cooldown
    };
    
    // constants
    static constexpr int freezeBufferLength = 16384; // = 2^14
    static constexpr float freezeThresholdDb = -20.0f;
    static constexpr int numGrains = 4;
    static constexpr float predelaySeconds = 0.1f;
    static constexpr float cooldownSeconds = 1.0f;
    static constexpr float shortFadeSeconds = 0.05f;
    static constexpr float longFadeSeconds = 0.1f;
    
    // freeze buffer
    juce::AudioBuffer<float> freezeBuffer;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> freezeWindowingFunction;
    int freezeBufferIndex;
    
    // grains
    float grainTargetRms;
    juce::AudioBuffer<float> freezeMags;
    std::array<juce::AudioBuffer<float>, numGrains> grains;
    std::array<int, numGrains> grainIndices;
    
    // predelay
    int predelaySamples;
    int predelayCounter;
    
    // cooldown
    int cooldownSamples;
    int coolDownCounter;
    
    // short fade
    int shortFadeSamples;
    std::vector<float> shortFadeIn;
    std::vector<float> shortFadeOut;
    int shortFadeIndex;
    
    // long fade
    int longFadeSamples;
    std::vector<float> longFadeIn;
    std::vector<float> longFadeOut;
    int longFadeIndex;
        
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoFreezeAudioProcessor)
};
