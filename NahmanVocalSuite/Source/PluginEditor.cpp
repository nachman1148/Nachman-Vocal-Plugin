#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::Colour background { 0xff080d17 };
    const juce::Colour surface { 0xff111c2b };
    const juce::Colour surface2 { 0xff17283a };
    const juce::Colour cyan { 0xff55e6e6 };
    const juce::Colour gold { 0xffd7b978 };
    const juce::Colour muted { 0xff9aafc3 };

    void styleButton(juce::TextButton& b, const juce::String& text, juce::Colour colour = surface2)
    {
        b.setButtonText(text);
        b.setColour(juce::TextButton::buttonColourId, colour);
        b.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        b.setColour(juce::TextButton::textColourOnId, cyan);
        b.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff214454));
        b.setClickingTogglesState(false);
    }
}

NahmanVocalSuiteAudioProcessorEditor::NahmanVocalSuiteAudioProcessorEditor(NahmanVocalSuiteAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(1180, 800);
    startTimerHz(30);

    auto addKnob = [this](juce::Slider& s, const juce::String& name, const char* id) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 66, 20);
        s.setColour(juce::Slider::rotarySliderFillColourId, cyan);
        s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff34495d));
        s.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0c1521));
        addAndMakeVisible(s);

        labels.emplace_back(std::make_unique<juce::Label>());
        auto& l = *labels.back();
        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, muted);
        l.setFont(juce::Font(12.0f, juce::Font::bold));
        addAndMakeVisible(l);
        attachments.emplace_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, id, s));
    };

    addKnob(input,      "כניסה · INPUT",       "input");
    addKnob(hpf,        "חיתוך נמוכים · HPF",  "hpf");
    addKnob(low,        "נמוכים · LOW",        "low");
    addKnob(mid,        "אמצע · MID",          "mid");
    addKnob(high,       "אוויר · AIR",         "high");
    addKnob(threshold,  "סף · THRESHOLD",      "threshold");
    addKnob(ratio,      "יחס · RATIO",         "ratio");
    addKnob(saturation, "חום · SATURATION",    "saturation");
    addKnob(reverb,     "חלל · REVERB",        "reverb");
    addKnob(delay,      "הד · DELAY",          "delay");
    addKnob(output,     "יציאה · OUTPUT",      "output");

    styleButton(magic, "✦  מיקס קסם  ·  MAGIC MIX", juce::Colour(0xff17445b));
    magic.setColour(juce::TextButton::textColourOffId, cyan);
    magic.onClick = [this] { processor.applyMagicMix(); };
    addAndMakeVisible(magic);

    styleButton(aiDetect, "זיהוי סגנון  ·  AI STYLE");
    styleButton(standardMode, "מצב רגיל  ·  STANDARD");
    styleButton(analyzeOnly, "נתח בלבד  ·  ANALYZE");
    styleButton(applySuggestion, "החל המלצה  ·  APPLY", juce::Colour(0xff244b43));
    styleButton(abCompare, "A  ·  אחרי עיבוד / PROCESSED", juce::Colour(0xff214454));
    abCompare.onClick = [this] {
        showingProcessed = !showingProcessed;
        if (auto* parameter = processor.parameters.getParameter("abBypass"))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(showingProcessed ? 0.0f : 1.0f));
        abCompare.setButtonText(showingProcessed ? "A  ·  אחרי עיבוד / PROCESSED" : "B  ·  לפני עיבוד / DRY");
    };
    addAndMakeVisible(abCompare);
    addAndMakeVisible(aiDetect); addAndMakeVisible(standardMode);
    addAndMakeVisible(analyzeOnly); addAndMakeVisible(applySuggestion);

    status.setText("עיבוד ווקאל חכם  ·  INTELLIGENT VOCAL PROCESSING     |     VST3 / STANDALONE", juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, cyan);
    status.setFont(juce::Font(12.0f, juce::Font::bold));
    addAndMakeVisible(status);

    modeStatus.setText("מצב פעיל: זיהוי סגנון  ·  AI STYLE DETECT", juce::dontSendNotification);
    modeStatus.setColour(juce::Label::textColourId, gold);
    modeStatus.setFont(juce::Font(12.0f));
    addAndMakeVisible(modeStatus);

    analysisResult.setText("נגן או הקלט ווקאל, ואז בחר 'נתח בלבד' או 'החל המלצה'.  ·  Play vocal, then analyze.", juce::dontSendNotification);
    analysisResult.setColour(juce::Label::textColourId, juce::Colour(0xffd6e5f2));
    analysisResult.setJustificationType(juce::Justification::centredRight);
    analysisResult.setFont(juce::Font(13.0f));
    addAndMakeVisible(analysisResult);

    aiDetect.onClick = [this] {
        useAiMode = true;
        modeStatus.setText("מצב פעיל: זיהוי סגנון  ·  AI STYLE DETECT", juce::dontSendNotification);
    };
    standardMode.onClick = [this] {
        useAiMode = false;
        modeStatus.setText("מצב פעיל: ידני בלבד  ·  STANDARD — NO AUTO CHANGES", juce::dontSendNotification);
    };
    analyzeOnly.onClick = [this] {
        processor.analyzeVocal(false);
        analysisResult.setText(processor.getAnalysisSummary(), juce::dontSendNotification);
    };
    applySuggestion.onClick = [this] {
        if (useAiMode) {
            processor.analyzeVocal(true);
            analysisResult.setText(processor.getAnalysisSummary(), juce::dontSendNotification);
        } else {
            analysisResult.setText("עבור למצב זיהוי סגנון כדי להחיל המלצה.  ·  Switch to AI STYLE first.", juce::dontSendNotification);
        }
    };

    // Dedicated pitch-correction strip: Hebrew first, concise English labels second.
    auto tuneLabelStyle = [this](juce::Label& l, const juce::String& text) {
        l.setText(text, juce::dontSendNotification); l.setColour(juce::Label::textColourId, gold);
        l.setFont(juce::Font(11.0f, juce::Font::bold)); l.setJustificationType(juce::Justification::centredRight); addAndMakeVisible(l);
    };
    autoTune.setButtonText("תיקון זיופים · AUTO-TUNE");
    autoTune.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    autoTune.setColour(juce::ToggleButton::tickColourId, cyan); addAndMakeVisible(autoTune);
    tuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "autoTuneOn", autoTune);
    for (int i = 0; i < 12; ++i) tuneKey.addItem(juce::StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"][i], i + 1);
    tuneScale.addItem("כרומטי · CHROMATIC", 1); tuneScale.addItem("מז'ור · MAJOR", 2); tuneScale.addItem("מינור · MINOR", 3);
    tuneKey.setColour(juce::ComboBox::backgroundColourId, surface2); tuneKey.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    tuneScale.setColour(juce::ComboBox::backgroundColourId, surface2); tuneScale.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    addAndMakeVisible(tuneKey); addAndMakeVisible(tuneScale);
    keyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters, "tuneKey", tuneKey);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters, "tuneScale", tuneScale);
    auto setupTuneSlider = [this](juce::Slider& sl, const juce::String& label, const char* id, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment) {
        sl.setSliderStyle(juce::Slider::LinearHorizontal); sl.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 18);
        sl.setColour(juce::Slider::trackColourId, cyan); sl.setColour(juce::Slider::thumbColourId, gold);
        sl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white); sl.setColour(juce::Slider::textBoxBackgroundColourId, surface2);
        addAndMakeVisible(sl); attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, id, sl);
    };
    tuneLabelStyle(tuneKeyLabel, "סולם · KEY"); tuneLabelStyle(tuneScaleLabel, "סוג סולם · SCALE");
    tuneLabelStyle(tuneSpeedLabel, "מהירות · SPEED"); tuneLabelStyle(tuneAmountLabel, "כמות · AMOUNT");
    setupTuneSlider(tuneSpeed, "מהירות תיקון", "tuneSpeed", tuneSpeedAttachment);
    setupTuneSlider(tuneAmount, "עוצמת תיקון", "tuneAmount", tuneAmountAttachment);

    auto addToggle = [this](juce::ToggleButton& b, const juce::String& text, const char* id) {
        b.setButtonText(text);
        b.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
        b.setColour(juce::ToggleButton::tickColourId, cyan);
        b.setColour(juce::ToggleButton::tickDisabledColourId, muted);
        addAndMakeVisible(b);
        toggles.emplace_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, id, b));
    };
    addToggle(gate, "שער רעש · GATE", "gateOn");
    addToggle(eq,   "אקולייזר · EQ", "eqOn");
    addToggle(comp, "קומפרסור · COMP", "compOn");
    addToggle(sat,  "רוויה · SAT", "satOn");
    addToggle(rev,  "ריוורב · VERB", "reverbOn");
    addToggle(del,  "דיליי · DLY", "delayOn");
}

void NahmanVocalSuiteAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff14283b), 0.0f, 0.0f,
                                           background, (float)getWidth(), (float)getHeight(), false));
    g.fillRoundedRectangle(18.0f, 18.0f, (float)getWidth() - 36.0f, (float)getHeight() - 36.0f, 18.0f);

    // Brand signature: a restrained Breslov-inspired touch.
    g.setColour(gold); g.setFont(juce::Font(25.0f, juce::Font::bold));
    g.drawText("נחמן", 42, 27, 100, 38, juce::Justification::right);
    g.setColour(cyan); g.setFont(juce::Font(21.0f, juce::Font::bold));
    g.drawText("VOCAL SUITE", 150, 31, 220, 32, juce::Justification::left);
    g.setColour(juce::Colour(0xff8297aa)); g.setFont(juce::Font(10.5f));
    g.drawText("אין ייאוש בעולם כלל  ·  CREATE WITH HEART", 790, 39, 330, 20, juce::Justification::right);

    const auto panel = [&g](float x, float y, float w, float h) {
        g.setColour(surface); g.fillRoundedRectangle(x, y, w, h, 12.0f);
        g.setColour(juce::Colour(0xff35536a)); g.drawRoundedRectangle(x, y, w, h, 12.0f, 1.0f);
    };
    // Three clear work zones: capture/analyze, processing, final comparison.
    panel(38.0f, 92.0f, (float)getWidth() - 76.0f, 166.0f);
    panel(38.0f, 270.0f, (float)getWidth() - 76.0f, 420.0f);
    panel(38.0f, 702.0f, (float)getWidth() - 76.0f, 64.0f);

    g.setColour(cyan); g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText("01  הקלטה וניתוח  ·  RECORD & ANALYZE", 54, 98, 400, 20, juce::Justification::left);
    g.drawText("02  עיבוד ועיצוב הקול  ·  VOCAL PROCESSING", 54, 276, 420, 20, juce::Justification::left);
    g.setColour(gold); g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawText("תיקון זיופים  ·  PITCH CORRECTION", 54, 320, 300, 18, juce::Justification::left);
    g.drawText("03  מיקס סופי והשוואה  ·  FINAL MIX & A/B", 54, 708, 350, 20, juce::Justification::left);
    const float outLevel = juce::jlimit(0.0f, 1.0f, processor.getOutputRms() * 5.0f);
    const float outPeak = juce::jlimit(0.0f, 1.0f, processor.getOutputPeak());
    g.setColour(juce::Colour(0xff0a1420)); g.fillRoundedRectangle(410.0f, 733.0f, 205.0f, 12.0f, 6.0f);
    g.setColour(gold); g.fillRoundedRectangle(410.0f, 733.0f, 205.0f * outLevel, 12.0f, 6.0f);
    g.setColour(cyan); g.drawLine(410.0f + 205.0f * outPeak, 729.0f, 410.0f + 205.0f * outPeak, 750.0f, 2.0f);
    g.setColour(muted); g.setFont(juce::Font(9.5f));
    g.drawText("יציאה  ·  OUTPUT", 410, 714, 205, 16, juce::Justification::left);

    // Live input waveform (128-point snapshot supplied by the audio thread).
    const juce::Rectangle<float> waveArea(58.0f, 124.0f, 330.0f, 105.0f);
    g.setColour(juce::Colour(0xff0a1420)); g.fillRoundedRectangle(waveArea, 8.0f);
    g.setColour(juce::Colour(0xff29475a));
    g.drawLine(waveArea.getX(), waveArea.getCentreY(), waveArea.getRight(), waveArea.getCentreY(), 1.0f);
    juce::Path wave;
    for (int i = 0; i < 128; ++i) {
        const float x = waveArea.getX() + (float)i / 127.0f * waveArea.getWidth();
        const float y = waveArea.getCentreY() - processor.getWaveformSample(i) * waveArea.getHeight() * 0.43f;
        if (i == 0) wave.startNewSubPath(x, y); else wave.lineTo(x, y);
    }
    g.setColour(cyan); g.strokePath(wave, juce::PathStrokeType(1.8f));

    // Input level meter and numeric readout.
    const float level = juce::jlimit(0.0f, 1.0f, processor.getInputRms() * 5.0f);
    const float peak = juce::jlimit(0.0f, 1.0f, processor.getInputPeak());
    g.setColour(juce::Colour(0xff0a1420)); g.fillRoundedRectangle(410.0f, 153.0f, 300.0f, 15.0f, 7.0f);
    g.setColour(cyan); g.fillRoundedRectangle(410.0f, 153.0f, 300.0f * level, 15.0f, 7.0f);
    g.setColour(gold); g.drawLine(410.0f + 300.0f * peak, 148.0f, 410.0f + 300.0f * peak, 174.0f, 2.0f);
    g.setColour(muted); g.setFont(juce::Font(10.5f));
    g.drawText("עוצמת כניסה  ·  INPUT LEVEL", 410, 130, 300, 18, juce::Justification::left);
    g.drawText("RMS  " + juce::String(juce::Decibels::gainToDecibels(juce::jmax(0.00001f, processor.getInputRms())), 1) + " dB", 410, 177, 145, 18, juce::Justification::left);
    g.drawText("PEAK  " + juce::String(juce::Decibels::gainToDecibels(juce::jmax(0.00001f, processor.getInputPeak())), 1) + " dB", 565, 177, 145, 18, juce::Justification::left);

    g.setColour(juce::Colour(0xff71879c)); g.setFont(juce::Font(10.0f));
    g.drawText("גרסת פיתוח 0.5  ·  DEVELOPMENT BUILD", 54, getHeight() - 25, 420, 14, juce::Justification::left);
}

