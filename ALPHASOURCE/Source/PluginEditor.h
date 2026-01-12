#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class BrainwaveEntrainmentFXAudioProcessorEditor : public juce::AudioProcessorEditor,
    private juce::Timer {
public:
    BrainwaveEntrainmentFXAudioProcessorEditor(BrainwaveEntrainmentFXAudioProcessor&);
    ~BrainwaveEntrainmentFXAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    BrainwaveEntrainmentFXAudioProcessor& audioProcessor;

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> frequencyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> beatOffsetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wetDryAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modulationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> carrierFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> carrierBlendAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stereoWidthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sidechainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hemiCorrelationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hemiDriftAttachment;

    // UI Components
    juce::TextButton bypassButton;

    juce::ComboBox modeSelector;
    juce::ComboBox frequencySelector;

    juce::Slider beatOffsetSlider;
    juce::Slider wetDrySlider;
    juce::Slider modulationSlider;
    juce::Slider carrierFreqSlider;
    juce::Slider carrierBlendSlider;
    juce::Slider stereoWidthSlider;
    juce::Slider sidechainSlider;
    juce::Slider hemiCorrelationSlider;
    juce::Slider hemiDriftSlider;

    juce::Label titleLabel;
    juce::Label modeLabel;
    juce::Label frequencyLabel;
    juce::Label beatOffsetLabel;
    juce::Label wetDryLabel;
    juce::Label modulationLabel;
    juce::Label carrierFreqLabel;
    juce::Label carrierBlendLabel;
    juce::Label stereoWidthLabel;
    juce::Label sidechainLabel;
    juce::Label hemiCorrelationLabel;
    juce::Label hemiDriftLabel;
    juce::Label statusLabel;

    // Metering
    float currentEnvelope = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrainwaveEntrainmentFXAudioProcessorEditor)
};