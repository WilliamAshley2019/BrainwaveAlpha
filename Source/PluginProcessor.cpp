#include "PluginProcessor.h"
#include "PluginEditor.h"

// ============================================================================
// AUDIO PROCESSOR EDITOR IMPLEMENTATION
// ============================================================================

juce::AudioProcessorEditor* BrainwaveEntrainmentAudioProcessor::createEditor() {
    return new BrainwaveEntrainmentAudioProcessorEditor(*this);
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

BrainwaveEntrainmentAudioProcessor::BrainwaveEntrainmentAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    parameters(*this, nullptr, "Parameters", createParameterLayout()) {

    // Add parameter listeners
    parameters.addParameterListener("brainwave_frequency", this);
    parameters.addParameterListener("entrainment_mode", this);
    parameters.addParameterListener("carrier_frequency", this);
    parameters.addParameterListener("solfeggio_preset", this);
    parameters.addParameterListener("beat_offset", this);
    parameters.addParameterListener("waveform", this);
    parameters.addParameterListener("wet_mix", this);
    parameters.addParameterListener("modulation_depth", this);
    parameters.addParameterListener("hemisync_correlation", this);
    parameters.addParameterListener("hemisync_drift", this);
}

BrainwaveEntrainmentAudioProcessor::~BrainwaveEntrainmentAudioProcessor() {
    parameters.removeParameterListener("brainwave_frequency", this);
    parameters.removeParameterListener("entrainment_mode", this);
    parameters.removeParameterListener("carrier_frequency", this);
    parameters.removeParameterListener("solfeggio_preset", this);
    parameters.removeParameterListener("beat_offset", this);
    parameters.removeParameterListener("waveform", this);
    parameters.removeParameterListener("wet_mix", this);
    parameters.removeParameterListener("modulation_depth", this);
    parameters.removeParameterListener("hemisync_correlation", this);
    parameters.removeParameterListener("hemisync_drift", this);
}

// ============================================================================
// AUDIO PROCESSING
// ============================================================================

void BrainwaveEntrainmentAudioProcessor::prepareToPlay(double sr, int samplesPerBlock) {
    sampleRate = sr;

    carrierOsc.setSampleRate(sr);
    leftModOsc.setSampleRate(sr);
    rightModOsc.setSampleRate(sr);

    // Setup smoothed values
    currentBeatHz.reset(sr, 0.05);
    carrierHz.reset(sr, 0.05);
    wetMixSmooth.reset(sr, 0.01);
    modulationDepthSmooth.reset(sr, 0.05);

    // Setup spectral asymmetry filters
    leftFilter.setLowpass(sr, 2000.0f, 0.707f);
    rightFilter.setLowpass(sr, 2400.0f, 0.707f);

    // Initialize entrainment buffer
    entrainmentBuffer.setSize(2, samplesPerBlock);

    updateFrequencies();
}

void BrainwaveEntrainmentAudioProcessor::releaseResources() {
    entrainmentBuffer.setSize(0, 0);
}

void BrainwaveEntrainmentAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages) {
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Ensure we have stereo input for processing
    if (totalNumInputChannels < 2) {
        // If mono input, duplicate to stereo
        if (totalNumInputChannels == 1) {
            buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
        }
    }

    // Generate entrainment signal
    entrainmentBuffer.clear();
    applyEntrainmentToInput(buffer);

    // Apply master gain
    auto masterGain = parameters.getRawParameterValue("master_gain")->load();
    auto gainLinear = juce::Decibels::decibelsToGain(masterGain);
    buffer.applyGain(gainLinear);
}

