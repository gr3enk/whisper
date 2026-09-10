#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace Colours
{
    const auto background = juce::Colour::fromRGB (18, 21, 25);
    const auto panel       = juce::Colour::fromRGB (28, 33, 39);
    const auto text        = juce::Colour::fromRGB (231, 235, 232);
    const auto muted       = juce::Colour::fromRGB (137, 150, 151);
    const auto mint        = juce::Colour::fromRGB (108, 226, 190);
    const auto lavender    = juce::Colour::fromRGB (173, 151, 242);
}

void AudioPluginAudioProcessorEditor::WhisperLookAndFeel::drawRotarySlider (
    juce::Graphics& g, int x, int y, int width, int height, float position,
    float startAngle, float endAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                           static_cast<float> (width), static_cast<float> (height)).reduced (9.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + position * (endAngle - startAngle);
    const auto lineWidth = juce::jmax (3.0f, radius * 0.12f);
    const auto arcBounds = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre).reduced (lineWidth);

    juce::Path backgroundArc, valueArc;
    backgroundArc.addCentredArc (centre.x, centre.y, arcBounds.getWidth() * 0.5f,
                                 arcBounds.getHeight() * 0.5f, 0.0f, startAngle, endAngle, true);
    valueArc.addCentredArc (centre.x, centre.y, arcBounds.getWidth() * 0.5f,
                            arcBounds.getHeight() * 0.5f, 0.0f, startAngle, angle, true);

    g.setColour (juce::Colour::fromRGB (54, 62, 69));
    g.strokePath (backgroundArc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
    g.strokePath (valueArc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    const auto pointerLength = radius * 0.54f;
    const auto pointerWidth = juce::jmax (2.0f, radius * 0.075f);
    juce::Path pointer;
    pointer.addRoundedRectangle (-pointerWidth * 0.5f, -pointerLength, pointerWidth, pointerLength, pointerWidth * 0.5f);
    g.setColour (Colours::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));

    g.setColour (juce::Colour::fromRGB (36, 42, 48));
    g.fillEllipse (juce::Rectangle<float> (radius * 0.92f, radius * 0.92f).withCentre (centre));
}

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);

    configureKnob (warmthSlider, warmthLabel, "WARMTH", "%");
    configureKnob (claritySlider, clarityLabel, "CLARITY", "%");
    configureKnob (tinglesSlider, tinglesLabel, "TINGLES", "%");
    configureKnob (compressionSlider, compressionLabel, "COMPRESSION", "%");
    configureKnob (outputSlider, outputLabel, "OUTPUT", " dB");

    warmthSlider.setColour (juce::Slider::rotarySliderFillColourId, Colours::mint);
    claritySlider.setColour (juce::Slider::rotarySliderFillColourId, Colours::mint);
    tinglesSlider.setColour (juce::Slider::rotarySliderFillColourId, Colours::lavender);
    compressionSlider.setColour (juce::Slider::rotarySliderFillColourId, Colours::lavender);
    outputSlider.setColour (juce::Slider::rotarySliderFillColourId, Colours::mint);

    bypassButton.setColour (juce::ToggleButton::textColourId, Colours::muted);
    bypassButton.setColour (juce::ToggleButton::tickColourId, Colours::mint);
    addAndMakeVisible (bypassButton);

    learnButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (43, 49, 56));
    learnButton.setColour (juce::TextButton::buttonOnColourId, Colours::lavender.darker (0.35f));
    learnButton.setColour (juce::TextButton::textColourOffId, Colours::text);
    learnButton.setColour (juce::TextButton::textColourOnId, Colours::text);
    learnButton.onClick = [this]
    {
        if (processorRef.isThresholdLearning())
            processorRef.stopThresholdLearning();
        else
            processorRef.startThresholdLearning();
    };
    addAndMakeVisible (learnButton);

    thresholdLabel.setJustificationType (juce::Justification::centredLeft);
    thresholdLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    thresholdLabel.setColour (juce::Label::textColourId, Colours::muted);
    addAndMakeVisible (thresholdLabel);

    peakLimitLabel.setText ("PEAK COMP", juce::dontSendNotification);
    peakLimitLabel.setJustificationType (juce::Justification::centredLeft);
    peakLimitLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    peakLimitLabel.setColour (juce::Label::textColourId, Colours::lavender);
    addAndMakeVisible (peakLimitLabel);

    auto& state = processorRef.parameters;
    warmthAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::warmth, warmthSlider);
    clarityAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::clarity, claritySlider);
    tinglesAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::tingles, tinglesSlider);
    compressionAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::compression, compressionSlider);
    outputAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::output, outputSlider);
    bypassAttachment = std::make_unique<ButtonAttachment> (state, ParameterIDs::bypass, bypassButton);

    setResizable (true, true);
    setResizeLimits (620, 360, 1100, 650);
    setSize (780, 440);
    startTimerHz (30);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void AudioPluginAudioProcessorEditor::configureKnob (juce::Slider& slider, juce::Label& label,
                                                       const juce::String& name, const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 22);
    slider.setTextValueSuffix (suffix);
    slider.setDoubleClickReturnValue (true, name == "OUTPUT" ? 0.0 : 50.0);
    slider.setColour (juce::Slider::textBoxTextColourId, Colours::text);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, Colours::muted);
    addAndMakeVisible (label);
}

