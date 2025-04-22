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
    
    switch (currentState) {
        case AutoFreezeState::BelowThreshold:
            processBelowThreshold(buffer);
            break;
        case AutoFreezeState::Predelay:
            processPredelay(buffer);
            break;
        case AutoFreezeState::ReadingFreeze:
            processReadingFreeze(buffer);
            break;
        case AutoFreezeState::Cooldown:
            processCooldown(buffer);
            break;
    }
    
    float channelTotalRms = 0.0f;
    
    for (int channel = 0; channel < buffer.getNumChannels(); channel++) {
        channelTotalRms += buffer.getRMSLevel(channel, 0, buffer.getNumSamples());
    }
    
    float averageRms = channelTotalRms / buffer.getNumChannels();
    dbLevel = juce::Decibels::gainToDecibels(averageRms);
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
    
    for (int channel = 0; channel < numChannels; channel++)
    {
        std::vector<float> fftData(freezeBufferSamples * 2);
        std::fill(fftData.begin(), fftData.end(), 0.0f);
        
        const float* channelFreezeData = buffer.getReadPointer(0);
        for (int sample = 0; sample < freezeBufferSamples; sample++)
            fftData[sample] = channelFreezeData[sample];
        
        freezeFft.performRealOnlyForwardTransform(fftData.data(), true);
        
        float* channelMagData = mags.getWritePointer(channel);
        for (int sample = 0; sample < freezeBufferSamples; sample++)
        {
            const float real = fftData[sample * 2];
            const float im = fftData[sample * 2 + 1];
            channelMagData[sample] = std::sqrt(real * real + im * im);
        }
    }
    
    return mags;
}

void AutoFreezeAudioProcessor::readIntoGrain(int grainNum)
{
    if (grainNum < 0 || grainNum >= numGrains)
    {
        throw std::runtime_error("Cannot read into non-existant grain");
    }
    
    std::vector<float> randomPhases(freezeBufferSamples);
    juce::Random random;
    
    for (int sample = 0; sample < freezeBufferSamples; sample++)
    {
        randomPhases[sample] = random.nextFloat() * juce::MathConstants<float>::twoPi;
    }
    
    for (int channel = 0; channel < freezeBuffer.getNumChannels(); channel++)
    {
        int ifftSize = 2 * freezeBufferSamples;
        std::vector<float> ifftData(2 * freezeBufferSamples);
        const float* magData = freezeMags.getReadPointer(channel);
        
        for (int sample = 0; sample < freezeBufferSamples; sample++) {
            float real = magData[sample] * std::cos(randomPhases[sample]);
            float imag = magData[sample] * std::sin(randomPhases[sample]);
            ifftData[sample * 2] = real;
            ifftData[sample * 2 + 1] = imag;
        }
        
        freezeFft.performRealOnlyInverseTransform(ifftData.data());
        
        float* grainChannelData = grains[grainNum].getWritePointer(channel);
        
        for (int sample = 0; sample < grains[grainNum].getNumSamples(); sample++)
        {
            grainChannelData[sample] = ifftData[sample];
        }
    }
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
                
                for (int i = 0; i < numGrains; i ++) {
                    readIntoGrain(i);
                    grainIndices[i] = round(freezeBufferSamples / numGrains * i);
                }
            }
            
            break;
        case AutoFreezeState::Cooldown:
            if (coolDownCounter >= cooldownSamples) {
                currentState = AutoFreezeState::BelowThreshold;
            }
            
            break;
    }
}

juce::AudioBuffer<float> AutoFreezeAudioProcessor::readFreeze(int numChannels, int blockSize)
{
    juce::AudioBuffer<float> buffer(numChannels, blockSize);
    
    
    for (int grainNum = 0; grainNum < numGrains; grainNum++)
    {
        if (grainIndices[grainNum] == freezeBufferSamples) {
            readIntoGrain(grainNum);
            grainIndices[grainNum] = 0;
        }
    }
        
    for (int channel = 0; channel < buffer.getNumChannels(); channel++)
    {
        float* channelData = buffer.getWritePointer(channel);
        
        for (int sample = 0; sample < buffer.getNumSamples(); sample++) {
            float windowSum = 0.0f;
            
            for (int grainNum = 0; grainNum < numGrains; grainNum++)
            {
                const float* grainData = grains[grainNum].getReadPointer(channel);
                int* pGrainIndex = &grainIndices[grainNum];
                channelData[sample] += grainData[*pGrainIndex];
                windowSum += freezeWindow[*pGrainIndex];
                (*pGrainIndex)++;
            }
            
            if (windowSum != 0.0f)
            {
                channelData[sample] /= windowSum;
            }
        }
    }
    
    return buffer;
}

