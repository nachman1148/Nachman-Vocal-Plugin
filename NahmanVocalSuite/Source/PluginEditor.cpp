#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::Colour background { 0xff100d09 };
    const juce::Colour surface { 0xe51b1711 };
    const juce::Colour surface2 { 0xff30271a };
    const juce::Colour cyan { 0xff35bfff };
    const juce::Colour gold { 0xffffc451 };
    const juce::Colour muted { 0xffd6c6a8 };

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
    setSize(1280, 850);
    startTimerHz(30);
    // Load the exact user-selected house photo from Documents/NahmanVocalSuite.
    const auto bgFolder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("NahmanVocalSuite");
    const auto pngBackground = bgFolder.getChildFile("nahman_background.png");
    const auto jpgBackground = bgFolder.getChildFile("nahman_background.jpg");
    if (pngBackground.existsAsFile())
        backgroundImage = juce::ImageFileFormat::loadFrom(pngBackground);
    else if (jpgBackground.existsAsFile())
        backgroundImage = juce::ImageFileFormat::loadFrom(jpgBackground);

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
    addKnob(deEsserAmount, "דה־אסר · DE-ESSER", "deEsserAmount");

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
    const char* tuneKeys[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i) tuneKey.addItem(tuneKeys[i], i + 1);
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
    addToggle(deEsser, "דה־אסר · DE-ESSER", "deEsserOn");
}

void NahmanVocalSuiteAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff8b6030), 0.0f, 0.0f,
                                           juce::Colour(0xff100d09), (float)getWidth(), (float)getHeight(), false));
    g.fillAll();
    if (backgroundImage.isValid())
        g.drawImage(backgroundImage, getLocalBounds().toFloat(), juce::RectanglePlacement(juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination));
    g.setColour(juce::Colour(0x72090000)); g.fillRect(0, 0, getWidth(), getHeight());
    g.setColour(juce::Colour(0x5a000000));
    g.fillRoundedRectangle(18.0f, 18.0f, (float)getWidth() - 36.0f, (float)getHeight() - 36.0f, 18.0f);

    g.setColour(gold); g.setFont(juce::Font(15.0f, juce::Font::bold));
    g.drawText("Breslov Audio", 34, 22, 150, 30, juce::Justification::left);
    g.setColour(juce::Colours::white); g.setFont(juce::Font(48.0f, juce::Font::bold));
    g.drawText("NAHMAN", 48, 105, 470, 70, juce::Justification::left);
    g.setColour(gold); g.setFont(juce::Font(43.0f, juce::Font::italic));
    g.drawText("Vocal Suite", 52, 165, 460, 62, juce::Justification::left);
    g.setColour(juce::Colour(0xfff5ead8)); g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("PROFESSIONAL VOCAL PROCESSOR", 58, 224, 430, 24, juce::Justification::left);
    g.setColour(gold); g.setFont(juce::Font(19.0f, juce::Font::italic));
    g.drawText("אין ייאוש בעולם כלל", getWidth() - 330, 28, 280, 34, juce::Justification::right);
    g.setColour(juce::Colour(0x99ffc451)); g.drawLine((float)getWidth() - 360.0f, 67.0f, (float)getWidth() - 48.0f, 67.0f, 1.5f);

    const auto panel = [&g](float x, float y, float w, float h) {
        g.setColour(juce::Colour(0xb91b1712)); g.fillRoundedRectangle(x, y, w, h, 15.0f);
        g.setColour(juce::Colour(0x88ffc451)); g.drawRoundedRectangle(x, y, w, h, 15.0f, 1.0f);
    };
    panel(26.0f, 285.0f, (float)getWidth() - 52.0f, 300.0f);
    panel(26.0f, 600.0f, 710.0f, 210.0f);
    panel(750.0f, 600.0f, 300.0f, 210.0f);
    panel(1065.0f, 600.0f, 185.0f, 210.0f);

    g.setColour(gold); g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText("VOCAL PROCESSING  ·  עיבוד הקול", 44, 292, 400, 22, juce::Justification::left);
    g.drawText("EQUALIZER  ·  אקולייזר", 44, 610, 260, 20, juce::Justification::left);
    g.drawText("MULTIBAND  ·  תחומי תדר", 770, 610, 260, 20, juce::Justification::left);
    g.drawText("OUTPUT  ·  יציאה", 1084, 610, 150, 20, juce::Justification::left);

    const float outLevel = juce::jlimit(0.0f, 1.0f, processor.getOutputRms() * 5.0f);
    const float outPeak = juce::jlimit(0.0f, 1.0f, processor.getOutputPeak());
    g.setColour(juce::Colour(0xff090806)); g.fillRoundedRectangle(1085.0f, 652.0f, 22.0f, 126.0f, 7.0f);
    g.setColour(juce::Colour(0xff55d98b)); g.fillRoundedRectangle(1087.0f, 778.0f - 122.0f * outLevel, 18.0f, 122.0f * outLevel, 5.0f);
    g.setColour(gold); g.drawLine(1083.0f, 778.0f - 122.0f * outPeak, 1110.0f, 778.0f - 122.0f * outPeak, 2.0f);
    g.setColour(muted); g.setFont(juce::Font(10.0f));
    g.drawText("RMS  " + juce::String(juce::Decibels::gainToDecibels(juce::jmax(0.00001f, processor.getOutputRms())), 1) + " dB", 1118, 665, 120, 18, juce::Justification::left);
    g.drawText("PEAK  " + juce::String(juce::Decibels::gainToDecibels(juce::jmax(0.00001f, processor.getOutputPeak())), 1) + " dB", 1118, 690, 120, 18, juce::Justification::left);

    const juce::Rectangle<float> eqArea(48.0f, 648.0f, 660.0f, 142.0f);
    g.setColour(juce::Colour(0xff100e0a)); g.fillRoundedRectangle(eqArea, 8.0f);
    g.setColour(juce::Colour(0x55ffc451));
    for (int i = 1; i < 8; ++i) g.drawVerticalLine((int)(eqArea.getX() + i * eqArea.getWidth() / 8.0f), eqArea.getY(), eqArea.getBottom());
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine((int)(eqArea.getY() + i * eqArea.getHeight() / 4.0f), eqArea.getX(), eqArea.getRight());
    juce::Path curve;
    curve.startNewSubPath(eqArea.getX(), eqArea.getCentreY());
    curve.cubicTo(eqArea.getX()+120, eqArea.getY()+eqArea.getHeight()*0.72f, eqArea.getX()+170, eqArea.getY()+eqArea.getHeight()*0.20f, eqArea.getX()+300, eqArea.getY()+eqArea.getHeight()*0.43f);
    curve.cubicTo(eqArea.getX()+420, eqArea.getY()+eqArea.getHeight()*0.56f, eqArea.getX()+520, eqArea.getY()+eqArea.getHeight()*0.58f, eqArea.getRight(), eqArea.getCentreY());
    g.setColour(gold); g.strokePath(curve, juce::PathStrokeType(2.4f));

    const juce::Rectangle<float> waveArea(540.0f, 112.0f, 350.0f, 105.0f);
    g.setColour(juce::Colour(0x99100d09)); g.fillRoundedRectangle(waveArea, 10.0f);
    g.setColour(juce::Colour(0x66ffc451)); g.drawLine(waveArea.getX(), waveArea.getCentreY(), waveArea.getRight(), waveArea.getCentreY(), 1.0f);
    juce::Path wave;
    for (int i = 0; i < 128; ++i) {
        const float x = waveArea.getX() + (float)i / 127.0f * waveArea.getWidth();
        const float y = waveArea.getCentreY() - processor.getWaveformSample(i) * waveArea.getHeight() * 0.43f;
        if (i == 0) wave.startNewSubPath(x, y); else wave.lineTo(x, y);
    }
    g.setColour(cyan); g.strokePath(wave, juce::PathStrokeType(1.8f));
    g.setColour(muted); g.setFont(juce::Font(10.0f));
    g.drawText("LIVE VOCAL SIGNAL  ·  אות חי", 548, 88, 300, 18, juce::Justification::left);
    g.drawText("גרסת פיתוח  ·  DEVELOPMENT BUILD", 42, getHeight() - 22, 400, 14, juce::Justification::left);
}