void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    auto bounds = getLocalBounds().toFloat();
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (bounds.reduced (18.0f), 18.0f);

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (25.0f, juce::Font::bold));
    g.drawText ("whisper", 42, 29, 180, 34, juce::Justification::centredLeft);
    g.setColour (Colours::mint);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("ASMR DETAIL PROCESSOR", 43, 60, 190, 18, juce::Justification::centredLeft);

    auto graph = juce::Rectangle<float> (245.0f, 34.0f, static_cast<float> (getWidth()) - 350.0f, 58.0f);
    g.setColour (juce::Colour::fromRGB (42, 49, 56));
    g.drawHorizontalLine (juce::roundToInt (graph.getCentreY()), graph.getX(), graph.getRight());

    const auto w = processorRef.parameters.getRawParameterValue (ParameterIDs::warmth)->load() / 100.0f;
    const auto c = processorRef.parameters.getRawParameterValue (ParameterIDs::clarity)->load() / 100.0f;
    const auto t = processorRef.parameters.getRawParameterValue (ParameterIDs::tingles)->load() / 100.0f;
    juce::Path curve;
    for (int i = 0; i <= 80; ++i)
    {
        const auto xNorm = i / 80.0f;
        const auto low = std::exp (-std::pow ((xNorm - 0.18f) / 0.22f, 2.0f)) * (w - 0.33f);
        const auto presence = std::exp (-std::pow ((xNorm - 0.62f) / 0.18f, 2.0f)) * (c - 0.3f);
        const auto air = juce::jmax (0.0f, (xNorm - 0.68f) / 0.32f) * (t - 0.25f);
        const auto point = juce::Point<float> (graph.getX() + xNorm * graph.getWidth(),
                                               graph.getCentreY() - (low + presence + air) * 22.0f);
        i == 0 ? curve.startNewSubPath (point) : curve.lineTo (point);
    }
    g.setColour (Colours::mint.withAlpha (0.7f));
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));

    drawGainReductionMeter (g, { static_cast<float> (getWidth()) - 86.0f, 30.0f, 7.0f, 66.0f });
    drawMeter (g, { static_cast<float> (getWidth()) - 67.0f, 30.0f, 7.0f, 66.0f },
               processorRef.getInputLevel(), "IN");
    drawMeter (g, { static_cast<float> (getWidth()) - 48.0f, 30.0f, 7.0f, 66.0f },
               processorRef.getOutputLevel(), "OUT");

    g.setColour (juce::Colour::fromRGB (40, 46, 52));
    g.drawHorizontalLine (112, 38.0f, static_cast<float> (getWidth()) - 38.0f);
}