void BrainwaveEntrainmentAudioProcessor::applyEntrainmentToInput(juce::AudioBuffer<float>& buffer) {
    auto numSamples = buffer.getNumSamples();
    auto numChannels = juce::jmin(buffer.getNumChannels(), 2); // Fixed: Added juce:: namespace

    // Get current parameter values
    auto wetMix = wetMixSmooth.getNextValue();
    auto noiseAmount = parameters.getRawParameterValue("noise_amount")->load();
    auto hemiDrift = parameters.getRawParameterValue("hemisync_drift")->load();
    correlationAmount = parameters.getRawParameterValue("hemisync_correlation")->load();

    // RMS accumulators
    float leftAccum = 0.0f;
    float rightAccum = 0.0f;

    // Generate entrainment signal
    for (int sample = 0; sample < numSamples; ++sample) {
        float beatHz = currentBeatHz.getNextValue();
        float carrier = carrierHz.getNextValue();
        float modDepthSmooth = modulationDepthSmooth.getNextValue();

        float time = static_cast<float>(sample) / static_cast<float>(sampleRate);

        float leftEntrainment = 0.0f;
        float rightEntrainment = 0.0f;

        // ====================================================================
        // BILATERAL SYNC MODE
        // ====================================================================
        if (currentMode == EntrainmentMode::BilateralSync) {
            sharedPhase += carrier / static_cast<float>(sampleRate);
            if (sharedPhase >= 1.0f) sharedPhase -= 1.0f;

            driftPhase += (0.02f * hemiDrift) / static_cast<float>(sampleRate);
            if (driftPhase >= 1.0f) driftPhase -= 1.0f;

            float driftModulation = std::sin(driftPhase * juce::MathConstants<float>::twoPi) * 0.1f;

            float leftPhase = sharedPhase + (beatHz * 0.5f / carrier) + driftModulation;
            float rightPhase = sharedPhase - (beatHz * 0.5f / carrier) - driftModulation;

            float leftCarrier = std::sin(leftPhase * juce::MathConstants<float>::twoPi);
            float rightCarrier = std::sin(rightPhase * juce::MathConstants<float>::twoPi);

            float sharedNoise = noiseGen.generatePink();
            float independentNoise = noiseGen.generatePink();

            float leftNoise = sharedNoise * correlationAmount +
                independentNoise * (1.0f - correlationAmount);
            float rightNoise = sharedNoise * correlationAmount +
                noiseGen.generatePink() * (1.0f - correlationAmount);

            leftEntrainment = leftCarrier * (1.0f - noiseAmount) + leftNoise * noiseAmount;
            rightEntrainment = rightCarrier * (1.0f - noiseAmount) + rightNoise * noiseAmount;

            float am = 0.5f * (1.0f + std::sin(juce::MathConstants<float>::twoPi * beatHz * time));
            am = juce::jlimit(0.0f, 1.0f, am * modDepthSmooth * 0.3f + 0.7f);

            leftEntrainment *= am;
            rightEntrainment *= am;

            leftEntrainment = leftFilter.process(leftEntrainment);
            rightEntrainment = rightFilter.process(rightEntrainment);
        }
        // ====================================================================
        // STANDARD MODES
        // ====================================================================
        else {
            float leftTone = 0.0f;
            float rightTone = 0.0f;

            sharedPhase += carrier / static_cast<float>(sampleRate);
            if (sharedPhase >= 1.0f) sharedPhase -= 1.0f;

            switch (currentMode) {
            case EntrainmentMode::Binaural: {
                leftModOsc.setFrequency(carrier + beatHz * 0.5f);
                rightModOsc.setFrequency(carrier - beatHz * 0.5f);
                leftTone = leftModOsc.process();
                rightTone = rightModOsc.process();
                break;
            }

            case EntrainmentMode::Monaural: {
                leftModOsc.setFrequency(carrier + beatHz * 0.5f);
                rightModOsc.setFrequency(carrier - beatHz * 0.5f);
                float mono = (leftModOsc.process() + rightModOsc.process()) * 0.5f;
                leftTone = mono;
                rightTone = mono;
                break;
            }

            case EntrainmentMode::Isochronic: {
                carrierOsc.setFrequency(carrier);
                float tone = carrierOsc.process();
                float gate = 0.5f * (1.0f + std::sin(juce::MathConstants<float>::twoPi * beatHz * time));
                gate = juce::jlimit(0.0f, 1.0f, gate * modDepthSmooth);
                leftTone = tone * gate;
                rightTone = tone * gate;
                break;
            }

            case EntrainmentMode::Hybrid: {
                leftModOsc.setFrequency(carrier + beatHz * 0.5f);
                rightModOsc.setFrequency(carrier - beatHz * 0.5f);
                leftTone = leftModOsc.process();
                rightTone = rightModOsc.process();

                float gate = 0.5f * (1.0f + std::sin(juce::MathConstants<float>::twoPi * beatHz * time));
                gate = juce::jlimit(0.0f, 1.0f, gate * modDepthSmooth * 0.5f + 0.5f);
                leftTone *= gate;
                rightTone *= gate;
                break;
            }

            default:
                break;
            }

            if (noiseAmount > 0.01f) {
                float noise = noiseGen.generatePink();
                leftTone = leftTone * (1.0f - noiseAmount) + noise * noiseAmount;
                rightTone = rightTone * (1.0f - noiseAmount) + noise * noiseAmount;
            }

            leftEntrainment = leftTone;
            rightEntrainment = rightTone;
        }

        // Store entrainment signal
        entrainmentBuffer.setSample(0, sample, leftEntrainment);
        entrainmentBuffer.setSample(1, sample, rightEntrainment);
    }

    // Mix input with entrainment signal
    for (int channel = 0; channel < numChannels; ++channel) {
        auto* inputData = buffer.getWritePointer(channel);
        auto* entrainmentData = entrainmentBuffer.getReadPointer(channel);

        for (int sample = 0; sample < numSamples; ++sample) {
            float wet = wetMixSmooth.getNextValue();
            float dry = 1.0f - wet;

            inputData[sample] = (inputData[sample] * dry) + (entrainmentData[sample] * wet);

            // Accumulate for RMS
            if (channel == 0) leftAccum += inputData[sample] * inputData[sample];
            if (channel == 1) rightAccum += inputData[sample] * inputData[sample];
        }
    }

    // Calculate RMS
    if (numSamples > 0) {
        leftRMS = std::sqrt(leftAccum / static_cast<float>(numSamples));
        rightRMS = std::sqrt(rightAccum / static_cast<float>(numSamples));
    }
}