void AutoFreezeAudioProcessor::processBelowThreshold(juce::AudioBuffer<float>& buffer)
{
    juce::AudioBuffer<float> freeze = readFreeze(buffer.getNumChannels(), buffer.getNumSamples());
    
    for (int channel = 0; channel < buffer.getNumChannels(); channel++)
    {
        const float* freezeChannelData = freeze.getReadPointer(channel);
        float* bufferChannelData = buffer.getWritePointer(channel);
        
        for (int sample = 0; sample < freeze.getNumSamples(); sample++)
        {
            bufferChannelData[sample] = freezeChannelData[sample];
        }
    }
}

void AutoFreezeAudioProcessor::processPredelay(juce::AudioBuffer<float>& buffer)
{
    juce::AudioBuffer<float> freeze = readFreeze(buffer.getNumChannels(), buffer.getNumSamples());
    
    for (int channel = 0; channel < buffer.getNumChannels(); channel++)
    {
        float* bufferChannelData = buffer.getWritePointer(channel);
        const float* freezeChannelData = freeze.getReadPointer(channel);
        
        for (int sample = 0; sample < buffer.getNumSamples(); sample++)
        {
            float fade_in_factor = 1;
            float fade_out_factor = 0;
            
            if (shortFadeIndex < shortFadeSamples)
            {
                fade_in_factor = shortFadeIn[shortFadeIndex];
                fade_out_factor = shortFadeOut[shortFadeIndex];
            }
                        
            float faded_in_dry = bufferChannelData[sample] * fade_in_factor;
            float faded_out_wet = freezeChannelData[sample] * fade_out_factor;
            
            bufferChannelData[sample] = faded_in_dry + faded_out_wet;
            
        }
    }
    
    predelayCounter += buffer.getNumSamples();
    shortFadeIndex += buffer.getNumSamples();
}

void AutoFreezeAudioProcessor::processReadingFreeze(juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); channel++) {
        const float* bufferChannelData = buffer.getReadPointer(channel);
        float* freezeChannelData = freezeBuffer.getWritePointer(channel);
        
        for (int sample = 0; sample < buffer.getNumSamples(); sample++) {
            float windowedSample = bufferChannelData[sample] * freezeWindow[freezeBufferIndex];
            freezeChannelData[freezeBufferIndex] = windowedSample;
            freezeBufferIndex++;
        }
    }
}

void AutoFreezeAudioProcessor::processCooldown(juce::AudioBuffer<float>& buffer)
{
    juce::AudioBuffer<float> freeze = readFreeze(buffer.getNumChannels(), buffer.getNumSamples());
    
    for (int channel = 0; channel < buffer.getNumChannels(); channel++)
    {
        float* bufferChannelData = buffer.getWritePointer(channel);
        const float* freezeChannelData = freeze.getReadPointer(channel);
        
        for (int sample = 0; sample < buffer.getNumSamples(); sample++)
        {
            float fade_in_factor = 1;
            float fade_out_factor = 0;
            
            if (longFadeIndex < shortFadeSamples)
            {
                fade_in_factor = longFadeIn[longFadeIndex];
                fade_out_factor = longFadeOut[longFadeIndex];
            }
                        
            float faded_in_wet = freezeChannelData[sample] * fade_in_factor;
            float faded_out_dry = bufferChannelData[sample] * fade_out_factor;
            
            bufferChannelData[sample] = faded_in_wet + faded_out_dry;
            
        }
    }
    
    coolDownCounter += buffer.getNumSamples();
    longFadeIndex += buffer.getNumSamples();
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
