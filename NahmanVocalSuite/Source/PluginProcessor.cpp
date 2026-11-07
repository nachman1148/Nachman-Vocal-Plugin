#include "PluginProcessor.h"
#include "PluginEditor.h"

NahmanVocalSuiteAudioProcessor::NahmanVocalSuiteAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
#else
    : AudioProcessor(),
#endif
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (int ch = 0; ch < 2; ++ch) {
        hpFilters[ch].reset(); lowFilters[ch].reset(); midFilters[ch].reset(); highFilters[ch].reset();
        compressor[ch].reset();
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout NahmanVocalSuiteAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto addFloat = [&p](const char* id, const char* name, NormalisableRange<float> range, float def) {
        p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id, 1}, name, range, def));
    };
    auto addBool = [&p](const char* id, const char* name, bool def) {
        p.push_back(std::make_unique<AudioParameterBool>(ParameterID{id, 1}, name, def));
    };
    addBool("gateOn", "Gate", false);
    addBool("eqOn", "EQ", true);
    addBool("compOn", "Compressor", true);
    addBool("satOn", "Saturation", false);
    addBool("reverbOn", "Reverb", false);
    addBool("delayOn", "Delay", false);
    addBool("abBypass", "A-B Bypass", false);
    addBool("autoTuneOn", "Auto Tune", false);
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"tuneKey", 1}, "Tune Key", StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"tuneScale", 1}, "Tune Scale", StringArray{"Chromatic", "Major", "Minor"}, 2));
    addFloat("tuneSpeed", "Retune Speed", {1.0f, 100.0f, 1.0f}, 35.0f);
    addFloat("tuneAmount", "Correction Amount", {0.0f, 100.0f, 1.0f}, 100.0f);
    addFloat("input", "Input", {-24.0f, 24.0f, 0.1f}, 0.0f);
    addFloat("hpf", "High Pass", {40.0f, 250.0f, 1.0f}, 90.0f);
    addFloat("low", "Low", {-12.0f, 12.0f, 0.1f}, 0.0f);
    addFloat("mid", "Mid", {-12.0f, 12.0f, 0.1f}, 0.0f);
    addFloat("high", "High", {-12.0f, 12.0f, 0.1f}, 0.0f);
    addFloat("threshold", "Threshold", {-48.0f, 0.0f, 0.1f}, -18.0f);
    addFloat("ratio", "Ratio", {1.0f, 10.0f, 0.1f}, 3.0f);
    addFloat("attack", "Attack", {1.0f, 100.0f, 0.1f}, 15.0f);
    addFloat("release", "Release", {20.0f, 500.0f, 1.0f}, 120.0f);
    addFloat("saturation", "Saturation", {0.0f, 100.0f, 1.0f}, 12.0f);
    addFloat("reverb", "Reverb Mix", {0.0f, 100.0f, 1.0f}, 12.0f);
    addFloat("delay", "Delay Mix", {0.0f, 100.0f, 1.0f}, 10.0f);
    addFloat("output", "Output", {-24.0f, 12.0f, 0.1f}, 0.0f);
    return {p.begin(), p.end()};
}

void NahmanVocalSuiteAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumOutputChannels()};
    dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);
    for (auto& channel : pitchDelayBuffer) channel.fill(0.0f);
    pitchDelayWrite.fill(0); pitchPhase.fill(0.0f);
    pitchHistory.fill(0.0f); pitchWritePosition = 0; pitchHistorySamples = 0; pitchAnalysisCounter = 0;
    detectedPitchHz.store(0.0f); smoothedPitchFactor = 1.0f;
    reverb.prepare(spec);
    juce::dsp::Reverb::Parameters rp;
    rp.roomSize = 0.55f; rp.damping = 0.45f; rp.wetLevel = 0.25f; rp.dryLevel = 0.75f;
    reverb.setParameters(rp);
    for (int ch = 0; ch < 2; ++ch) {
        compressor[ch].prepare(spec);
        compressor[ch].setAttack(15.0f); compressor[ch].setRelease(120.0f);
        compressor[ch].setRatio(3.0f); compressor[ch].setThreshold(-18.0f);
        delayLine[ch].prepare({sampleRate, (juce::uint32)samplesPerBlock, 1});
        delayLine[ch].setMaximumDelayInSamples((int)(sampleRate * 0.75));
        delayLine[ch].setDelay((float)(sampleRate * 0.25));
    }
    updateFilters();
}

void NahmanVocalSuiteAudioProcessor::releaseResources() {}

