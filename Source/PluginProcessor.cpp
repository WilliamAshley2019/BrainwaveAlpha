#include "PluginProcessor.h"
#include "PluginEditor.h"

// ============================================================================
// AUDIO PROCESSOR EDITOR IMPLEMENTATION
// ============================================================================

juce::AudioProcessorEditor* BrainwaveEntrainmentFXAudioProcessor::createEditor() {
    return new BrainwaveEntrainmentFXAudioProcessorEditor(*this);
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

BrainwaveEntrainmentFXAudioProcessor::BrainwaveEntrainmentFXAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    parameters(*this, nullptr, "Parameters", createParameterLayout()) {

    // Add parameter listeners
    parameters.addParameterListener("brainwave_frequency", this);
    parameters.addParameterListener("processing_mode", this);
    parameters.addParameterListener("carrier_frequency", this);
    parameters.addParameterListener("beat_offset", this);
    parameters.addParameterListener("wet_dry_mix", this);
    parameters.addParameterListener("carrier_blend", this);
    parameters.addParameterListener("stereo_width", this);
    parameters.addParameterListener("bypass", this);
    parameters.addParameterListener("hemisync_correlation", this);
    parameters.addParameterListener("hemisync_drift", this);
}

BrainwaveEntrainmentFXAudioProcessor::~BrainwaveEntrainmentFXAudioProcessor() {
    parameters.removeParameterListener("brainwave_frequency", this);
    parameters.removeParameterListener("processing_mode", this);
    parameters.removeParameterListener("carrier_frequency", this);
    parameters.removeParameterListener("beat_offset", this);
    parameters.removeParameterListener("wet_dry_mix", this);
    parameters.removeParameterListener("carrier_blend", this);
    parameters.removeParameterListener("stereo_width", this);
    parameters.removeParameterListener("bypass", this);
    parameters.removeParameterListener("hemisync_correlation", this);
    parameters.removeParameterListener("hemisync_drift", this);
}

// ============================================================================
// AUDIO PROCESSING
// ============================================================================

void BrainwaveEntrainmentFXAudioProcessor::prepareToPlay(double sr, int samplesPerBlock) {
    juce::ignoreUnused(samplesPerBlock);

    sampleRate = sr;

    carrierOsc.setSampleRate(sr);
    envelopeFollower.setSampleRate(sr);

    // Setup smoothed values
    currentBeatHz.reset(sr, 0.05);
    carrierHz.reset(sr, 0.05);
    wetDryMix.reset(sr, 0.01);
    carrierBlend.reset(sr, 0.01);
    stereoWidth.reset(sr, 0.05);

    // Setup filters for spectral asymmetry
    leftFilter.setLowpass(sr, 2000.0f, 0.707f);
    rightFilter.setLowpass(sr, 2400.0f, 0.707f);

    // Setup crossover filters for binaural pan mode
    leftSplitLow.setLowpass(sr, 500.0f, 0.707f);
    leftSplitHigh.setHighpass(sr, 500.0f, 0.707f);
    rightSplitLow.setLowpass(sr, 500.0f, 0.707f);
    rightSplitHigh.setHighpass(sr, 500.0f, 0.707f);

    updateFrequencies();
}

void BrainwaveEntrainmentFXAudioProcessor::releaseResources() {
}

void BrainwaveEntrainmentFXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages) {
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear extra output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Bypass check
    auto bypass = parameters.getRawParameterValue("bypass")->load() > 0.5f;
    if (bypass || totalNumInputChannels < 2) {
        return; // Pass through unprocessed
    }

    processAudio(buffer);

    samplesProcessed += buffer.getNumSamples();
}

