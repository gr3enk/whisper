#pragma once

#include "PluginProcessor.h"

class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class WhisperLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        void drawRotarySlider (juce::Graphics&, int, int, int, int, float,
                               float, float, juce::Slider&) override;
    };

    void timerCallback() override;
    void configureKnob (juce::Slider&, juce::Label&, const juce::String&, const juce::String& suffix);
    void drawMeter (juce::Graphics&, juce::Rectangle<float>, float level, const juce::String& label) const;
    void drawGainReductionMeter (juce::Graphics&, juce::Rectangle<float>) const;

    AudioPluginAudioProcessor& processorRef;
    WhisperLookAndFeel lookAndFeel;

    juce::Slider warmthSlider, claritySlider, tinglesSlider, compressionSlider, outputSlider;
    juce::Label warmthLabel, clarityLabel, tinglesLabel, compressionLabel, outputLabel;
    juce::ToggleButton bypassButton { "BYPASS" };
    juce::TextButton learnButton { "LEARN THRESHOLD" };
    juce::Label thresholdLabel, peakLimitLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> warmthAttachment, clarityAttachment, tinglesAttachment,
                                      compressionAttachment, outputAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