bool NahmanVocalSuiteAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void NahmanVocalSuiteAudioProcessor::updateFilters()
{
    const auto sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    const auto hpf = parameters.getRawParameterValue("hpf")->load();
    const auto low = parameters.getRawParameterValue("low")->load();
    const auto mid = parameters.getRawParameterValue("mid")->load();
    const auto high = parameters.getRawParameterValue("high")->load();
    for (int ch = 0; ch < 2; ++ch) {
        hpFilters[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, hpf);
        lowFilters[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 160.0f, 0.707f, juce::Decibels::decibelsToGain(low));
        midFilters[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, 1200.0f, 0.9f, juce::Decibels::decibelsToGain(mid));
        highFilters[ch].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, 8500.0f, 0.707f, juce::Decibels::decibelsToGain(high));
    }
}

void NahmanVocalSuiteAudioProcessor::processPitchCorrection(int channel, float* samples, int numSamples, float ratio)
{
    // Two crossfaded delay taps provide a lightweight real-time pitch shifter.
    // It is intended for modest monophonic vocal correction, not polyphonic material.
    auto& ring = pitchDelayBuffer[(size_t)channel];
    int& write = pitchDelayWrite[(size_t)channel];
    float& phase = pitchPhase[(size_t)channel];
    const float depth = juce::jlimit(256.0f, 3000.0f, (float)getSampleRate() * 0.035f);
    const float minDelay = 96.0f;
    const float direction = ratio > 1.0f ? -1.0f : 1.0f;
    const float phaseStep = juce::jlimit(0.0f, 0.0015f, std::abs(1.0f - ratio) / depth);
    auto readAtDelay = [&ring, &write](float delay) {
        const float readPosition = (float)write - delay;
        const int i0 = ((int)std::floor(readPosition) + (int)ring.size() * 2) % (int)ring.size();
        const int i1 = (i0 + 1) % (int)ring.size();
        const float frac = readPosition - std::floor(readPosition);
        return ring[(size_t)i0] + (ring[(size_t)i1] - ring[(size_t)i0]) * frac;
    };
    for (int i = 0; i < numSamples; ++i) {
        const float input = samples[i];
        ring[(size_t)write] = input;
        const float pA = phase;
        float pB = pA + 0.5f; if (pB >= 1.0f) pB -= 1.0f;
        const float wA = 0.5f + 0.5f * std::cos(juce::MathConstants<float>::twoPi * pA);
        const float wB = 1.0f - wA;
        const float a = readAtDelay(minDelay + pA * depth);
        const float b = readAtDelay(minDelay + pB * depth);
        samples[i] = a * wA + b * wB;
        phase += direction * phaseStep;
        while (phase < 0.0f) phase += 1.0f;
        while (phase >= 1.0f) phase -= 1.0f;
        write = (write + 1) % (int)ring.size();
    }
}

float NahmanVocalSuiteAudioProcessor::estimatePitchHz() const
{
    const double sr = getSampleRate();
    if (sr <= 0.0) return 0.0f;
    constexpr int N = 2048;
    const int minLag = juce::jmax(2, (int)(sr / 1000.0));
    const int maxLag = juce::jmin(N / 2, (int)(sr / 70.0));
    float bestCorrelation = 0.0f;
    int bestLag = 0;
    // The history is read oldest-to-newest from the circular buffer.
    for (int lag = minLag; lag <= maxLag; ++lag) {
        double xy = 0.0, xx = 0.0, yy = 0.0;
        for (int i = 0; i < N - lag; i += 2) {
            const float a = pitchHistory[(size_t)((pitchWritePosition + i) % N)];
            const float b = pitchHistory[(size_t)((pitchWritePosition + i + lag) % N)];
            xy += (double)a * b; xx += (double)a * a; yy += (double)b * b;
        }
        const float corr = (xx > 1.0e-9 && yy > 1.0e-9) ? (float)(xy / std::sqrt(xx * yy)) : 0.0f;
        if (corr > bestCorrelation) { bestCorrelation = corr; bestLag = lag; }
    }
    return (bestCorrelation > 0.72f && bestLag > 0) ? (float)(sr / bestLag) : 0.0f;
}

void NahmanVocalSuiteAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels = juce::jmin(2, buffer.getNumChannels());
    const float inputGain = juce::Decibels::decibelsToGain(parameters.getRawParameterValue("input")->load());
    const float outputGain = juce::Decibels::decibelsToGain(parameters.getRawParameterValue("output")->load());
    const bool gateOn = parameters.getRawParameterValue("gateOn")->load() > 0.5f;
    const bool eqOn = parameters.getRawParameterValue("eqOn")->load() > 0.5f;
    const bool compOn = parameters.getRawParameterValue("compOn")->load() > 0.5f;
    const bool satOn = parameters.getRawParameterValue("satOn")->load() > 0.5f;
    const bool revOn = parameters.getRawParameterValue("reverbOn")->load() > 0.5f;
    const bool delOn = parameters.getRawParameterValue("delayOn")->load() > 0.5f;
    const bool abBypass = parameters.getRawParameterValue("abBypass")->load() > 0.5f;
    const float threshold = parameters.getRawParameterValue("threshold")->load();
    const float ratio = parameters.getRawParameterValue("ratio")->load();
    const float sat = parameters.getRawParameterValue("saturation")->load() / 100.0f;
    const float revMix = parameters.getRawParameterValue("reverb")->load() / 100.0f;
    const float delMix = parameters.getRawParameterValue("delay")->load() / 100.0f;
    updateFilters();

    // Keep an untouched copy for instant A/B (processed vs. dry) comparison.
    for (int ch = 0; ch < channels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, juce::jmin(buffer.getNumSamples(), dryBuffer.getNumSamples()));

    // Lightweight input analysis. This is a DSP heuristic, not a trained genre-recognition model.
    double sumSquares = 0.0;
    float peak = 0.0f;
    int crossings = 0;
    float previous = 0.0f;
    int measured = 0;
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        float x = 0.0f;
        for (int ch = 0; ch < channels; ++ch) x += buffer.getReadPointer(ch)[i];
        x /= (float) juce::jmax(1, channels);
        sumSquares += (double)x * (double)x;
        peak = juce::jmax(peak, std::abs(x));
        if (i > 0 && ((x >= 0.0f) != (previous >= 0.0f))) ++crossings;
        previous = x;
        ++measured;
    }
    const float rms = measured > 0 ? (float)std::sqrt(sumSquares / measured) : 0.0f;
    recentRms.store(rms); recentPeak.store(peak);
    // Snapshot 128 evenly spaced points for the live waveform display.
    for (int point = 0; point < 128; ++point) {
        const int sampleIndex = measured > 1 ? (point * (measured - 1)) / 127 : 0;
        float sample = 0.0f;
        for (int ch = 0; ch < channels; ++ch) sample += buffer.getReadPointer(ch)[sampleIndex];
        waveform[(size_t)point].store(sample / (float)juce::jmax(1, channels));
    }
    // Keep a rolling mono history for monophonic vocal pitch tracking.
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        float mono = 0.0f;
        for (int ch = 0; ch < channels; ++ch) mono += buffer.getReadPointer(ch)[i];
        pitchHistory[(size_t)pitchWritePosition] = mono / (float)juce::jmax(1, channels);
        pitchWritePosition = (pitchWritePosition + 1) % (int)pitchHistory.size();
        pitchHistorySamples = juce::jmin((int)pitchHistory.size(), pitchHistorySamples + 1);
    }
    pitchAnalysisCounter += buffer.getNumSamples();
    if (pitchAnalysisCounter >= 512) {
        if (pitchHistorySamples >= (int)pitchHistory.size()) detectedPitchHz.store(estimatePitchHz());
        pitchAnalysisCounter %= 512;
    }
    recentCrest.store(rms > 1.0e-6f ? peak / rms : 0.0f);
    recentZcr.store(measured > 1 ? (float)crossings / (float)measured : 0.0f);

    const bool tuneOn = parameters.getRawParameterValue("autoTuneOn")->load() > 0.5f;
    if (tuneOn) {
        const float pitch = detectedPitchHz.load();
        if (pitch > 65.0f && pitch < 1050.0f) {
            const int key = (int)parameters.getRawParameterValue("tuneKey")->load();
            const int scale = (int)parameters.getRawParameterValue("tuneScale")->load();
            const float amount = parameters.getRawParameterValue("tuneAmount")->load() / 100.0f;
            const float speedMs = parameters.getRawParameterValue("tuneSpeed")->load();
            const float midi = 69.0f + 12.0f * std::log2(pitch / 440.0f);
            const int nearest = (int)std::round(midi);
            const int major[] = {0, 2, 4, 5, 7, 9, 11};
            const int minor[] = {0, 2, 3, 5, 7, 8, 10};
            int target = nearest;
            float bestDistance = 100.0f;
            for (int candidate = nearest - 6; candidate <= nearest + 6; ++candidate) {
                int pc = (candidate - key) % 12; if (pc < 0) pc += 12;
                bool allowed = (scale == 0);
                if (scale == 1) for (int n : major) allowed = allowed || pc == n;
                if (scale == 2) for (int n : minor) allowed = allowed || pc == n;
                const float distance = std::abs(midi - (float)candidate);
                if (allowed && distance < bestDistance) { bestDistance = distance; target = candidate; }
            }
            const float desired = std::pow(2.0f, ((float)target - midi) * amount / 12.0f);
            const float smoothing = 1.0f - std::exp(-(float)buffer.getNumSamples() / (juce::jmax(1.0f, speedMs) * 0.001f * (float)getSampleRate()));
            smoothedPitchFactor += (desired - smoothedPitchFactor) * smoothing;
        }
    } else { smoothedPitchFactor = 1.0f; }

    for (int ch = 0; ch < channels; ++ch) {
        auto* data = buffer.getWritePointer(ch);
        if (tuneOn && detectedPitchHz.load() > 65.0f && detectedPitchHz.load() < 1050.0f) {
            processPitchCorrection(ch, data, buffer.getNumSamples(), smoothedPitchFactor);
        }
        juce::dsp::AudioBlock<float> block(buffer);
        auto channelBlock = block.getSingleChannelBlock((size_t)ch);
        juce::dsp::ProcessContextReplacing<float> context(channelBlock);
        for (int i = 0; i < buffer.getNumSamples(); ++i) data[i] *= inputGain;
        if (eqOn) {
            hpFilters[ch].process(context); lowFilters[ch].process(context);
            midFilters[ch].process(context); highFilters[ch].process(context);
        }
        if (gateOn) {
            const float gateLevel = juce::Decibels::decibelsToGain(-48.0f);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (std::abs(data[i]) < gateLevel) data[i] *= 0.15f;
        }
        if (compOn) {
            compressor[ch].setThreshold(threshold); compressor[ch].setRatio(ratio);
            compressor[ch].setAttack(parameters.getRawParameterValue("attack")->load());
            compressor[ch].setRelease(parameters.getRawParameterValue("release")->load());
            compressor[ch].process(context);
        }
        if (satOn && sat > 0.0f)
            for (int i = 0; i < buffer.getNumSamples(); ++i) data[i] = std::tanh(data[i] * (1.0f + sat * 5.0f));
        if (delOn && delMix > 0.0f)
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                const float dry = data[i]; const float wet = delayLine[ch].popSample(0);
                delayLine[ch].pushSample(0, dry); data[i] = dry * (1.0f - delMix) + wet * delMix;
            }
    }
    if (revOn && revMix > 0.0f) {
        auto rp = reverb.getParameters();
        rp.wetLevel = revMix;
        rp.dryLevel = 1.0f - revMix;
        reverb.setParameters(rp);
        juce::dsp::AudioBlock<float> reverbBlock(buffer);
        juce::dsp::ProcessContextReplacing<float> reverbContext(reverbBlock);
        reverb.process(reverbContext);
    }
    buffer.applyGain(outputGain);
    if (abBypass) {
        for (int ch = 0; ch < channels; ++ch)
            buffer.copyFrom(ch, 0, dryBuffer, ch, 0, juce::jmin(buffer.getNumSamples(), dryBuffer.getNumSamples()));
    }
    double outSumSquares = 0.0;
    float outPeak = 0.0f;
    int outSamples = 0;
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        float sample = 0.0f;
        for (int ch = 0; ch < channels; ++ch) {
            const float value = buffer.getReadPointer(ch)[i];
            sample += value;
            outPeak = juce::jmax(outPeak, std::abs(value));
        }
        sample /= (float)juce::jmax(1, channels);
        outSumSquares += (double)sample * (double)sample;
        ++outSamples;
    }
    outputRms.store(outSamples > 0 ? (float)std::sqrt(outSumSquares / outSamples) : 0.0f);
    outputPeak.store(outPeak);
}

