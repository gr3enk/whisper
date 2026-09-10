#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>

namespace ParameterIDs
{
    inline constexpr auto warmth      = "warmth";
    inline constexpr auto clarity     = "clarity";
    inline constexpr auto tingles     = "tingles";
    inline constexpr auto compression = "compression";
    inline constexpr auto threshold   = "threshold";
    inline constexpr auto output      = "output";
    inline constexpr auto bypass      = "bypass";
}

class AudioPluginAudioProcessor final : public juce::AudioProcessor
{
public:
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;
    float getInputLevel() const noexcept  { return inputLevel.load(); }
    float getOutputLevel() const noexcept { return outputLevel.load(); }
    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }
    void startThresholdLearning() noexcept;
    void stopThresholdLearning() noexcept;
    bool isThresholdLearning() const noexcept { return thresholdLearning.load(); }
    float getThresholdLearningProgress() const noexcept { return thresholdLearningProgress.load(); }
    bool consumeLearnedThreshold (float& thresholdDb) noexcept;

private:
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                   juce::dsp::IIR::Coefficients<float>>;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateProcessingSettings();
    void processCompression (juce::AudioBuffer<float>& buffer);
    void processPeakCompression (juce::AudioBuffer<float>& buffer);
    void processPeakLimiter (juce::AudioBuffer<float>& buffer, bool bypassed);
    void analyseThreshold (const juce::AudioBuffer<float>& buffer, int numInputChannels);
    void finishThresholdLearning() noexcept;
    static void updateMeter (std::atomic<float>& meter, float newValue) noexcept;

    Filter highPass, warmthFilter, clarityFilter, airFilter;
    juce::dsp::Gain<float> outputGain;
    double currentSampleRate = 44100.0;
    float compressorThresholdDb = -18.0f;
    float compressorRatio = 3.0f;
    float compressorAttackMs = 12.0f;
    float compressorReleaseMs = 140.0f;
    float compressorEnvelopeDb = 0.0f;
    float peakCompressorEnvelopeDb = 0.0f;
    juce::AudioBuffer<float> limiterDelayBuffer;
    int limiterWritePosition = 0;
    int limiterLookAheadSamples = 1;
    int limiterHoldSamples = 0;
    float limiterEnvelopeDb = 0.0f;
    float lastWarmth = -1.0f;
    float lastClarity = -1.0f;
    float lastTingles = -1.0f;
    float lastCompression = -1.0f;
    float lastThreshold = -100.0f;
    float lastOutput = -100.0f;

    static constexpr int thresholdHistogramBins = 81;
    std::array<juce::uint32, thresholdHistogramBins> thresholdHistogram {};
    int thresholdFrameSamples = 1;
    int thresholdSamplesInFrame = 0;
    int thresholdFramesAnalysed = 0;
    int thresholdTargetFrames = 1;
    float thresholdFramePeak = 0.0f;
    std::atomic<bool> thresholdLearnRequested { false };
    std::atomic<bool> thresholdStopRequested { false };
    std::atomic<bool> thresholdLearning { false };
    std::atomic<float> thresholdLearningProgress { 0.0f };
    std::atomic<float> pendingLearnedThresholdDb { 100.0f };

    std::atomic<float> inputLevel { 0.0f };
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> gainReductionDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
