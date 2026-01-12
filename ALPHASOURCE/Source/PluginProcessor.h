#pragma once
#include <JuceHeader.h>
#include <random>
#include <vector>
#include <cmath>

// ============================================================================
// SHARED DSP CLASSES (from synth version)
// ============================================================================

enum class BrainwaveFrequency {
    Delta = 0,    // 1-4 Hz
    Theta = 1,    // 4-8 Hz
    Alpha = 2,    // 8-13 Hz
    Beta = 3,     // 13-30 Hz
    Gamma = 4     // 30-100 Hz
};

enum class ProcessingMode {
    BinauralPan = 0,      // Frequency-dependent L/R separation
    IsochronicGate = 1,   // Rhythmic amplitude modulation
    HemiSync = 2,         // Full Hemi-Sync treatment
    FrequencyShift = 3,   // Subtle pitch modulation
    Hybrid = 4            // Combination
};

enum class Waveform {
    Sine = 0,
    Triangle,
    Sawtooth,
    Square,
    Pulse,
    Noise
};

// ============================================================================
// OSCILLATOR (for carrier generation)
// ============================================================================

class BrainwaveOscillator {
public:
    BrainwaveOscillator()
        : randomGenerator(std::random_device{}()),
        randomDistribution(-1.0f, 1.0f) {
    }

    void setSampleRate(double sr) {
        sampleRate = sr;
        updateIncrement();
    }

    void setFrequency(float freq) {
        frequency = freq;
        updateIncrement();
    }

    void setWaveform(Waveform wave) {
        currentWaveform = wave;
    }

    void setPhase(float ph) {
        phase = ph;
    }

    void reset() {
        phase = 0.0f;
    }

    float process() {
        float sample = 0.0f;

        switch (currentWaveform) {
        case Waveform::Sine:
            sample = std::sin(phase * juce::MathConstants<float>::twoPi);
            break;
        case Waveform::Triangle:
            sample = 2.0f * std::abs(2.0f * (phase - 0.5f)) - 1.0f;
            break;
        case Waveform::Sawtooth:
            sample = 2.0f * phase - 1.0f;
            break;
        case Waveform::Square:
            sample = phase < 0.5f ? 1.0f : -1.0f;
            break;
        case Waveform::Pulse:
            sample = (phase < 0.25f) ? 1.0f : -1.0f;
            break;
        case Waveform::Noise:
            sample = randomDistribution(randomGenerator);
            break;
        default:
            sample = std::sin(phase * juce::MathConstants<float>::twoPi);
        }

        phase += phaseIncrement;
        if (phase >= 1.0f) phase -= 1.0f;

        return sample;
    }

private:
    Waveform currentWaveform = Waveform::Sine;
    double sampleRate = 44100.0;
    float frequency = 440.0f;
    float phase = 0.0f;
    float phaseIncrement = 0.0f;

    std::mt19937 randomGenerator;
    std::uniform_real_distribution<float> randomDistribution;

    void updateIncrement() {
        phaseIncrement = frequency / static_cast<float>(sampleRate);
    }
};

// ============================================================================
// NOISE GENERATOR
// ============================================================================

class NoiseGenerator {
public:
    NoiseGenerator()
        : randomEngine(std::random_device{}()),
        distribution(-1.0f, 1.0f) {
        for (int i = 0; i < 7; ++i)
            pinkState[i] = 0.0f;
    }

    float generateWhite() {
        return distribution(randomEngine);
    }

    float generatePink() {
        float white = distribution(randomEngine);

        pinkState[0] = 0.99886f * pinkState[0] + white * 0.0555179f;
        pinkState[1] = 0.99332f * pinkState[1] + white * 0.0750759f;
        pinkState[2] = 0.96900f * pinkState[2] + white * 0.1538520f;
        pinkState[3] = 0.86650f * pinkState[3] + white * 0.3104856f;
        pinkState[4] = 0.55000f * pinkState[4] + white * 0.5329522f;
        pinkState[5] = -0.7616f * pinkState[5] - white * 0.0168980f;

        float pink = pinkState[0] + pinkState[1] + pinkState[2] +
            pinkState[3] + pinkState[4] + pinkState[5] +
            pinkState[6] + white * 0.5362f;
        pinkState[6] = white * 0.115926f;

        return pink * 0.11f;
    }

private:
    std::mt19937 randomEngine;
    std::uniform_real_distribution<float> distribution;
    float pinkState[7];
};

// ============================================================================
// BIQUAD FILTER
// ============================================================================

