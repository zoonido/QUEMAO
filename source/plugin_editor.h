#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "plugin_processor.h"

namespace ui
{
    const juce::Colour ground   { 0xff141210 };
    const juce::Colour panel    { 0xff1d1a17 };
    const juce::Colour edge     { 0xff2e2924 };
    const juce::Colour well     { 0xff2a2521 };
    const juce::Colour wellEdge { 0xff3d3630 };
    const juce::Colour text     { 0xffece4d8 };
    const juce::Colour muted    { 0xffa39a8d };
    const juce::Colour ember    { 0xffff6a2b };
    const juce::Colour laneColours[4] = { juce::Colour (0xffff6a2b), juce::Colour (0xfff0b23a),
                                          juce::Colour (0xffe8dcc8), juce::Colour (0xffc9774d) };
}

class QuemaoLook : public juce::LookAndFeel_V4
{
public:
    QuemaoLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

class StepGrid : public juce::Component
{
public:
    StepGrid (quemao::Lane& l, juce::Colour c) : lane (l), colour (c) {}
    int stepAt (float x) const { return juce::jlimit (0, 15, (int) (x / ((float) getWidth() / 16.0f))); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    quemao::Lane& lane;
    juce::Colour colour;
};

class LaneStrip : public juce::Component
{
public:
    LaneStrip (QuemaoProcessor&, int index);
    ~LaneStrip() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

private:
    void setChoice (const char* base, int value);
    int choice (const char* base) const;

    QuemaoProcessor& proc;
    const int index;
    const juce::Colour colour;

    juce::TextButton engines[quemao::kNumEngines];
    juce::TextButton launch { "LAUNCH" }, trigger { "TRIGGER" }, latch { "LATCH" };
    static constexpr int kNumKnobs = 13;
    juce::Slider knobs[kNumKnobs];
    juce::Label knobLabels[kNumKnobs];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAttach[kNumKnobs];
    juce::TextButton lp { "LP" }, hp { "HP" }, chokeButtons[3];

    // stage 4: generator
    juce::ComboBox genreBox;
    juce::TextButton generateButton { "GENERATE" };
    static constexpr int kNumGenKnobs = 6;
    juce::Slider genKnobs[kNumGenKnobs];
    juce::Label genLabels[kNumGenKnobs];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> genAttach[kNumGenKnobs];

    // stage 5: sends
    juce::Slider sendKnobs[3];
    juce::Label sendLabels[3];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sendAttach[3];
    juce::ComboBox delayTimeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> delayTimeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> latchAttach;
    StepGrid grid;

    int lastFlash = 0, lastEng = -1;
    float ledLevel = 0.0f;
};

// stage 5: the four effect blocks along the bottom
class FxPanel : public juce::Component
{
public:
    explicit FxPanel (QuemaoProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    struct Knob { juce::Slider s; juce::Label l; std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a; };
    void addKnob (Knob&, const char* id, const char* label);
    QuemaoProcessor& proc;
    Knob chTone, chRate, chMix, dFdbk, dTone, dMix, vSize, vDamp, vMix, comp, crush, gain;
    juce::ComboBox dTime, vType;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> dTimeA, vTypeA;
};

// stage 6: the mod sequencer panel
class ModBars : public juce::Component
{
public:
    explicit ModBars (QuemaoProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override;
    std::function<void()> onEdit;
    int hover = -1;
private:
    void setAt (juce::Point<float>);
    int stepAt (float x) const { return juce::jlimit (0, 15, (int) (x / ((float) getWidth() / 16.0f))); }
    QuemaoProcessor& proc;
};

class ModPanel : public juce::Component
{
public:
    explicit ModPanel (QuemaoProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
private:
    void updateTargetNames();
    juce::String describe (int target, float offset) const;
    QuemaoProcessor& proc;
    juce::ComboBox target, rate;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rateA;
    juce::Slider depth;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthA;
    juce::TextButton clear { "CLEAR" }, dice { "RANDOM" };
    ModBars bars;
    int lastStep = -1, lastTarget = -1;
};

// everything inside the window, drawn at 1200 x 982 and scaled to the window size
class MainView : public juce::Component
{
public:
    explicit MainView (QuemaoProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    static constexpr int kWidth = 1200, kHeight = 982;
private:
    QuemaoProcessor& proc;
    std::unique_ptr<LaneStrip> strips[4];
    ModPanel modPanel;
    FxPanel fxPanel;
};

class QuemaoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit QuemaoEditor (QuemaoProcessor&);
    ~QuemaoEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshNow() { view.refresh(); }

private:
    void timerCallback() override;

    QuemaoProcessor& proc;
    QuemaoLook look;
    MainView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuemaoEditor)
};
