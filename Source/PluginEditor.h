#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

namespace flui
{
using APVTS = juce::AudioProcessorValueTreeState;

class FlowerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FlowerLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

/** Rotary "petal" knob with a title above and a value below. */
struct Knob : juce::Component
{
    Knob (APVTS& s, const juce::String& paramId, const juce::String& title, juce::Colour accent);
    void resized() override;
    juce::Slider slider;
    juce::Label title;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

/** On/off button bound to a bool parameter. */
struct Toggle : juce::TextButton
{
    Toggle (APVTS& s, const juce::String& paramId, const juce::String& text, juce::Colour lit);
    std::unique_ptr<APVTS::ButtonAttachment> att;
};

/** Row of mutually-exclusive buttons bound to a choice parameter. */
struct Segmented : juce::Component
{
    Segmented (APVTS& s, const juce::String& paramId, juce::StringArray labels, juce::Colour lit);
    void resized() override;
    void refresh();
    APVTS& state; juce::RangedAudioParameter* param;
    juce::OwnedArray<juce::TextButton> buttons;
};

/** The centrepiece: an animated flower that IS the Open <-> Dense control. */
struct Flower : juce::Component
{
    explicit Flower (ChengFlowerProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void tick();

    ChengFlowerProcessor& proc;
    juce::RangedAudioParameter* param;
    float dispBloom = 0.0f, level = 0.0f, time = 0.0f, dragStart = 0.0f;
    float sweet = 0.0f, shine = 0.0f, activity = 0.0f;
    std::array<float, fl::Unmask::kBands> cut {};

private:
    void setBloom (float v);
};

/** Output spectrum with build-up hints from the unmask detector. */
struct Spectrum : juce::Component
{
    explicit Spectrum (ChengFlowerProcessor& p);
    void paint (juce::Graphics&) override;
    void tick();

    ChengFlowerProcessor& proc;
    juce::dsp::FFT fft { 11 };
    std::vector<float> fftData, window, smooth;
    std::array<float, fl::Unmask::kBands> cut {}, excess {};
};

struct LevelMeter : juce::Component
{
    void paint (juce::Graphics&) override;
    float db = -100.0f, grDb = 0.0f;
};
} // namespace flui

class ChengFlowerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChengFlowerEditor (ChengFlowerProcessor&);
    ~ChengFlowerEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 980, baseH = 720;
    void refresh() { timerCallback(); }

private:
    void timerCallback() override;

    struct Content : juce::Component
    {
        explicit Content (ChengFlowerProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

        ChengFlowerProcessor& proc;
        flui::APVTS& s;
        flui::Segmented mode, sweetChar;
        flui::Toggle autoGain, bypass, limiter;
        flui::Flower flower;
        flui::Knob shine, thick, low, sweet, focus, harsh, width, color, output, ceiling, mix;
        flui::Spectrum spectrum;
        flui::LevelMeter meter;
        juce::String infoLine;
    };

    ChengFlowerProcessor& proc;
    flui::FlowerLookAndFeel lnf;
    Content content;
};
