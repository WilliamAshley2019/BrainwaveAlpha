#include "PluginProcessor.h"
#include "PluginEditor.h"

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

BrainwaveEntrainmentAudioProcessorEditor::BrainwaveEntrainmentAudioProcessorEditor(
    BrainwaveEntrainmentAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p) {

    setSize(600, 700);  // Smaller since we removed play button and duration controls

    // Title
    titleLabel.setText("Brainwave Entrainment FX", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    // Mode Selector
    modeLabel.setText("Mode", juce::dontSendNotification);
    modeLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(modeLabel);

    modeSelector.addItemList(juce::StringArray{ "Binaural", "Monaural", "Isochronic", "Hybrid", "Bilateral Sync" }, 1);
    addAndMakeVisible(modeSelector);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getValueTreeState(), "entrainment_mode", modeSelector);

    // Frequency Selector
    frequencyLabel.setText("Brainwave Band", juce::dontSendNotification);
    frequencyLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(frequencyLabel);

    frequencySelector.addItemList(juce::StringArray{
        "Delta (1-4Hz)", "Theta (4-8Hz)", "Alpha (8-13Hz)",
        "Beta (13-30Hz)", "Gamma (30-100Hz)",
        "Focus 3 (4Hz)", "Focus 10 (7.5Hz)", "Focus 12 (10Hz)",
        "Focus 15 (12Hz)", "Focus 21 (20Hz)" }, 1);
    addAndMakeVisible(frequencySelector);
    frequencyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getValueTreeState(), "brainwave_frequency", frequencySelector);

    // Wet/Dry Mix (CRITICAL for effect!)
    wetMixLabel.setText("Entrainment Mix", juce::dontSendNotification);
    wetMixLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(wetMixLabel);

    wetMixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    wetMixSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    addAndMakeVisible(wetMixSlider);
    wetMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "wet_mix", wetMixSlider);

    // Waveform Selector
    waveformLabel.setText("Waveform", juce::dontSendNotification);
    waveformLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(waveformLabel);

    waveformSelector.addItemList(juce::StringArray{
        "Sine", "Triangle", "Sawtooth", "Square", "Pulse",
        "Noise", "Kick", "Snare", "Hat Closed", "Hat Open" }, 1);
    addAndMakeVisible(waveformSelector);
    waveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getValueTreeState(), "waveform", waveformSelector);

    // Solfeggio Preset Selector
    solfeggioLabel.setText("Carrier Preset", juce::dontSendNotification);
    solfeggioLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(solfeggioLabel);

    solfeggioSelector.addItemList(juce::StringArray{
        "Manual", "174Hz (UT)", "285Hz (RE)", "396Hz (MI)",
        "417Hz (FA)", "528Hz (SOL)", "639Hz (LA)",
        "741Hz (TI)", "852Hz", "963Hz" }, 1);
    addAndMakeVisible(solfeggioSelector);
    solfeggioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getValueTreeState(), "solfeggio_preset", solfeggioSelector);

    // Beat Offset
    beatOffsetLabel.setText("Beat Fine Tune", juce::dontSendNotification);
    beatOffsetLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(beatOffsetLabel);

    beatOffsetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    beatOffsetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    addAndMakeVisible(beatOffsetSlider);
    beatOffsetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "beat_offset", beatOffsetSlider);

    // Carrier Frequency
    carrierLabel.setText("Carrier Frequency", juce::dontSendNotification);
    carrierLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(carrierLabel);

    carrierSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    carrierSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    addAndMakeVisible(carrierSlider);
    carrierAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "carrier_frequency", carrierSlider);

    // Modulation Depth
    modDepthLabel.setText("Mod Depth", juce::dontSendNotification);
    modDepthLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(modDepthLabel);

    modDepthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    modDepthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    addAndMakeVisible(modDepthSlider);
    modDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "modulation_depth", modDepthSlider);

    // Noise Mix
    noiseLabel.setText("Noise Mix", juce::dontSendNotification);
    noiseLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(noiseLabel);

    noiseSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    noiseSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    addAndMakeVisible(noiseSlider);
    noiseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "noise_amount", noiseSlider);

    // Hemi-Sync Correlation
    hemiCorrelationLabel.setText("Noise Correlation (HS)", juce::dontSendNotification);
    hemiCorrelationLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(hemiCorrelationLabel);

    hemiCorrelationSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    hemiCorrelationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    addAndMakeVisible(hemiCorrelationSlider);
    hemiCorrelationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "hemisync_correlation", hemiCorrelationSlider);

    // Hemi-Sync Drift
    hemiDriftLabel.setText("Hemispheric Drift (HS)", juce::dontSendNotification);
    hemiDriftLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(hemiDriftLabel);

    hemiDriftSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    hemiDriftSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    addAndMakeVisible(hemiDriftSlider);
    hemiDriftAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "hemisync_drift", hemiDriftSlider);

    // Master Gain
    gainLabel.setText("Master Gain", juce::dontSendNotification);
    gainLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(gainLabel);

    gainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
    addAndMakeVisible(gainSlider);
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getValueTreeState(), "master_gain", gainSlider);

    // Status Label (now shows current beat frequency)
    statusLabel.setText("Active", juce::dontSendNotification);
    statusLabel.setFont(juce::FontOptions(14.0f));
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    // Beat Frequency Display
    beatLabel.setText("Beat: -- Hz", juce::dontSendNotification);
    beatLabel.setFont(juce::FontOptions(12.0f));
    beatLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(beatLabel);

    // RMS Monitoring
    rmsLabel.setText("SPL: -- dB", juce::dontSendNotification);
    rmsLabel.setFont(juce::FontOptions(12.0f));
    rmsLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(rmsLabel);

    startTimerHz(30);
}