class SimpleBiquad {
public:
    void setLowpass(double sampleRate, float cutoffHz, float Q = 0.707f) {
        float w0 = juce::MathConstants<float>::twoPi * cutoffHz / static_cast<float>(sampleRate);
        float cosw0 = std::cos(w0);
        float sinw0 = std::sin(w0);
        float alpha = sinw0 / (2.0f * Q);

        float b0_temp = (1.0f - cosw0) / 2.0f;
        float b1_temp = 1.0f - cosw0;
        float b2_temp = (1.0f - cosw0) / 2.0f;
        float a0_temp = 1.0f + alpha;
        float a1_temp = -2.0f * cosw0;
        float a2_temp = 1.0f - alpha;

        b0 = b0_temp / a0_temp;
        b1 = b1_temp / a0_temp;
        b2 = b2_temp / a0_temp;
        a1 = a1_temp / a0_temp;
        a2 = a2_temp / a0_temp;
    }

    void setHighpass(double sampleRate, float cutoffHz, float Q = 0.707f) {
        float w0 = juce::MathConstants<float>::twoPi * cutoffHz / static_cast<float>(sampleRate);
        float cosw0 = std::cos(w0);
        float sinw0 = std::sin(w0);
        float alpha = sinw0 / (2.0f * Q);

        float b0_temp = (1.0f + cosw0) / 2.0f;
        float b1_temp = -(1.0f + cosw0);
        float b2_temp = (1.0f + cosw0) / 2.0f;
        float a0_temp = 1.0f + alpha;
        float a1_temp = -2.0f * cosw0;
        float a2_temp = 1.0f - alpha;

        b0 = b0_temp / a0_temp;
        b1 = b1_temp / a0_temp;
        b2 = b2_temp / a0_temp;
        a1 = a1_temp / a0_temp;
        a2 = a2_temp / a0_temp;
    }

    float process(float input) {
        float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

        x2 = x1;
        x1 = input;
        y2 = y1;
        y1 = output;

        return output;
    }

    void reset() {
        x1 = x2 = y1 = y2 = 0.0f;
    }

private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
};

// ============================================================================
// ENVELOPE FOLLOWER (for sidechain)
// ============================================================================

class EnvelopeFollower {
public:
    void setSampleRate(double sr) {
        sampleRate = sr;
        setAttackTime(10.0f);
        setReleaseTime(100.0f);
    }

    void setAttackTime(float ms) {
        attackCoeff = std::exp(-1.0f / (static_cast<float>(sampleRate) * ms * 0.001f));
    }

    void setReleaseTime(float ms) {
        releaseCoeff = std::exp(-1.0f / (static_cast<float>(sampleRate) * ms * 0.001f));
    }

    float process(float input) {
        float inputAbs = std::abs(input);

        if (inputAbs > envelope) {
            envelope = attackCoeff * (envelope - inputAbs) + inputAbs;
        }
        else {
            envelope = releaseCoeff * (envelope - inputAbs) + inputAbs;
        }

        return envelope;
    }

    void reset() {
        envelope = 0.0f;
    }

private:
    double sampleRate = 44100.0;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float envelope = 0.0f;
};

// ============================================================================
// MAIN PROCESSOR (EFFECT)
// ============================================================================

class BrainwaveEntrainmentFXAudioProcessor : public juce::AudioProcessor,
    public juce::AudioProcessorValueTreeState::Listener {
public:
    BrainwaveEntrainmentFXAudioProcessor();
    ~BrainwaveEntrainmentFXAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return parameters; }

    bool isActive() const { return processingActive; }
    float getCurrentBeatFrequency() const { return currentBeatHz.getCurrentValue(); }
    float getCurrentEnvelope() const { return currentEnvelope; }

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void updateFrequencies();
    void processAudio(juce::AudioBuffer<float>& buffer);

    // DSP Components
    BrainwaveOscillator carrierOsc;
    NoiseGenerator noiseGen;
    SimpleBiquad leftFilter;
    SimpleBiquad rightFilter;
    SimpleBiquad leftSplitLow;
    SimpleBiquad leftSplitHigh;
    SimpleBiquad rightSplitLow;
    SimpleBiquad rightSplitHigh;
    EnvelopeFollower envelopeFollower;

    // Parameters
    juce::AudioProcessorValueTreeState parameters;

    // State
    double sampleRate = 44100.0;
    bool processingActive = true;

    juce::int64 samplesProcessed = 0;

    // Smoothed values
    juce::SmoothedValue<float> currentBeatHz{ 10.0f };
    juce::SmoothedValue<float> carrierHz{ 100.0f };
    juce::SmoothedValue<float> wetDryMix{ 0.5f };
    juce::SmoothedValue<float> carrierBlend{ 0.0f };
    juce::SmoothedValue<float> stereoWidth{ 1.0f };

    // Hemi-Sync state
    float sharedPhase = 0.0f;
    float driftPhase = 0.0f;
    float correlationAmount = 0.7f;
    float currentEnvelope = 0.0f;

    // Current settings
    ProcessingMode currentMode = ProcessingMode::HemiSync;
    BrainwaveFrequency currentFrequency = BrainwaveFrequency::Alpha;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrainwaveEntrainmentFXAudioProcessor)
};