// ============================================================================
// PARAMETER HANDLING
// ============================================================================

void BrainwaveEntrainmentAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue) {
    if (parameterID == "brainwave_frequency") {
        currentFrequency = static_cast<BrainwaveFrequency>(static_cast<int>(newValue));
        updateFrequencies();
    }
    else if (parameterID == "entrainment_mode") {
        currentMode = static_cast<EntrainmentMode>(static_cast<int>(newValue));
        updateFrequencies();
    }
    else if (parameterID == "carrier_frequency" || parameterID == "solfeggio_preset") {
        updateFrequencies();
    }
    else if (parameterID == "beat_offset") {
        updateFrequencies();
    }
    else if (parameterID == "waveform") {
        auto waveform = static_cast<Waveform>(static_cast<int>(newValue));
        carrierOsc.setWaveform(waveform);
        leftModOsc.setWaveform(waveform);
        rightModOsc.setWaveform(waveform);
    }
    else if (parameterID == "wet_mix") {
        wetMixSmooth.setTargetValue(newValue);
    }
    else if (parameterID == "modulation_depth") {
        modulationDepthSmooth.setTargetValue(newValue);
    }
}

void BrainwaveEntrainmentAudioProcessor::updateFrequencies() {
    // Get base brainwave frequency
    float baseHz = 1.0f;
    switch (currentFrequency) {
    case BrainwaveFrequency::Delta: baseHz = 2.0f; break;
    case BrainwaveFrequency::Theta: baseHz = 6.0f; break;
    case BrainwaveFrequency::Alpha: baseHz = 10.0f; break;
    case BrainwaveFrequency::Beta: baseHz = 20.0f; break;
    case BrainwaveFrequency::Gamma: baseHz = 40.0f; break;
    case BrainwaveFrequency::Focus3: baseHz = 4.0f; break;
    case BrainwaveFrequency::Focus10: baseHz = 7.5f; break;
    case BrainwaveFrequency::Focus12: baseHz = 10.0f; break;
    case BrainwaveFrequency::Focus15: baseHz = 12.0f; break;
    case BrainwaveFrequency::Focus21: baseHz = 20.0f; break;
    }

    // Apply solfeggio preset if selected
    int solfeggioPreset = static_cast<int>(parameters.getRawParameterValue("solfeggio_preset")->load());
    float carrier = parameters.getRawParameterValue("carrier_frequency")->load();

    // Override carrier with solfeggio frequency if not manual
    if (solfeggioPreset > 0) {
        switch (solfeggioPreset) {
        case 1: carrier = 174.0f; break;  // UT
        case 2: carrier = 285.0f; break;  // RE
        case 3: carrier = 396.0f; break;  // MI
        case 4: carrier = 417.0f; break;  // FA
        case 5: carrier = 528.0f; break;  // SOL
        case 6: carrier = 639.0f; break;  // LA
        case 7: carrier = 741.0f; break;  // TI
        case 8: carrier = 852.0f; break;
        case 9: carrier = 963.0f; break;
        }
    }

    // Add user offset
    float beatOffset = parameters.getRawParameterValue("beat_offset")->load();
    float finalBeatHz = juce::jlimit(0.5f, 100.0f, baseHz + beatOffset);

    currentBeatHz.setTargetValue(finalBeatHz);
    carrierHz.setTargetValue(carrier);
}

