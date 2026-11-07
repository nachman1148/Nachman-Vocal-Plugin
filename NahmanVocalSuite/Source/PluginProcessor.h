#pragma once
#include <JuceHeader.h>
#include <array>

class NahmanVocalSuiteAudioProcessor : public juce::AudioProcessor
{
public:
    NahmanVocalSuiteAudioProcessor();
    ~NahmanVocalSuiteAudioProcessor() override = default;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override; bool producesMidi() const override; bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override; int getCurrentProgram() override; void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override; void changeProgramName(int, const juce::String&) override;
    void getStateInformation(juce::MemoryBlock&) override; void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState parameters;
    void applyMagicMix();
    void analyzeVocal(bool applySuggestedSettings);
    juce::String getAnalysisSummary() const { return analysisSummary; }
    juce::String getDetectedStyle() const { return detectedStyle; }
    juce::String getDetectedRegister() const { return detectedRegister; }
    juce::String getRecordingAssessment() const { return recordingAssessment; }
    float getInputRms() const { return recentRms.load(); }
    float getInputPeak() const { return recentPeak.load(); }
    float getOutputRms() const { return outputRms.load(); }
    float getOutputPeak() const { return outputPeak.load(); }
    float getWaveformSample(int index) const { return waveform[(size_t)juce::jlimit(0, 127, index)].load(); }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
private:
    using Filter = juce::dsp::IIR::Filter<float>;
    Filter hpFilters[2], lowFilters[2], midFilters[2], highFilters[2];
    juce::dsp::Compressor<float> compressor[2];
    juce::dsp::Reverb reverb;
    std::array<float, 2> deEsserHighpassState {};
    std::array<float, 2> deEsserEnvelope {};
    std::array<float, 2> gateEnvelope {};
    std::array<std::array<float, 8192>, 2> pitchDelayBuffer {};
    std::array<int, 2> pitchDelayWrite {};
    std::array<float, 2> pitchPhase {};
    void processPitchCorrection(int channel, float* samples, int numSamples, float ratio);
    std::array<float, 2048> pitchHistory {};
    int pitchWritePosition = 0;
    int pitchHistorySamples = 0;
    int pitchAnalysisCounter = 0;
    std::atomic<float> detectedPitchHz { 0.0f };
    float smoothedPitchFactor = 1.0f;
    float estimatePitchHz() const;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine[2];
    void updateFilters();
    std::atomic<float> recentRms { 0.0f }, recentPeak { 0.0f }, recentCrest { 0.0f }, recentZcr { 0.0f };
    std::atomic<float> outputRms { 0.0f }, outputPeak { 0.0f };
    std::array<std::atomic<float>, 128> waveform {};
    juce::AudioBuffer<float> dryBuffer;
    juce::String detectedStyle { "Not analyzed" }, detectedRegister { "Unknown register" }, recordingAssessment { "Waiting for audio" }, analysisSummary { "Play or record vocal, then press ANALYZE" };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NahmanVocalSuiteAudioProcessor)
};