void BrainwaveEntrainmentFXAudioProcessor::processAudio(juce::AudioBuffer<float>& buffer) {
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);

    auto numSamples = buffer.getNumSamples();

    // Get parameters
    auto wetDry = parameters.getRawParameterValue("wet_dry_mix")->load();
    auto carrierMix = parameters.getRawParameterValue("carrier_blend")->load();
    auto widthParam = parameters.getRawParameterValue("stereo_width")->load();
    auto hemiDrift = parameters.getRawParameterValue("hemisync_drift")->load();
    auto sidechainDepth = parameters.getRawParameterValue("sidechain_depth")->load();
    auto modulationDepth = parameters.getRawParameterValue("modulation_depth")->load();

    correlationAmount = parameters.getRawParameterValue("hemisync_correlation")->load();

    // Create dry buffer copy for wet/dry mixing
    juce::AudioBuffer<float> dryBuffer(2, numSamples);
    dryBuffer.copyFrom(0, 0, leftChannel, numSamples);
    dryBuffer.copyFrom(1, 0, rightChannel, numSamples);

    for (int sample = 0; sample < numSamples; ++sample) {
        float beatHz = currentBeatHz.getNextValue();
        float carrier = carrierHz.getNextValue();
        float wet = wetDryMix.getNextValue();
        float carrierAmount = carrierBlend.getNextValue();
        float width = stereoWidth.getNextValue();

        float time = static_cast<float>(samplesProcessed + sample) / static_cast<float>(sampleRate);

        // Get input samples
        float inputL = leftChannel[sample];
        float inputR = rightChannel[sample];

        // Envelope following for sidechain modulation
        float inputEnv = envelopeFollower.process((std::abs(inputL) + std::abs(inputR)) * 0.5f);
        currentEnvelope = inputEnv;

        float outputL = 0.0f;
        float outputR = 0.0f;

        // ================================================================
        // PROCESSING MODES
        // ================================================================

        switch (currentMode) {
            // ============================================================
            // BINAURAL PAN - Frequency-dependent L/R separation
            // ============================================================
        case ProcessingMode::BinauralPan: {
            // Split audio into low and high bands
            float lowL = leftSplitLow.process(inputL);
            float highL = leftSplitHigh.process(inputL);
            float lowR = rightSplitLow.process(inputR);
            float highR = rightSplitHigh.process(inputR);

            // Pan modulation at brainwave frequency
            float pan = std::sin(juce::MathConstants<float>::twoPi * beatHz * time);

            // Apply frequency-dependent panning
            // Low frequencies stay centered, highs pan
            float panGainL = 0.5f * (1.0f - pan * modulationDepth);
            float panGainR = 0.5f * (1.0f + pan * modulationDepth);

            outputL = lowL + highL * panGainL + highR * (1.0f - panGainL) * 0.3f;
            outputR = lowR + highR * panGainR + highL * (1.0f - panGainR) * 0.3f;
            break;
        }

                                        // ============================================================
                                        // ISOCHRONIC GATE - Rhythmic amplitude modulation
                                        // ============================================================
        case ProcessingMode::IsochronicGate: {
            float gate = 0.5f * (1.0f + std::sin(juce::MathConstants<float>::twoPi * beatHz * time));
            gate = juce::jlimit(0.0f, 1.0f, gate * modulationDepth + (1.0f - modulationDepth));

            // Apply sidechain if enabled
            if (sidechainDepth > 0.01f) {
                gate *= (1.0f - inputEnv * sidechainDepth);
            }

            outputL = inputL * gate;
            outputR = inputR * gate;
            break;
        }

                                           // ============================================================
                                           // HEMI-SYNC - Full treatment
                                           // ============================================================
        case ProcessingMode::HemiSync: {
            // 1. Phase-locked modulation
            sharedPhase += beatHz / static_cast<float>(sampleRate);
            if (sharedPhase >= 1.0f) sharedPhase -= 1.0f;

            // 2. Hemispheric drift
            driftPhase += (0.02f * hemiDrift) / static_cast<float>(sampleRate);
            if (driftPhase >= 1.0f) driftPhase -= 1.0f;

            float drift = std::sin(driftPhase * juce::MathConstants<float>::twoPi) * 0.15f;

            // 3. Create modulation signals with drift
            float modL = std::sin((sharedPhase + drift) * juce::MathConstants<float>::twoPi);
            float modR = std::sin((sharedPhase - drift) * juce::MathConstants<float>::twoPi);

            // 4. Apply amplitude modulation
            float amDepth = modulationDepth * 0.5f;
            float gateL = 0.5f * (1.0f + modL * amDepth) + 0.5f * (1.0f - amDepth);
            float gateR = 0.5f * (1.0f + modR * amDepth) + 0.5f * (1.0f - amDepth);

            // Apply sidechain
            if (sidechainDepth > 0.01f) {
                float scMod = 1.0f - inputEnv * sidechainDepth;
                gateL *= scMod;
                gateR *= scMod;
            }

            // 5. Process through spectral asymmetry filters
            outputL = leftFilter.process(inputL * gateL);
            outputR = rightFilter.process(inputR * gateR);

            // 6. Add correlated noise for depth
            float sharedNoise = noiseGen.generatePink() * 0.02f;
            float independentNoiseL = noiseGen.generatePink() * 0.02f;
            float independentNoiseR = noiseGen.generatePink() * 0.02f;

            outputL += sharedNoise * correlationAmount + independentNoiseL * (1.0f - correlationAmount);
            outputR += sharedNoise * correlationAmount + independentNoiseR * (1.0f - correlationAmount);
            break;
        }

                                     // ============================================================
                                     // FREQUENCY SHIFT - Subtle pitch modulation
                                     // ============================================================
        case ProcessingMode::FrequencyShift: {
            // Simple vibrato effect at brainwave rate
            float vibrato = std::sin(juce::MathConstants<float>::twoPi * beatHz * time) * modulationDepth * 0.02f;

            // This is a simplified version - true frequency shifting would need phase vocoder
            // For now, we'll use a subtle tremolo + phase modulation
            float mod = 1.0f + vibrato;

            outputL = inputL * mod;
            outputR = inputR * mod;
            break;
        }

                                           // ============================================================
                                           // HYBRID - Combination of techniques
                                           // ============================================================
        case ProcessingMode::Hybrid: {
            // Combine isochronic gate + binaural pan
            float gate = 0.5f * (1.0f + std::sin(juce::MathConstants<float>::twoPi * beatHz * time));
            gate = juce::jlimit(0.0f, 1.0f, gate * modulationDepth * 0.5f + 0.5f);

            float pan = std::sin(juce::MathConstants<float>::twoPi * beatHz * time * 0.5f);
            float panGainL = 0.5f * (1.0f - pan * 0.3f);
            float panGainR = 0.5f * (1.0f + pan * 0.3f);

            outputL = inputL * gate * panGainL + inputR * gate * (1.0f - panGainL) * 0.2f;
            outputR = inputR * gate * panGainR + inputL * gate * (1.0f - panGainR) * 0.2f;
            break;
        }
        }

        // Add carrier tone if enabled
        if (carrierAmount > 0.01f) {
            float carrierSample = carrierOsc.process();
            outputL += carrierSample * carrierAmount * 0.3f;
            outputR += carrierSample * carrierAmount * 0.3f;
        }

        // Stereo width adjustment
        float mid = (outputL + outputR) * 0.5f;
        float side = (outputL - outputR) * 0.5f;

        outputL = mid + side * width;
        outputR = mid - side * width;

        // Wet/dry mix
        leftChannel[sample] = dryBuffer.getSample(0, sample) * (1.0f - wet) + outputL * wet;
        rightChannel[sample] = dryBuffer.getSample(1, sample) * (1.0f - wet) + outputR * wet;
    }
}