juce::AudioProcessorEditor* NahmanVocalSuiteAudioProcessor::createEditor() { return new NahmanVocalSuiteAudioProcessorEditor(*this); }
bool NahmanVocalSuiteAudioProcessor::hasEditor() const { return true; }
const juce::String NahmanVocalSuiteAudioProcessor::getName() const { return JucePlugin_Name; }
bool NahmanVocalSuiteAudioProcessor::acceptsMidi() const { return false; }
bool NahmanVocalSuiteAudioProcessor::producesMidi() const { return false; }
bool NahmanVocalSuiteAudioProcessor::isMidiEffect() const { return false; }
double NahmanVocalSuiteAudioProcessor::getTailLengthSeconds() const { return 2.0; }
int NahmanVocalSuiteAudioProcessor::getNumPrograms() { return 1; }
int NahmanVocalSuiteAudioProcessor::getCurrentProgram() { return 0; }
void NahmanVocalSuiteAudioProcessor::setCurrentProgram(int) {}
const juce::String NahmanVocalSuiteAudioProcessor::getProgramName(int) { return {}; }
void NahmanVocalSuiteAudioProcessor::changeProgramName(int, const juce::String&) {}
void NahmanVocalSuiteAudioProcessor::getStateInformation(juce::MemoryBlock& dest) { auto state = parameters.copyState(); std::unique_ptr<juce::XmlElement> xml(state.createXml()); copyXmlToBinary(*xml, dest); }
void NahmanVocalSuiteAudioProcessor::setStateInformation(const void* data, int size) { std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size)); if (xml && xml->hasTagName(parameters.state.getType())) parameters.replaceState(juce::ValueTree::fromXml(*xml)); }

