#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class NahmanVocalSuiteAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NahmanVocalSuiteAudioProcessorEditor(NahmanVocalSuiteAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
private:
    NahmanVocalSuiteAudioProcessor& processor;
    juce::TextButton magic, aiDetect, standardMode, analyzeOnly, applySuggestion, abCompare;
    juce::ToggleButton autoTune;
    juce::ComboBox tuneKey, tuneScale;
    juce::Slider tuneSpeed, tuneAmount;
    juce::Label tuneKeyLabel, tuneScaleLabel, tuneSpeedLabel, tuneAmountLabel;
    juce::Label status, analysisResult, modeStatus;
    bool useAiMode = true;
    bool showingProcessed = true;

    juce::Slider input,hpf,low,mid,high,threshold,ratio,saturation,reverb,delay,output;
    juce::ToggleButton gate,eq,comp,sat,rev,del;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> toggles;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> abAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tuneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment, scaleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tuneSpeedAttachment, tuneAmountAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NahmanVocalSuiteAudioProcessorEditor)
};
