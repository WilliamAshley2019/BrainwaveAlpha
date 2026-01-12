#include "PluginProcessor.h"
#include "PluginEditor.h"

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

BrainwaveEntrainmentFXAudioProcessorEditor::BrainwaveEntrainmentFXAudioProcessorEditor(
    BrainwaveEntrainmentFXAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {

    setSize(600, 700);

    // Title
    titleLabel.setText("Brainwave Entrainment FX", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    // Bypass Button
    bypassButton.setButtonText("BYPASS");
    bypassButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    bypassButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getValueTreeState(), "bypass", bypassButton);

    // Processing Mode
    modeLabel.setText("Processing Mode", juce::dontSendNotification);
    modeLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(modeLabel);

    modeSelector.addItemList(juce::StringArray{ "Binaural Pan", "Isochronic Gate", "Hemi-Sync", "Frequency Shift", "Hybrid" }, 1);
    addAndMakeVisible(modeSelector);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getValueTreeState(), "processing_mode", modeSelector);

    // Brainwave Frequency
    frequencyLabel.setText("Brainwave Band", juce::dontSendNotification);
    frequencyLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(frequencyLabel);

    frequencySelector.addItemList(juce::StringArray{
        "Delta (1-4Hz)", "Theta (4-8Hz)", "Alpha (8-13Hz)",
        "Beta (13-30Hz)", "Gamma (30-100Hz)" }, 1);
    addAndMakeVisible(frequencySelector);
    frequencyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getValueTreeState(), "brainwave_frequency", frequencySelector);

    // Helper lambda for slider setup
    auto setupSlider = [this](juce::Slider& slider, juce::Label& label,
        const juce::String& labelText, const juce::String& paramID,
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment) {
            label.setText(labelText, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centredLeft);
            addAndMakeVisible(label);

            slider.setSliderStyle(juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
            addAndMakeVisible(slider);

            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.getValueTreeState(), paramID, slider);
        };

    // Setup all sliders
    setupSlider(beatOffsetSlider, beatOffsetLabel, "Beat Fine Tune", "beat_offset", beatOffsetAttachment);
    setupSlider(wetDrySlider, wetDryLabel, "Wet/Dry Mix", "wet_dry_mix", wetDryAttachment);
    setupSlider(modulationSlider, modulationLabel, "Modulation Depth", "modulation_depth", modulationAttachment);
    setupSlider(carrierFreqSlider, carrierFreqLabel, "Carrier Frequency", "carrier_frequency", carrierFreqAttachment);
    setupSlider(carrierBlendSlider, carrierBlendLabel, "Carrier Mix", "carrier_blend", carrierBlendAttachment);
    setupSlider(stereoWidthSlider, stereoWidthLabel, "Stereo Width", "stereo_width", stereoWidthAttachment);
    setupSlider(sidechainSlider, sidechainLabel, "Sidechain Depth", "sidechain_depth", sidechainAttachment);
    setupSlider(hemiCorrelationSlider, hemiCorrelationLabel, "Noise Correlation (HS)", "hemisync_correlation", hemiCorrelationAttachment);
    setupSlider(hemiDriftSlider, hemiDriftLabel, "Hemispheric Drift (HS)", "hemisync_drift", hemiDriftAttachment);

    // Status Label
    statusLabel.setText("Processing Active", juce::dontSendNotification);
    statusLabel.setFont(juce::FontOptions(14.0f));
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    startTimerHz(30);
}

BrainwaveEntrainmentFXAudioProcessorEditor::~BrainwaveEntrainmentFXAudioProcessorEditor() {
    stopTimer();
}

// ============================================================================
// LAYOUT
// ============================================================================

void BrainwaveEntrainmentFXAudioProcessorEditor::resized() {
    auto area = getLocalBounds();
    auto margin = 10;

    area.reduce(margin, margin);

    // Title
    titleLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(10);

    // Bypass button
    auto bypassArea = area.removeFromTop(40);
    bypassButton.setBounds(bypassArea.reduced(bypassArea.getWidth() / 3, 5));
    area.removeFromTop(10);

    // Status
    statusLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(15);

    auto labelWidth = 180;
    auto rowHeight = 30;
    auto spacing = 5;

    auto createRow = [&](juce::Label& label, juce::Component& component) {
        auto row = area.removeFromTop(rowHeight);
        label.setBounds(row.removeFromLeft(labelWidth));
        row.removeFromLeft(spacing);
        component.setBounds(row);
        area.removeFromTop(spacing);
        };

    // Selectors
    createRow(modeLabel, modeSelector);
    createRow(frequencyLabel, frequencySelector);

    area.removeFromTop(10);

    // Sliders
    createRow(beatOffsetLabel, beatOffsetSlider);
    createRow(wetDryLabel, wetDrySlider);
    createRow(modulationLabel, modulationSlider);

    area.removeFromTop(5);

    createRow(carrierFreqLabel, carrierFreqSlider);
    createRow(carrierBlendLabel, carrierBlendSlider);

    area.removeFromTop(5);

    createRow(stereoWidthLabel, stereoWidthSlider);
    createRow(sidechainLabel, sidechainSlider);

    area.removeFromTop(5);

    createRow(hemiCorrelationLabel, hemiCorrelationSlider);
    createRow(hemiDriftLabel, hemiDriftSlider);
}

// ============================================================================
// PAINTING
// ============================================================================

void BrainwaveEntrainmentFXAudioProcessorEditor::paint(juce::Graphics& g) {
    // Background
    g.fillAll(juce::Colour(0xff1a1a2e));

    auto area = getLocalBounds();

    // Header gradient
    auto headerArea = area.removeFromTop(100);
    juce::ColourGradient gradient(
        juce::Colour(0xff16213e), static_cast<float>(headerArea.getX()), static_cast<float>(headerArea.getY()),
        juce::Colour(0xff1a1a2e), static_cast<float>(headerArea.getX()), static_cast<float>(headerArea.getBottom()),
        false);
    g.setGradientFill(gradient);
    g.fillRect(headerArea);

    // Envelope meter (if sidechain is active)
    if (currentEnvelope > 0.01f) {
        auto meterArea = getLocalBounds();
        meterArea.removeFromTop(130);
        meterArea.removeFromBottom(getHeight() - 160);
        meterArea.reduce(20, 0);

        // Background
        g.setColour(juce::Colour(0xff2d2d44));
        g.fillRoundedRectangle(meterArea.toFloat(), 3.0f);

        // Level
        auto levelWidth = static_cast<int>(static_cast<float>(meterArea.getWidth()) * currentEnvelope);
        auto levelArea = meterArea.removeFromLeft(levelWidth);

        g.setColour(juce::Colour(0xff00d9ff));
        g.fillRoundedRectangle(levelArea.toFloat(), 3.0f);
    }
}

// ============================================================================
// TIMER CALLBACK
// ============================================================================

void BrainwaveEntrainmentFXAudioProcessorEditor::timerCallback() {
    currentEnvelope = audioProcessor.getCurrentEnvelope();

    if (audioProcessor.isActive()) {
        auto beatHz = audioProcessor.getCurrentBeatFrequency();
        statusLabel.setText("Processing: " + juce::String(beatHz, 2) + " Hz",
            juce::dontSendNotification);
    }
    else {
        statusLabel.setText("Bypassed", juce::dontSendNotification);
    }

    repaint();
}