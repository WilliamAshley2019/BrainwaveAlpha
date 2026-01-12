#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class BrainwaveEntrainmentAudioProcessorEditor : public juce::AudioProcessorEditor,
    private juce::Timer {
public:
    BrainwaveEntrainmentAudioProcessorEditor(BrainwaveEntrainmentAudioProcessor&);
    ~BrainwaveEntrainmentAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    BrainwaveEntrainmentAudioProcessor& audioProcessor;

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> frequencyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveformAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> solfeggioAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> beatOffsetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> carrierAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wetMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hemiCorrelationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hemiDriftAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;

    // UI Components
    juce::ComboBox modeSelector;
    juce::ComboBox frequencySelector;
    juce::ComboBox waveformSelector;
    juce::ComboBox solfeggioSelector;

    juce::Slider wetMixSlider;
    juce::Slider beatOffsetSlider;
    juce::Slider carrierSlider;
    juce::Slider modDepthSlider;
    juce::Slider noiseSlider;
    juce::Slider hemiCorrelationSlider;
    juce::Slider hemiDriftSlider;
    juce::Slider gainSlider;

    juce::Label titleLabel;
    juce::Label modeLabel;
    juce::Label frequencyLabel;
    juce::Label waveformLabel;
    juce::Label solfeggioLabel;
    juce::Label wetMixLabel;
    juce::Label beatOffsetLabel;
    juce::Label carrierLabel;
    juce::Label modDepthLabel;
    juce::Label noiseLabel;
    juce::Label hemiCorrelationLabel;
    juce::Label hemiDriftLabel;
    juce::Label gainLabel;
    juce::Label statusLabel;
    juce::Label rmsLabel;
    juce::Label beatLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrainwaveEntrainmentAudioProcessorEditor)
};