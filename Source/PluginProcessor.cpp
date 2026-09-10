#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
      parameters (*this, nullptr, "ASMR_STATE", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout AudioPluginAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto percent = juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f);

    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParameterIDs::warmth, 1 }, "Warmth", percent, 55.0f, "%"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParameterIDs::clarity, 1 }, "Clarity", percent, 50.0f, "%"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParameterIDs::tingles, 1 }, "Tingles", percent, 40.0f, "%"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParameterIDs::compression, 1 }, "Compression", percent, 38.0f, "%"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParameterIDs::threshold, 1 }, "Threshold",
                                                             juce::NormalisableRange<float> (-60.0f, -6.0f, 0.1f), -24.0f, " dB"));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParameterIDs::output, 1 }, "Output",
                                                             juce::NormalisableRange<float> (-18.0f, 24.0f, 0.1f), 0.0f, " dB"));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParameterIDs::bypass, 1 }, "Bypass", false));
    return layout;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const juce::dsp::ProcessSpec spec { sampleRate,
                                        static_cast<juce::uint32> (samplesPerBlock),
                                        static_cast<juce::uint32> (getTotalNumOutputChannels()) };

    highPass.prepare (spec);
    warmthFilter.prepare (spec);
    clarityFilter.prepare (spec);
    airFilter.prepare (spec);
    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.04);

    highPass.reset();
    warmthFilter.reset();
    clarityFilter.reset();
    airFilter.reset();
    compressorEnvelopeDb = 0.0f;
    peakCompressorEnvelopeDb = 0.0f;
    limiterLookAheadSamples = juce::jmax (1, juce::roundToInt (sampleRate * 0.0025));
    limiterDelayBuffer.setSize (getTotalNumOutputChannels(), limiterLookAheadSamples + 1,
                                false, true, true);
    limiterDelayBuffer.clear();
    limiterWritePosition = 0;
    limiterHoldSamples = 0;
    limiterEnvelopeDb = 0.0f;
    setLatencySamples (limiterLookAheadSamples);
    thresholdFrameSamples = juce::jmax (1, juce::roundToInt (sampleRate * 0.02));
    thresholdTargetFrames = juce::jmax (1, juce::roundToInt (10.0 * sampleRate / thresholdFrameSamples));
    outputGain.reset();

    *highPass.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, 65.0f, 0.707f);
    lastWarmth = lastClarity = lastTingles = lastCompression = -1.0f;
    lastThreshold = -100.0f;
    lastOutput = -100.0f;
    updateProcessingSettings();
}

void AudioPluginAudioProcessor::releaseResources()
{
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (input != juce::AudioChannelSet::mono() && input != juce::AudioChannelSet::stereo())
        return false;

    // In addition to regular mono and stereo operation, expose a mono-to-stereo
    // layout. Logic can then place the plugin as a stereo effect on a mono source.
    if (output == juce::AudioChannelSet::mono())
        return input == juce::AudioChannelSet::mono();

    if (output == juce::AudioChannelSet::stereo())
        return input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo();

    return false;
  #endif
}

void AudioPluginAudioProcessor::updateProcessingSettings()
{
    const auto warmth = parameters.getRawParameterValue (ParameterIDs::warmth)->load() / 100.0f;
    const auto clarity = parameters.getRawParameterValue (ParameterIDs::clarity)->load() / 100.0f;
    const auto tingles = parameters.getRawParameterValue (ParameterIDs::tingles)->load() / 100.0f;
    const auto compression = parameters.getRawParameterValue (ParameterIDs::compression)->load() / 100.0f;
    const auto threshold = parameters.getRawParameterValue (ParameterIDs::threshold)->load();
    const auto userOutput = parameters.getRawParameterValue (ParameterIDs::output)->load();
    const auto changed = [] (float a, float b) { return std::abs (a - b) > 0.00001f; };

    if (changed (warmth, lastWarmth))
    {
        *warmthFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            currentSampleRate, 230.0f, 0.72f, juce::Decibels::decibelsToGain (juce::jmap (warmth, -2.5f, 5.0f)));
        lastWarmth = warmth;
    }

    if (changed (clarity, lastClarity))
    {
        *clarityFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            currentSampleRate, 3200.0f, 0.75f, juce::Decibels::decibelsToGain (juce::jmap (clarity, -2.0f, 4.5f)));
        lastClarity = clarity;
    }

    if (changed (tingles, lastTingles))
    {
        const auto airFrequency = juce::jmin (9000.0f, static_cast<float> (currentSampleRate * 0.45));
        *airFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            currentSampleRate, airFrequency, 0.68f,
            juce::Decibels::decibelsToGain (juce::jmap (tingles, -2.0f, 6.0f)));
        lastTingles = tingles;
    }

    const auto compressionChanged = changed (compression, lastCompression);
    const auto thresholdChanged = changed (threshold, lastThreshold);
    if (compressionChanged || thresholdChanged)
    {
        compressorThresholdDb = threshold;
        compressorRatio = juce::jmap (compression, 3.0f, 6.0f);
        compressorAttackMs = juce::jmap (compression, 10.0f, 1.0f);
        compressorReleaseMs = juce::jmap (compression, 300.0f, 100.0f);
        lastCompression = compression;
        lastThreshold = threshold;
    }

    if (compressionChanged || changed (userOutput, lastOutput))
    {
        outputGain.setGainDecibels (userOutput + compression * 5.0f);
        lastOutput = userOutput;
    }
}