BrainwaveEntrainmentAudioProcessorEditor::~BrainwaveEntrainmentAudioProcessorEditor() {
    stopTimer();
}

// ============================================================================
// LAYOUT
// ============================================================================

void BrainwaveEntrainmentAudioProcessorEditor::resized() {
    auto area = getLocalBounds();
    auto margin = 10;

    area.reduce(margin, margin);

    // Title
    titleLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(10);

    // Status
    statusLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(5);

    // Beat and RMS monitoring
    auto monitorRow = area.removeFromTop(25);
    beatLabel.setBounds(monitorRow.removeFromLeft(monitorRow.getWidth() / 2));
    rmsLabel.setBounds(monitorRow);
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

    // Selectors and sliders
    createRow(modeLabel, modeSelector);
    createRow(frequencyLabel, frequencySelector);
    createRow(wetMixLabel, wetMixSlider);
    createRow(waveformLabel, waveformSelector);
    createRow(solfeggioLabel, solfeggioSelector);

    area.removeFromTop(10);

    createRow(beatOffsetLabel, beatOffsetSlider);
    createRow(carrierLabel, carrierSlider);
    createRow(modDepthLabel, modDepthSlider);
    createRow(noiseLabel, noiseSlider);
    createRow(hemiCorrelationLabel, hemiCorrelationSlider);
    createRow(hemiDriftLabel, hemiDriftSlider);
    createRow(gainLabel, gainSlider);
}

// ============================================================================
// PAINTING
// ============================================================================

void BrainwaveEntrainmentAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a2e));

    auto area = getLocalBounds();

    // Header gradient
    auto headerArea = area.removeFromTop(100);
    juce::ColourGradient gradient(
        juce::Colour(0xff0f3460), static_cast<float>(headerArea.getX()), static_cast<float>(headerArea.getY()),
        juce::Colour(0xff1a1a2e), static_cast<float>(headerArea.getX()), static_cast<float>(headerArea.getBottom()),
        false);
    g.setGradientFill(gradient);
    g.fillRect(headerArea);

    // Draw a subtle visualizer for the beat frequency
    auto beatHz = audioProcessor.getCurrentBeatFrequency();
    if (beatHz > 0.5f) {
        auto visArea = getLocalBounds();
        visArea.removeFromTop(110);
        visArea.removeFromBottom(getHeight() - 150);
        visArea.reduce(20, 10);

        g.setColour(juce::Colour(0xff2d2d44));
        g.fillRoundedRectangle(visArea.toFloat(), 5.0f);

        // Draw waveform visualization based on beat frequency
        int numPoints = 50;
        juce::Path wavePath;
        wavePath.startNewSubPath(visArea.getX(), visArea.getCentreY());

        for (int i = 1; i <= numPoints; ++i) {
            float x = visArea.getX() + (visArea.getWidth() * i / static_cast<float>(numPoints));
            float y = visArea.getCentreY() +
                std::sin(beatHz * i * 0.5f) *
                std::sin(i * 0.1f) *
                (visArea.getHeight() * 0.4f);

            wavePath.lineTo(x, y);
        }

        g.setColour(juce::Colour(0xff16c79a).withAlpha(0.7f));
        g.strokePath(wavePath, juce::PathStrokeType(2.0f));
    }
}

// ============================================================================
// TIMER CALLBACK
// ============================================================================

void BrainwaveEntrainmentAudioProcessorEditor::timerCallback() {
    // Update beat frequency display
    float beatHz = audioProcessor.getCurrentBeatFrequency();
    beatLabel.setText("Beat: " + juce::String(beatHz, 2) + " Hz", juce::dontSendNotification);

    // Update RMS monitoring
    float leftRMS = audioProcessor.getLeftRMSLevel();
    float rightRMS = audioProcessor.getRightRMSLevel();
    float avgRMS = (leftRMS + rightRMS) * 0.5f;

    if (avgRMS > 0.0001f) {
        float splApprox = 20.0f * std::log10(avgRMS) + 94.0f;
        rmsLabel.setText("SPL: ~" + juce::String(splApprox, 1) + " dB", juce::dontSendNotification);
    }
    else {
        rmsLabel.setText("SPL: -- dB", juce::dontSendNotification);
    }

    // Update status based on mix level
    float wetMix = audioProcessor.getValueTreeState().getRawParameterValue("wet_mix")->load();
    if (wetMix < 0.01f) {
        statusLabel.setText("Bypassed (Mix = 0%)", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    }
    else if (wetMix > 0.99f) {
        statusLabel.setText("Entrainment Only", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    }
    else {
        statusLabel.setText("Active (Mix: " + juce::String(static_cast<int>(wetMix * 100)) + "%)", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
    }

    repaint();
}