void NahmanVocalSuiteAudioProcessorEditor::resized()
{
    // Zone 1: record / analyze controls.
    status.setBounds(410, 112, 700, 18);
    modeStatus.setBounds(410, 132, 300, 20);
    aiDetect.setBounds(730, 125, 175, 32);
    standardMode.setBounds(913, 125, 175, 32);
    analyzeOnly.setBounds(730, 164, 160, 32);
    applySuggestion.setBounds(898, 164, 190, 32);
    analysisResult.setBounds(730, 202, 365, 40);

    // Zone 2: processing modules.
    juce::ToggleButton* buttons[] = { &gate, &eq, &comp, &sat, &rev, &del };
    for (int i = 0; i < 6; ++i) buttons[i]->setBounds(54 + i * 178, 365, 170, 22);

    juce::Component* knobs[] = { &input, &hpf, &low, &mid, &high, &threshold, &ratio, &saturation, &reverb, &delay, &output };
    const int columns = 6, startX = 58, startY = 405, cellW = 180, cellH = 135;
    for (int i = 0; i < 11; ++i) {
        const int row = i / columns, col = i % columns;
        knobs[i]->setBounds(startX + col * cellW, startY + row * cellH, 116, 104);
        if (i < (int)labels.size()) labels[(size_t)i]->setBounds(startX + col * cellW - 5, startY + row * cellH + 103, 126, 20);
    }

    // Auto-tune controls sit above the general processing row.
    autoTune.setBounds(54, 320, 190, 26);
    tuneKeyLabel.setBounds(260, 319, 70, 20); tuneKey.setBounds(334, 318, 72, 25);
    tuneScaleLabel.setBounds(416, 319, 95, 20); tuneScale.setBounds(514, 318, 170, 25);
    tuneSpeedLabel.setBounds(690, 319, 82, 20); tuneSpeed.setBounds(772, 318, 135, 24);
    tuneAmountLabel.setBounds(914, 319, 82, 20); tuneAmount.setBounds(996, 318, 112, 24);

    // Zone 3: instant dry/processed A-B and one-click starting mix.
    magic.setBounds(650, 718, 220, 34);
    abCompare.setBounds(884, 718, 225, 34);
}

void NahmanVocalSuiteAudioProcessorEditor::timerCallback()
{
    repaint(50, 118, 670, 116);
    repaint(400, 710, 225, 48);
}