void AudioPluginAudioProcessor::startThresholdLearning() noexcept
{
    thresholdStopRequested.store (false);
    thresholdLearnRequested.store (true);
}

void AudioPluginAudioProcessor::stopThresholdLearning() noexcept
{
    thresholdStopRequested.store (true);
}

bool AudioPluginAudioProcessor::consumeLearnedThreshold (float& thresholdDb) noexcept
{
    const auto result = pendingLearnedThresholdDb.exchange (100.0f);
    if (result > 0.0f)
        return false;

    thresholdDb = result;
    return true;
}

void AudioPluginAudioProcessor::analyseThreshold (const juce::AudioBuffer<float>& buffer,
                                                   int numInputChannels)
{
    if (thresholdLearnRequested.exchange (false))
    {
        thresholdHistogram.fill (0);
        thresholdSamplesInFrame = 0;
        thresholdFramesAnalysed = 0;
        thresholdFramePeak = 0.0f;
        thresholdLearningProgress.store (0.0f);
        thresholdLearning.store (true);
    }

    if (! thresholdLearning.load())
        return;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float peak = 0.0f;
        for (int channel = 0; channel < numInputChannels; ++channel)
            peak = juce::jmax (peak, std::abs (buffer.getSample (channel, sample)));

        thresholdFramePeak = juce::jmax (thresholdFramePeak, peak);

        if (++thresholdSamplesInFrame >= thresholdFrameSamples)
        {
            const auto frameDb = juce::Decibels::gainToDecibels (thresholdFramePeak, -80.0f);
            const auto bin = juce::jlimit (0, thresholdHistogramBins - 1,
                                           juce::roundToInt (frameDb + 80.0f));
            ++thresholdHistogram[static_cast<size_t> (bin)];
            ++thresholdFramesAnalysed;
            thresholdSamplesInFrame = 0;
            thresholdFramePeak = 0.0f;
            thresholdLearningProgress.store (juce::jlimit (0.0f, 1.0f,
                static_cast<float> (thresholdFramesAnalysed) / static_cast<float> (thresholdTargetFrames)));
        }

        if (thresholdFramesAnalysed >= thresholdTargetFrames)
            break;
    }

    if (thresholdStopRequested.exchange (false) || thresholdFramesAnalysed >= thresholdTargetFrames)
        finishThresholdLearning();
}

void AudioPluginAudioProcessor::finishThresholdLearning() noexcept
{
    thresholdLearning.store (false);

    juce::uint32 totalFrames = 0;
    for (const auto count : thresholdHistogram)
        totalFrames += count;

    if (totalFrames < 10)
        return;

    const auto percentileBin = [this] (int firstBin, juce::uint32 target)
    {
        juce::uint32 cumulative = 0;
        for (int bin = firstBin; bin < thresholdHistogramBins; ++bin)
        {
            cumulative += thresholdHistogram[static_cast<size_t> (bin)];
            if (cumulative >= target)
                return bin;
        }
        return thresholdHistogramBins - 1;
    };

    // Estimate the upper active level first. A gate 35 dB below it removes
    // silence/room tone without allowing a few loud events to set the result.
    const auto upperBin = percentileBin (0, juce::jmax<juce::uint32> (1, totalFrames * 95 / 100));
    const auto firstActiveBin = juce::jmax (15, upperBin - 35); // never analyse below -65 dBFS

    juce::uint32 activeFrames = 0;
    for (int bin = firstActiveBin; bin < thresholdHistogramBins; ++bin)
        activeFrames += thresholdHistogram[static_cast<size_t> (bin)];

    if (activeFrames < 5)
        return;

    const auto typicalActiveBin = percentileBin (
        firstActiveBin, juce::jmax<juce::uint32> (1, activeFrames * 65 / 100));
    const auto typicalPeakDb = static_cast<float> (typicalActiveBin) - 80.0f;
    pendingLearnedThresholdDb.store (juce::jlimit (-60.0f, -6.0f, typicalPeakDb - 3.0f));
    thresholdLearningProgress.store (1.0f);
}