// ============================================================================
// PARAMETER HANDLING
// ============================================================================

void BrainwaveEntrainmentFXAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue) {
    if (parameterID == "brainwave_frequency") {
        currentFrequency = static_cast<BrainwaveFrequency>(static_cast<int>(newValue));
        updateFrequencies();
    }
    else if (parameterID == "processing_mode") {
        currentMode = static_cast<ProcessingMode>(static_cast<int>(newValue));
    }
    else if (parameterID == "carrier_frequency") {
        carrierHz.setTargetValue(newValue);
    }
    else if (parameterID == "beat_offset") {
        updateFrequencies();
    }
    else if (parameterID == "wet_dry_mix") {
        wetDryMix.setTargetValue(newValue);
    }
    else if (parameterID == "carrier_blend") {
        carrierBlend.setTargetValue(newValue);
    }
    else if (parameterID == "stereo_width") {
        stereoWidth.setTargetValue(newValue);
    }
    else if (parameterID == "bypass") {
        processingActive = newValue < 0.5f;
    }
}

void BrainwaveEntrainmentFXAudioProcessor::updateFrequencies() {
    // Get base brainwave frequency
    float baseHz = 10.0f;
    switch (currentFrequency) {
    case BrainwaveFrequency::Delta: baseHz = 2.0f; break;
    case BrainwaveFrequency::Theta: baseHz = 6.0f; break;
    case BrainwaveFrequency::Alpha: baseHz = 10.0f; break;
    case BrainwaveFrequency::Beta: baseHz = 20.0f; break;
    case BrainwaveFrequency::Gamma: baseHz = 40.0f; break;
    }

    // Add user offset
    float beatOffset = parameters.getRawParameterValue("beat_offset")->load();
    float finalBeatHz = juce::jlimit(0.5f, 100.0f, baseHz + beatOffset);

    currentBeatHz.setTargetValue(finalBeatHz);
}

// ============================================================================
// PARAMETER LAYOUT
// ============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
BrainwaveEntrainmentFXAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "processing_mode", "Processing Mode",
        juce::StringArray{ "Binaural Pan", "Isochronic Gate", "Hemi-Sync", "Frequency Shift", "Hybrid" }, 2));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "brainwave_frequency", "Brainwave Band",
        juce::StringArray{ "Delta (1-4Hz)", "Theta (4-8Hz)", "Alpha (8-13Hz)",
                          "Beta (13-30Hz)", "Gamma (30-100Hz)" }, 2));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "beat_offset", "Beat Offset",
        juce::NormalisableRange<float>(-5.0f, 5.0f, 0.1f), 0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " Hz"; }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "wet_dry_mix", "Wet/Dry Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(static_cast<int>(value * 100.0f)) + "%"; }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "modulation_depth", "Modulation Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(static_cast<int>(value * 100.0f)) + "%"; }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "carrier_frequency", "Carrier Frequency",
        juce::NormalisableRange<float>(40.0f, 500.0f, 1.0f), 100.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " Hz"; }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "carrier_blend", "Carrier Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(static_cast<int>(value * 100.0f)) + "%"; }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "stereo_width", "Stereo Width",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(static_cast<int>(value * 100.0f)) + "%"; }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "sidechain_depth", "Sidechain Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(static_cast<int>(value * 100.0f)) + "%"; }));

    // Hemi-Sync specific
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "hemisync_correlation", "Noise Correlation",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(static_cast<int>(value * 100.0f)) + "%"; }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "hemisync_drift", "Hemispheric Drift",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(static_cast<int>(value * 100.0f)) + "%"; }));

    return layout;
}

// ============================================================================
// STATE SAVE/LOAD
// ============================================================================

void BrainwaveEntrainmentFXAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BrainwaveEntrainmentFXAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// ============================================================================
// PLUGIN FACTORY
// ============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new BrainwaveEntrainmentFXAudioProcessor();
}