void NahmanVocalSuiteAudioProcessorEditor::resized()
{
    status.setBounds(540, 225, 570, 18);
    modeStatus.setBounds(540, 245, 300, 20);
    aiDetect.setBounds(850, 238, 170, 30);
    standardMode.setBounds(1026, 238, 190, 30);
    analyzeOnly.setBounds(850, 271, 160, 28);
    applySuggestion.setBounds(1018, 271, 198, 28);
    analysisResult.setBounds(540, 192, 660, 28);

    autoTune.setBounds(48, 300, 185, 25);
    tuneKeyLabel.setBounds(240, 300, 65, 20); tuneKey.setBounds(305, 298, 72, 25);
    tuneScaleLabel.setBounds(385, 300, 80, 20); tuneScale.setBounds(465, 298, 145, 25);
    tuneSpeedLabel.setBounds(620, 300, 75, 20); tuneSpeed.setBounds(695, 298, 125, 24);
    tuneAmountLabel.setBounds(830, 300, 75, 20); tuneAmount.setBounds(905, 298, 115, 24);

    juce::ToggleButton* buttons[] = { &gate, &eq, &comp, &sat, &rev, &del, &deEsser };
    const int bx[] = { 42, 214, 386, 558, 730, 902, 1074 };
    for (int i = 0; i < 7; ++i) buttons[i]->setBounds(bx[i], 330, 160, 25);

    juce::Component* knobs[] = { &input, &hpf, &low, &mid, &high, &threshold, &ratio, &saturation, &reverb, &delay, &output, &deEsserAmount };
    const int x[] = { 42, 214, 386, 558, 730, 902, 1074, 42, 214, 386, 558, 730 };
    for (int i = 0; i < 12; ++i) {
        const int y = i < 7 ? 365 : 455;
        knobs[i]->setBounds(x[i], y, 112, 86);
        if (i < (int)labels.size()) labels[(size_t)i]->setBounds(x[i] - 8, y + 82, 128, 18);
    }

    magic.setBounds(42, 750, 190, 34);
    abCompare.setBounds(248, 750, 235, 34);
}

void NahmanVocalSuiteAudioProcessorEditor::timerCallback()
{
    repaint(50, 118, 670, 116);
    repaint(400, 710, 225, 48);
}