// ============================================================================
// PARAMETER LAYOUT
// ============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
BrainwaveEntrainmentAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Entrainment mode
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "entrainment_mode", "Entrainment Mode",
        juce::StringArray{ "Binaural", "Monaural", "Isochronic", "Hybrid", "Bilateral Sync" }, 0));

    // Brainwave frequency
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "brainwave_frequency", "Brainwave Band",
        juce::StringArray{
            "Delta (1-4Hz)", "Theta (4-8Hz)", "Alpha (8-13Hz)",
            "Beta (13-30Hz)", "Gamma (30-100Hz)",
            "Focus 3 (4Hz)", "Focus 10 (7.5Hz)", "Focus 12 (10Hz)",
            "Focus 15 (12Hz)", "Focus 21 (20Hz)"
        }, 2));

    // Wet/Dry mix (most important for effect!)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "wet_mix", 1 }, "Entrainment Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(static_cast<int>(value * 100.0f)) + "%"; })));

    // Beat offset
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "beat_offset", 1 }, "Beat Offset",
        juce::NormalisableRange<float>(-5.0f, 5.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(value, 1) + " Hz"; })));

    // Carrier frequency
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "carrier_frequency", 1 }, "Carrier Frequency",
        juce::NormalisableRange<float>(40.0f, 1000.0f, 1.0f), 400.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(value, 1) + " Hz"; })));

    // Solfeggio presets
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "solfeggio_preset", "Carrier Preset",
        juce::StringArray{
            "Manual", "174Hz (UT)", "285Hz (RE)", "396Hz (MI)",
            "417Hz (FA)", "528Hz (SOL)", "639Hz (LA)",
            "741Hz (TI)", "852Hz", "963Hz"
        }, 0));

    // Waveform
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "waveform", "Waveform",
        juce::StringArray{
            "Sine", "Triangle", "Sawtooth", "Square", "Pulse",
            "Noise", "Kick", "Snare", "Hat Closed", "Hat Open"
        }, 0));

    // Modulation depth
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "modulation_depth", 1 }, "Modulation Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(static_cast<int>(value * 100.0f)) + "%"; })));

    // Noise amount
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "noise_amount", 1 }, "Noise Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(static_cast<int>(value * 100.0f)) + "%"; })));

    // Bilateral Sync parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "hemisync_correlation", 1 }, "Noise Correlation",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(static_cast<int>(value * 100.0f)) + "%"; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "hemisync_drift", 1 }, "Hemispheric Drift",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(static_cast<int>(value * 100.0f)) + "%"; })));

    // Master gain
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "master_gain", 1 }, "Master Gain",
        juce::NormalisableRange<float>(-48.0f, 6.0f, 0.1f), -12.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float value, int) { return juce::String(value, 1) + " dB"; })));

    return layout;
}

// ============================================================================
// STATE SAVE/LOAD
// ============================================================================

void BrainwaveEntrainmentAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BrainwaveEntrainmentAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// ============================================================================
// PLUGIN FACTORY
// ============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new BrainwaveEntrainmentAudioProcessor();
}