void AudioPluginAudioProcessorEditor::drawGainReductionMeter (juce::Graphics& g,
                                                               juce::Rectangle<float> area) const
{
    g.setColour (juce::Colour::fromRGB (43, 49, 55));
    g.fillRoundedRectangle (area, 3.0f);

    const auto reduction = juce::jlimit (0.0f, 18.0f, processorRef.getGainReductionDb());
    auto fill = area.withHeight (area.getHeight() * reduction / 18.0f);
    g.setColour (Colours::lavender);
    g.fillRoundedRectangle (fill, 3.0f);

    g.setColour (Colours::muted);
    g.setFont (juce::FontOptions (8.0f, juce::Font::bold));
    g.drawText ("GR", area.expanded (7.0f, 0.0f).translated (0.0f, 69.0f), juce::Justification::centredTop);
}

void AudioPluginAudioProcessorEditor::drawMeter (juce::Graphics& g, juce::Rectangle<float> area,
                                                  float level, const juce::String& label) const
{
    g.setColour (juce::Colour::fromRGB (43, 49, 55));
    g.fillRoundedRectangle (area, 3.0f);
    const auto db = juce::Decibels::gainToDecibels (level, -60.0f);
    const auto amount = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -60.0f, 0.0f, 0.0f, 1.0f));
    auto fill = area.withTop (area.getBottom() - area.getHeight() * amount);
    g.setColour (db > -3.0f ? juce::Colour::fromRGB (244, 126, 113) : Colours::mint);
    g.fillRoundedRectangle (fill, 3.0f);
    g.setColour (Colours::muted);
    g.setFont (juce::FontOptions (8.0f, juce::Font::bold));
    g.drawText (label, area.expanded (7.0f, 0.0f).translated (0.0f, 69.0f), juce::Justification::centredTop);
}

void AudioPluginAudioProcessorEditor::resized()
{
    auto content = getLocalBounds().reduced (36);
    content.removeFromTop (82);
    auto controls = content.reduced (4, 12);

    const auto knobWidth = controls.getWidth() / 5;
    juce::Slider* sliders[] = { &warmthSlider, &claritySlider, &tinglesSlider, &compressionSlider, &outputSlider };
    juce::Label* labels[] = { &warmthLabel, &clarityLabel, &tinglesLabel, &compressionLabel, &outputLabel };

    for (int i = 0; i < 5; ++i)
    {
        auto column = controls.removeFromLeft (i == 4 ? controls.getWidth() : knobWidth);
        labels[i]->setBounds (column.removeFromTop (24));
        sliders[i]->setBounds (column.reduced (4, 0));
    }

    bypassButton.setBounds (getWidth() - 128, getHeight() - 43, 90, 24);
    learnButton.setBounds (40, getHeight() - 45, 148, 27);
    thresholdLabel.setBounds (198, getHeight() - 44, 150, 25);
    peakLimitLabel.setBounds (348, getHeight() - 44, 145, 25);
}

void AudioPluginAudioProcessorEditor::timerCallback()
{
    float learnedThreshold = 0.0f;
    if (processorRef.consumeLearnedThreshold (learnedThreshold))
    {
        if (auto* parameter = processorRef.parameters.getParameter (ParameterIDs::threshold))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (learnedThreshold));
            parameter->endChangeGesture();
        }
    }

    const auto learning = processorRef.isThresholdLearning();
    learnButton.setToggleState (learning, juce::dontSendNotification);
    learnButton.setButtonText (learning
        ? "STOP  " + juce::String (juce::roundToInt (processorRef.getThresholdLearningProgress() * 100.0f)) + "%"
        : "LEARN THRESHOLD");

    const auto threshold = processorRef.parameters.getRawParameterValue (ParameterIDs::threshold)->load();
    thresholdLabel.setText ("THRESHOLD  " + juce::String (threshold, 1) + " dB",
                            juce::dontSendNotification);
    const auto peakThreshold = juce::jlimit (-48.0f, -6.0f, threshold + 12.0f);
    peakLimitLabel.setText ("PEAK COMP  " + juce::String (peakThreshold, 1) + " dB",
                            juce::dontSendNotification);
    repaint();
}
