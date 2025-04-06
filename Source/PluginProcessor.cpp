/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AutoFreezeAudioProcessor::AutoFreezeAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

AutoFreezeAudioProcessor::~AutoFreezeAudioProcessor()
{
}

//==============================================================================
const juce::String AutoFreezeAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AutoFreezeAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AutoFreezeAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AutoFreezeAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AutoFreezeAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AutoFreezeAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AutoFreezeAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AutoFreezeAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String AutoFreezeAudioProcessor::getProgramName (int index)
{
    return {};
}

void AutoFreezeAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
int roundToMultiple (int unRoundedNumber, int multiple)
{
    return static_cast<int>(std::round(static_cast<float>(unRoundedNumber) / multiple)) * multiple;
}

void generateFade (std::vector<float>& fade, bool fadeIn, int size)
{
    fade.resize(size);
    
    for (int i = 0; i < size; i++)
    {
        float x = static_cast<float>(i) / size * (M_PI / 2);
        
        if (fadeIn) fade[i] = std::sin(x);
        else fade[i] = std::cos(x);
    }
}

void AutoFreezeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int channels = getTotalNumInputChannels();
    
    // freeze buffer
    freezeBuffer.setSize(channels, freezeBufferSamples);
    freezeBuffer.clear();
    juce::dsp::WindowingFunction<float> freezeWindowingFunction =
        juce::dsp::WindowingFunction<float>(freezeBufferSamples, juce::dsp::WindowingFunction<float>::hann);
    freezeWindow.resize(freezeBufferSamples);
    std::fill(freezeWindow.begin(), freezeWindow.end(), 1.0f);
    freezeWindowingFunction.multiplyWithWindowingTable(freezeWindow.data(), freezeBufferSamples);
    freezeBufferIndex = 0;
    
    // grains
    grainTargetsRms.resize(channels);
    std::fill(grainTargetsRms.begin(), grainTargetsRms.end(), 0.0f);
    freezeMags.setSize(channels, freezeBufferSamples);
    
    for (int i = 0; i < numGrains; i++)
    {
        auto& grain = grains[i];
        grain.setSize(channels, freezeBufferSamples);
        grainIndices[i] = freezeBufferSamples / 4 * i;
    }
    
    // predelay
    predelaySamples = roundToMultiple(predelaySeconds * sampleRate, samplesPerBlock);
    predelayCounter = 0;
    
    // cooldown
    cooldownSamples = roundToMultiple(cooldownSeconds * sampleRate, samplesPerBlock);
    coolDownCounter = 0;
    
    // short fade
    shortFadeSamples = roundToMultiple(shortFadeSeconds * sampleRate, samplesPerBlock);
    generateFade(shortFadeIn, true, shortFadeSamples);
    generateFade(shortFadeOut, false, shortFadeSamples);
    shortFadeIndex = 0;
    
    // long fade
    longFadeSamples = roundToMultiple(longFadeSeconds * sampleRate, samplesPerBlock);
    generateFade(longFadeIn, true, longFadeSamples);
    generateFade(longFadeOut, false, longFadeSamples);
    longFadeIndex = 0;
}

void AutoFreezeAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    freezeBuffer.setSize(0, 0);
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AutoFreezeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void AutoFreezeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    updateState(buffer);
}

std::vector<float> getChannelsRms(const juce::AudioBuffer<float>& buffer) {
    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    std::vector<float> channelsRms = std::vector<float>(numChannels);
    
    for (int channel = 0; channel < numChannels; channel++) {
        channelsRms[channel] = buffer.getRMSLevel(channel, 0, numSamples);
    }
    
    return channelsRms;
}

float getChannelAveragedRms(const juce::AudioBuffer<float>& buffer)
{
    float rmsSum = 0.0f;
    std::vector<float> channelsRms = getChannelsRms(buffer);
    
    for (float channelRms : channelsRms)
    {
        rmsSum += channelRms;
    }
    
    return rmsSum / buffer.getNumChannels();
}

juce::AudioBuffer<float> AutoFreezeAudioProcessor::getMagnitudes(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumSamples() != freezeBufferSamples) {
        throw std::runtime_error("Freeze buffer has unexpected length");
    }
    
    int numChannels = buffer.getNumChannels();
    juce::AudioBuffer<float> mags(numChannels, freezeBufferSamples);
    juce::dsp::FFT fft(std::log2(freezeBufferSamples));
    
    for (int channel = 0; channel < buffer.getNumChannels(); channel++)
    {
        std::vector<float> fftData(freezeBufferSamples * 2);
        std::fill(fftData.begin(), fftData.end(), 0.0f);
        
        const float* channelFreezeData = buffer.getReadPointer(0);
        for (int sample = 0; sample < freezeBufferSamples; sample++)
            fftData[sample] = channelFreezeData[sample];
        
        fft.performRealOnlyForwardTransform(fftData.data(), true);
        
        float* channelMagData = mags.getWritePointer(0);
        for (int sample = 0; sample < freezeBufferSamples; sample++)
        {
            const float real = fftData[sample * 2];
            const float im = fftData[sample * 2 + 1];
            channelMagData[sample] = std::sqrt(real * real + im * im);
        }
    }
    
    return mags;
}

void AutoFreezeAudioProcessor::updateState(juce::AudioBuffer<float>& buffer)
{
    switch(currentState)
    {
        case AutoFreezeState::BelowThreshold: {
            float rms = getChannelAveragedRms(buffer);
            float rmsDb = juce::Decibels::gainToDecibels(rms);
            
            if (rmsDb > freezeThresholdDb) {
                currentState = AutoFreezeState::Predelay;
                predelayCounter = 0;
                shortFadeIndex = 0;
            }
            
            break;
        }
        case AutoFreezeState::Predelay: {
            if (predelayCounter >= predelaySamples) {
                currentState = AutoFreezeState::ReadingFreeze;
                freezeBufferIndex = 0;
            }
            
            break;
        }
        case AutoFreezeState::ReadingFreeze:
            if (freezeBufferIndex >= freezeBufferSamples) {
                currentState = AutoFreezeState::Cooldown;
                coolDownCounter = 0;
                longFadeIndex = 0;
                grainTargetsRms = getChannelsRms(buffer);
                
                freezeMags = getMagnitudes(freezeBuffer);
            }
            
            break;
        case AutoFreezeState::Cooldown:
            break;
    }
}

//==============================================================================
bool AutoFreezeAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AutoFreezeAudioProcessor::createEditor()
{
    return new AutoFreezeAudioProcessorEditor (*this);
}

//==============================================================================
void AutoFreezeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void AutoFreezeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoFreezeAudioProcessor();
}