void NahmanVocalSuiteAudioProcessor::analyzeVocal(bool applySuggestedSettings)
{
    const float rms = recentRms.load();
    const float peak = recentPeak.load();
    const float crest = recentCrest.load();
    const float zcr = recentZcr.load();
    if (rms < 0.0005f) {
        analysisSummary = "לא זוהה אות ווקאל ברור. נגן את ההקלטה ונסה שוב.  ·  No clear vocal signal.";
        detectedStyle = "אות חלש / INSUFFICIENT SIGNAL";
        detectedRegister = "לא נמדד / NOT MEASURED";
        recordingAssessment = "עוצמת כניסה נמוכה או שקט / LOW INPUT";
        return;
    }

    // Conservative signal-shape classification. Genre cannot be reliably inferred from
    // an isolated vocal using these measurements alone; this gives a starting profile.
    if (crest > 5.5f) detectedStyle = "ווקאל דינמי / DYNAMIC";
    else if (zcr > 0.105f && crest > 3.0f) detectedStyle = "הגשה קצבית / RHYTHMIC · RAP-LIKE";
    else if (crest < 2.7f) detectedStyle = "ווקאל מלודי / SUSTAINED · MELODIC";
    else detectedStyle = "דיבורי / טבעי · SPEECH-LIKE";

    // Register is deliberately described as signal brightness proxy, not gender detection.
    if (zcr < 0.035f) detectedRegister = "נטייה נמוכה וחמה / LOW · WARM";
    else if (zcr > 0.095f) detectedRegister = "נטייה בהירה וגבוהה / BRIGHT · HIGH";
    else detectedRegister = "נטייה לתחום האמצע / MID-RANGE";

    if (peak > 0.98f) recordingAssessment = "סכנת קליפינג – הנמך כניסה / CLIPPING RISK";
    else if (rms < 0.015f) recordingAssessment = "הקלטה חלשה – בדוק גיין ורעשים / LOW LEVEL";
    else if (crest > 7.0f) recordingAssessment = "דינמיקה גבוהה – קומפרסיה עשויה לעזור / HIGH DYNAMICS";
    else recordingAssessment = "העוצמה נראית תקינה – בדוק רעשי חדר ועיוות / USABLE LEVEL";

    analysisSummary = detectedStyle + "  |  " + detectedRegister + "  |  " + recordingAssessment;

    if (applySuggestedSettings) {
        auto set = [this](const char* id, float value) {
            if (auto* parameter = parameters.getParameter(id))
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        };
        // These are conservative starting points, not a finished mix or a genre guarantee.
        set("eqOn", 1.0f); set("compOn", 1.0f);
        set("hpf", detectedStyle.contains("Rap") ? 95.0f : 85.0f);
        set("low", -1.0f); set("mid", detectedStyle.contains("Speech") ? -0.5f : -1.5f);
        set("high", detectedStyle.contains("Dynamic") ? 1.0f : 2.0f);
        set("threshold", crest > 5.5f ? -22.0f : -19.0f);
        set("ratio", detectedStyle.contains("Dynamic") ? 4.0f : 3.0f);
        set("attack", 18.0f); set("release", 115.0f);
    }
}

void NahmanVocalSuiteAudioProcessor::applyMagicMix()
{
    // Conservative starting point for a typical close-miked vocal; always fine-tune by ear.
    auto set = [this](const char* id, float value) { if (auto* p = parameters.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(value)); };
    set("eqOn", 1.0f); set("compOn", 1.0f); set("hpf", 90.0f);
    set("low", -1.5f); set("mid", -1.0f); set("high", 2.0f);
    set("threshold", -20.0f); set("ratio", 3.0f); set("attack", 18.0f); set("release", 110.0f);
    set("satOn", 1.0f); set("saturation", 8.0f); set("reverbOn", 1.0f); set("reverb", 9.0f);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NahmanVocalSuiteAudioProcessor(); }