void AudioPluginAudioProcessor::updateMeter (std::atomic<float>& meter, float newValue) noexcept
{
    const auto previous = meter.load();
    meter.store (newValue > previous ? newValue : previous * 0.92f);
}

void AudioPluginAudioProcessor::processCompression (juce::AudioBuffer<float>& buffer)
{
    const auto attackCoefficient = std::exp (-1.0f / (0.001f * compressorAttackMs
                                                      * static_cast<float> (currentSampleRate)));
    const auto releaseCoefficient = std::exp (-1.0f / (0.001f * compressorReleaseMs
                                                       * static_cast<float> (currentSampleRate)));
    float peakReduction = 0.0f;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float detector = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            detector = juce::jmax (detector, std::abs (buffer.getSample (channel, sample)));

        const auto inputDb = juce::Decibels::gainToDecibels (detector, -100.0f);
        const auto overThreshold = juce::jmax (0.0f, inputDb - compressorThresholdDb);
        const auto targetReduction = overThreshold * (1.0f - 1.0f / compressorRatio);
        const auto coefficient = targetReduction > compressorEnvelopeDb
                                   ? attackCoefficient : releaseCoefficient;

        compressorEnvelopeDb = coefficient * compressorEnvelopeDb
                               + (1.0f - coefficient) * targetReduction;
        const auto gain = juce::Decibels::decibelsToGain (-compressorEnvelopeDb);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.getWritePointer (channel)[sample] *= gain;

        peakReduction = juce::jmax (peakReduction, compressorEnvelopeDb);
    }

    const auto previous = gainReductionDb.load();
    gainReductionDb.store (peakReduction > previous ? peakReduction : previous * 0.9f);
}

void AudioPluginAudioProcessor::processPeakCompression (juce::AudioBuffer<float>& buffer)
{
    constexpr auto thresholdOffsetDb = 12.0f;
    constexpr auto peakRatio = 12.0f;
    constexpr auto releaseMs = 80.0f;

    const auto peakThresholdDb = juce::jlimit (-48.0f, -6.0f,
                                                compressorThresholdDb + thresholdOffsetDb);
    const auto releaseCoefficient = std::exp (-1.0f / (0.001f * releaseMs
                                                       * static_cast<float> (currentSampleRate)));
    float peakReduction = 0.0f;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float detector = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            detector = juce::jmax (detector, std::abs (buffer.getSample (channel, sample)));

        const auto inputDb = juce::Decibels::gainToDecibels (detector, -100.0f);
        const auto overThreshold = juce::jmax (0.0f, inputDb - peakThresholdDb);
        const auto targetReduction = overThreshold * (1.0f - 1.0f / peakRatio);

        // Instant attack catches the first sample of short consonants and taps.
        if (targetReduction > peakCompressorEnvelopeDb)
            peakCompressorEnvelopeDb = targetReduction;
        else
            peakCompressorEnvelopeDb = targetReduction
                                     + releaseCoefficient * (peakCompressorEnvelopeDb - targetReduction);

        const auto gain = juce::Decibels::decibelsToGain (-peakCompressorEnvelopeDb);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.getWritePointer (channel)[sample] *= gain;

        peakReduction = juce::jmax (peakReduction, peakCompressorEnvelopeDb);
    }

    const auto previous = gainReductionDb.load();
    gainReductionDb.store (juce::jmax (previous, peakReduction));
}

void AudioPluginAudioProcessor::processPeakLimiter (juce::AudioBuffer<float>& buffer, bool bypassed)
{
    constexpr auto ceilingDb = -1.0f;
    const auto releaseCoefficient = std::exp (-1.0f / (0.001f * 80.0f
                                                       * static_cast<float> (currentSampleRate)));
    const auto delaySize = limiterDelayBuffer.getNumSamples();
    float peakReduction = 0.0f;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float detector = 0.0f;
        if (! bypassed)
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                detector = juce::jmax (detector, std::abs (buffer.getSample (channel, sample)));

        const auto requiredReduction = bypassed ? 0.0f
            : juce::jmax (0.0f, juce::Decibels::gainToDecibels (detector, -100.0f) - ceilingDb);

        if (requiredReduction > limiterEnvelopeDb)
        {
            limiterEnvelopeDb = requiredReduction;
            limiterHoldSamples = limiterLookAheadSamples;
        }
        else if (limiterHoldSamples > 0)
        {
            --limiterHoldSamples;
        }
        else
        {
            limiterEnvelopeDb = requiredReduction
                              + releaseCoefficient * (limiterEnvelopeDb - requiredReduction);
        }

        if (bypassed)
        {
            limiterEnvelopeDb = 0.0f;
            limiterHoldSamples = 0;
        }

        const auto limiterGain = juce::Decibels::decibelsToGain (-limiterEnvelopeDb);
        const auto readPosition = (limiterWritePosition + 1) % delaySize;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto input = buffer.getSample (channel, sample);
            limiterDelayBuffer.setSample (channel, limiterWritePosition, input);
            buffer.setSample (channel, sample,
                              limiterDelayBuffer.getSample (channel, readPosition) * limiterGain);
        }

        limiterWritePosition = readPosition;
        peakReduction = juce::jmax (peakReduction, limiterEnvelopeDb);
    }

    if (! bypassed)
    {
        const auto previous = gainReductionDb.load();
        gainReductionDb.store (juce::jmax (previous, peakReduction));
    }
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto numInputChannels = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();

    for (auto channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    float inputPeak = 0.0f;
    for (int channel = 0; channel < numInputChannels; ++channel)
        inputPeak = juce::jmax (inputPeak, buffer.getMagnitude (channel, 0, buffer.getNumSamples()));
    updateMeter (inputLevel, inputPeak);
    analyseThreshold (buffer, numInputChannels);

    // Hosts provide only channel 0 for a mono input. Seed the second output
    // channel before running the stereo-linked processing chain.
    if (numInputChannels == 1 && numOutputChannels == 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());

    const auto bypassed = parameters.getRawParameterValue (ParameterIDs::bypass)->load() >= 0.5f;
    if (! bypassed)
    {
        updateProcessingSettings();
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);
        highPass.process (context);
        warmthFilter.process (context);
        clarityFilter.process (context);
        airFilter.process (context);
        processCompression (buffer);
        processPeakCompression (buffer);
        outputGain.process (context);
    }
    else
    {
        compressorEnvelopeDb = 0.0f;
        peakCompressorEnvelopeDb = 0.0f;
        gainReductionDb.store (gainReductionDb.load() * 0.82f);
    }

    // The compressor intentionally retains its attack transient. A short
    // lookahead stage catches that transient before it reaches the output.
    processPeakLimiter (buffer, bypassed);

    float outputPeak = 0.0f;
    for (int channel = 0; channel < numOutputChannels; ++channel)
        outputPeak = juce::jmax (outputPeak, buffer.getMagnitude (channel, 0, buffer.getNumSamples()));
    updateMeter (outputLevel, outputPeak);
}

void AudioPluginAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer,
                                                       juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto numInputChannels = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();
    float inputPeak = 0.0f;

    for (int channel = 0; channel < numInputChannels; ++channel)
        inputPeak = juce::jmax (inputPeak, buffer.getMagnitude (channel, 0, buffer.getNumSamples()));

    for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (numInputChannels == 1 && numOutputChannels == 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());

    updateMeter (inputLevel, inputPeak);
    compressorEnvelopeDb = 0.0f;
    peakCompressorEnvelopeDb = 0.0f;
    gainReductionDb.store (gainReductionDb.load() * 0.82f);
    processPeakLimiter (buffer, true);

    float outputPeak = 0.0f;
    for (int channel = 0; channel < numOutputChannels; ++channel)
        outputPeak = juce::jmax (outputPeak, buffer.getMagnitude (channel, 0, buffer.getNumSamples()));
    updateMeter (outputLevel, outputPeak);
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
