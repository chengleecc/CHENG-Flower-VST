#include "PluginEditor.h"

using namespace flui;
using juce::Colour;

namespace
{
    const Colour kBgTop   { 0xff1a141f };
    const Colour kBgBot   { 0xff0c0a10 };
    const Colour kPanel   { 0xff17131b };
    const Colour kEdge    { 0xff2c2532 };
    const Colour kText    { 0xffefe7ea };
    const Colour kDim     { 0xff8c8290 };
    const Colour kRose    { 0xfff2a7c3 };
    const Colour kPeach   { 0xffffd9c8 };
    const Colour kMagenta { 0xffb02e6c };
    const Colour kAmber   { 0xfff0a24a };
    const Colour kGold    { 0xffe9c46a };

    const Colour kShineC { 0xfff6d77a }, kLowC { 0xffe07a5f }, kSweetC { 0xfff2a65a };
    const Colour kFocusC { 0xff9ad1d4 }, kHarshC { 0xffb8a1e3 }, kWidthC { 0xff7fb7e8 };
    const Colour kOutC   { 0xffd9cfd3 };
    const Colour kThickC { 0xffe58fa8 }, kColorC { 0xffd4a373 };

    juce::Font font (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    float freqToX (float f, float w) { return w * std::log (f / 20.0f) / std::log (1000.0f); }
}

//==============================================================================
FlowerLookAndFeel::FlowerLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, kText);
    setColour (juce::TextEditor::highlightColourId, kRose.withAlpha (0.4f));
}

void FlowerLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const Colour acc = s.findColour (juce::Slider::rotarySliderFillColourId);

    // soft glow that grows with the value
    const float glowA = 0.05f + 0.25f * pos;
    g.setGradientFill (juce::ColourGradient (acc.withAlpha (glowA), c.x, c.y, acc.withAlpha (0.0f), c.x + r * 1.25f, c.y, true));
    g.fillEllipse (c.x - r * 1.25f, c.y - r * 1.25f, r * 2.5f, r * 2.5f);

    // track + value arc
    const float ar = r - 2.0f;
    juce::Path track;
    track.addCentredArc (c.x, c.y, ar, ar, 0.0f, start, end, true);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, ar, ar, 0.0f, std::min (from, angle), std::max (from, angle), true);
        g.setColour (acc);
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // body
    const float br = r - 8.0f;
    g.setGradientFill (juce::ColourGradient (Colour (0xff3a3240), c.x, c.y - br, Colour (0xff141117), c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (c.x - br, c.y - br, br * 2, br * 2, 1.0f);

    // pointer dot
    const float pr = br * 0.68f;
    const float px = c.x + pr * std::sin (angle), py = c.y - pr * std::cos (angle);
    g.setColour (acc);
    g.fillEllipse (px - 3.0f, py - 3.0f, 6.0f, 6.0f);
}

void FlowerLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const Colour lit = b.findColour (juce::TextButton::buttonOnColourId);
    Colour fill = on ? lit.withAlpha (0.9f) : Colour (0xff221c27);
    if (over) fill = fill.brighter (0.1f);
    if (down) fill = fill.darker (0.15f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (on ? lit.brighter (0.3f) : kEdge);
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
}

void FlowerLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (font (std::min (11.5f, b.getHeight() * 0.48f), true));
    g.setColour (b.getToggleState() ? Colour (0xff1a1418) : kText.withAlpha (0.8f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1);
}

//==============================================================================
Knob::Knob (APVTS& s, const juce::String& paramId, const juce::String& text, Colour accent)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setColour (juce::Slider::textBoxTextColourId, kText.withAlpha (0.85f));
    slider.setVelocityBasedMode (false);
    title.setText (text, juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setColour (juce::Label::textColourId, accent.interpolatedWith (kText, 0.35f));
    title.setFont (font (12.5f, true));
    addAndMakeVisible (slider);
    addAndMakeVisible (title);
    att = std::make_unique<APVTS::SliderAttachment> (s, paramId, slider);

    const auto unit = s.getParameter (paramId)->getLabel();
    slider.textFromValueFunction = [unit] (double v) -> juce::String
    {
        if (unit == "dB") return (v > 0.05 ? "+" : "") + juce::String (v, 1) + " dB";
        return juce::String (juce::roundToInt (v)) + "%";
    };
    slider.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
    slider.updateText();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    title.setBounds (r.removeFromTop (18));
    slider.setBounds (r);
}

Toggle::Toggle (APVTS& s, const juce::String& paramId, const juce::String& text, Colour lit)
{
    setButtonText (text);
    setClickingTogglesState (true);
    setColour (juce::TextButton::buttonOnColourId, lit);
    att = std::make_unique<APVTS::ButtonAttachment> (s, paramId, *this);
}

Segmented::Segmented (APVTS& s, const juce::String& paramId, juce::StringArray labels, Colour lit)
    : state (s), param (s.getParameter (paramId))
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i].toUpperCase()));
        b->setColour (juce::TextButton::buttonOnColourId, lit);
        b->onClick = [this, i]
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) i));
            param->endChangeGesture();
            refresh();
        };
        addAndMakeVisible (b);
    }
    refresh();
}

void Segmented::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / std::max (1, buttons.size());
    for (auto* b : buttons) b->setBounds (r.removeFromLeft (w).reduced (2, 0));
}

void Segmented::refresh()
{
    const int idx = juce::roundToInt (param->convertFrom0to1 (param->getValue()));
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == idx, juce::dontSendNotification);
}

//==============================================================================
// Flower
//==============================================================================
Flower::Flower (ChengFlowerProcessor& p) : proc (p), param (p.apvts.getParameter ("bloom"))
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    dispBloom = param->convertFrom0to1 (param->getValue()) * 0.01f;
}

void Flower::setBloom (float v)
{
    param->setValueNotifyingHost (param->convertTo0to1 (juce::jlimit (-100.0f, 100.0f, v)));
}

void Flower::mouseDown (const juce::MouseEvent&)
{
    dragStart = param->convertFrom0to1 (param->getValue());
    param->beginChangeGesture();
}

void Flower::mouseDrag (const juce::MouseEvent& e)
{
    const float sens = e.mods.isShiftDown() ? 0.2f : 0.8f;
    setBloom (dragStart - (float) e.getDistanceFromDragStartY() * sens);
}

void Flower::mouseUp (const juce::MouseEvent&) { param->endChangeGesture(); }

void Flower::mouseDoubleClick (const juce::MouseEvent&)
{
    param->beginChangeGesture(); setBloom (0.0f); param->endChangeGesture();
}

void Flower::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    param->beginChangeGesture();
    setBloom (param->convertFrom0to1 (param->getValue()) + w.deltaY * 40.0f);
    param->endChangeGesture();
}

void Flower::tick()
{
    const float target = param->convertFrom0to1 (param->getValue()) * 0.01f;
    dispBloom += (target - dispBloom) * 0.25f;
    const float lv = juce::jlimit (0.0f, 1.0f, (proc.meterOut.load() + 36.0f) / 36.0f);
    level += (lv - level) * 0.2f;
    sweet = proc.apvts.getRawParameterValue ("sweet")->load() * 0.01f;
    shine = proc.apvts.getRawParameterValue ("shine")->load() * 0.01f * proc.meterShineGate.load();
    activity += (proc.meterBloom.load() - activity) * 0.3f;
    for (size_t i = 0; i < cut.size(); ++i)
        cut[i] += (std::abs (proc.meterCut[i].load()) - cut[i]) * 0.35f;
    time += 1.0f / 30.0f;
    repaint();
}

void Flower::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre().translated (0.0f, -10.0f);
    const float R = std::min (b.getWidth(), b.getHeight()) * 0.42f;
    const float open = std::max (0.0f, -dispBloom), dense = std::max (0.0f, dispBloom);
    const float breath = 0.97f + 0.045f * level + 0.01f * std::sin (time * 1.3f);
    const float Rs = R * breath;

    // halo
    const Colour haloC = kRose.interpolatedWith (kMagenta, dense).interpolatedWith (kPeach, open * 0.6f);
    const float hr = std::min (R * 1.4f, std::min (b.getWidth() * 0.5f, c.y - b.getY()) - 1.0f);
    g.setGradientFill (juce::ColourGradient (haloC.withAlpha (0.20f + 0.12f * level), c.x, c.y,
                                             haloC.withAlpha (0.0f), c.x + hr, c.y, true));
    g.fillEllipse (c.x - hr, c.y - hr, hr * 2.0f, hr * 2.0f);

    auto petal = [] (float len, float wid)
    {
        juce::Path p;
        p.startNewSubPath (0.0f, 0.0f);
        p.cubicTo (wid * 1.05f, -len * 0.28f, wid * 0.75f, -len * 0.92f, 0.0f, -len);
        p.cubicTo (-wid * 0.75f, -len * 0.92f, -wid * 1.05f, -len * 0.28f, 0.0f, 0.0f);
        p.closeSubPath();
        return p;
    };

    const int nPetals = 8;
    const float lenOut = Rs * (0.80f + 0.16f * open - 0.16f * dense);
    const float widOut = Rs * (0.27f - 0.05f * open + 0.13f * dense);
    const float baseOff = Rs * (0.06f + 0.12f * open);
    const float spin = 0.12f * open - 0.08f * dense + 0.02f * std::sin (time * 0.4f);

    const Colour baseCol = kRose.interpolatedWith (kMagenta, dense * 0.9f);
    const Colour tipCol  = kPeach.interpolatedWith (kAmber, dense * 0.8f);

    auto drawRing = [&] (float len, float wid, float off, float rot, float alpha, float darken, bool outer)
    {
        for (int i = 0; i < nPetals; ++i)
        {
            // each outer petal listens to a slice of the unmask bands
            float tremble = 0.0f, glow = 0.0f;
            if (outer)
            {
                const int b0 = i * fl::Unmask::kBands / nPetals;
                const int b1 = (i + 1) * fl::Unmask::kBands / nPetals;
                for (int k = b0; k < b1; ++k) glow = std::max (glow, cut[(size_t) k]);
                tremble = std::sin (time * 23.0f + (float) i * 1.7f) * glow * 0.006f;
            }
            const float a = rot + (float) i * juce::MathConstants<float>::twoPi / (float) nPetals + tremble;
            const auto tr = juce::AffineTransform::translation (0.0f, -off).rotated (a).translated (c.x, c.y);
            const auto path = petal (len * (1.0f - 0.015f * glow), wid);

            const auto tipPt  = juce::Point<float> (0.0f, -off - len).transformedBy (juce::AffineTransform::rotation (a).translated (c.x, c.y));
            const auto basePt = juce::Point<float> (0.0f, -off).transformedBy (juce::AffineTransform::rotation (a).translated (c.x, c.y));
            juce::ColourGradient grad (baseCol.darker (darken).withAlpha (alpha), basePt.x, basePt.y,
                                       tipCol.darker (darken * 0.5f).withAlpha (alpha), tipPt.x, tipPt.y, false);
            g.setGradientFill (grad);
            g.fillPath (path, tr);

            g.setColour (juce::Colours::white.withAlpha (0.10f + 0.05f * glow));
            g.strokePath (path, juce::PathStrokeType (1.0f), tr);

            // centre vein
            juce::Path vein;
            vein.startNewSubPath (0.0f, -len * 0.08f);
            vein.quadraticTo (wid * 0.08f, -len * 0.5f, 0.0f, -len * 0.82f);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.strokePath (vein, juce::PathStrokeType (0.8f), tr);

            if (outer && glow > 0.3f) // build-up being removed: warm edge light
            {
                g.setColour (kGold.withAlpha (std::min (0.5f, glow * 0.07f)));
                g.strokePath (path, juce::PathStrokeType (2.0f), tr);
            }
            if (outer && shine > 0.02f) // shine sparkle at the tips
            {
                const float sp = 0.5f + 0.5f * std::sin (time * 3.1f + (float) i * 2.3f);
                const float rr = 3.0f + 7.0f * shine * sp;
                g.setGradientFill (juce::ColourGradient (Colour (0xfffff6dc).withAlpha (0.3f + 0.65f * shine * sp), tipPt.x, tipPt.y,
                                                         kGold.withAlpha (0.0f), tipPt.x + rr, tipPt.y, true));
                g.fillEllipse (tipPt.x - rr, tipPt.y - rr, rr * 2, rr * 2);
            }
        }
    };

    drawRing (lenOut, widOut, baseOff, spin, 0.92f, 0.18f, true);
    drawRing (lenOut * (0.62f + 0.06f * dense), widOut * (0.85f + 0.1f * dense), baseOff * 0.5f,
              spin * 1.6f + juce::MathConstants<float>::pi / (float) nPetals, 0.96f, 0.0f, false);

    // core
    const float cr = Rs * (0.20f + 0.03f * dense);
    const Colour warm = kGold.interpolatedWith (kAmber, sweet);
    g.setGradientFill (juce::ColourGradient (warm.withAlpha (0.35f + 0.6f * sweet), c.x, c.y,
                                             warm.withAlpha (0.0f), c.x + cr * 2.3f, c.y, true));
    g.fillEllipse (c.x - cr * 2.3f, c.y - cr * 2.3f, cr * 4.6f, cr * 4.6f);
    g.setGradientFill (juce::ColourGradient (Colour (0xff2a2030), c.x, c.y - cr, Colour (0xff0f0c12), c.x, c.y + cr, false));
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
    g.setColour (warm.withAlpha (0.85f));
    g.drawEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2, 1.5f);
    // pollen dots
    for (int i = 0; i < 12; ++i)
    {
        const float a = (float) i * 0.5236f + time * 0.15f;
        const float pr = cr * 1.18f;
        g.setColour (warm.withAlpha (0.55f));
        g.fillEllipse (c.x + pr * std::cos (a) - 1.6f, c.y + pr * std::sin (a) - 1.6f, 3.2f, 3.2f);
    }

    const int v = juce::roundToInt (dispBloom * 100.0f);
    const juce::String word = v < 0 ? "OPEN" : (v > 0 ? "DENSE" : "BLOOM");
    g.setColour (kText);
    g.setFont (font (cr * 0.36f, true));
    g.drawText (word, juce::Rectangle<float> (c.x - cr, c.y - cr * 0.42f, cr * 2, cr * 0.4f), juce::Justification::centred);
    g.setFont (font (cr * 0.52f, true));
    g.setColour (warm.brighter (0.3f));
    g.drawText (v == 0 ? juce::String ("0") : juce::String (std::abs (v)), juce::Rectangle<float> (c.x - cr, c.y - cr * 0.02f, cr * 2, cr * 0.55f),
                juce::Justification::centred);

    // Open / Dense scale under the flower
    const float sy = b.getBottom() - 16.0f;
    const float sx0 = c.x - R * 0.9f, sx1 = c.x + R * 0.9f;
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.fillRoundedRectangle (sx0, sy - 1.5f, sx1 - sx0, 3.0f, 1.5f);
    const float mx = c.x + dispBloom * (sx1 - c.x);
    g.setColour (haloC);
    g.fillRoundedRectangle (std::min (c.x, mx), sy - 1.5f, std::abs (mx - c.x), 3.0f, 1.5f);
    g.fillEllipse (mx - 4.0f, sy - 4.0f, 8.0f, 8.0f);
    g.setFont (font (11.0f, true));
    g.setColour (kDim);
    g.drawText ("OPEN", juce::Rectangle<float> (sx0 - 60.0f, sy - 8.0f, 52.0f, 16.0f), juce::Justification::centredRight);
    g.drawText ("DENSE", juce::Rectangle<float> (sx1 + 8.0f, sy - 8.0f, 60.0f, 16.0f), juce::Justification::centredLeft);
}

//==============================================================================
// Spectrum
//==============================================================================
Spectrum::Spectrum (ChengFlowerProcessor& p) : proc (p)
{
    fftData.assign (4096, 0.0f);
    window.resize (2048);
    for (int i = 0; i < 2048; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / 2047.0f);
    smooth.assign (1025, -100.0f);
}

void Spectrum::tick()
{
    const int pos = proc.scopePos.load (std::memory_order_acquire);
    for (int i = 0; i < 2048; ++i)
        fftData[(size_t) i] = proc.scope[(size_t) ((pos - 2048 + i + ChengFlowerProcessor::kScopeSize) & (ChengFlowerProcessor::kScopeSize - 1))]
                              * window[(size_t) i];
    std::fill (fftData.begin() + 2048, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);
    for (int k = 0; k <= 1024; ++k)
    {
        const float db = juce::Decibels::gainToDecibels (fftData[(size_t) k] / 512.0f, -120.0f);
        auto& s = smooth[(size_t) k];
        s = db > s ? s + (db - s) * 0.6f : s + (db - s) * 0.15f;
    }
    for (size_t i = 0; i < cut.size(); ++i)
    {
        cut[i]    += (proc.meterCut[i].load() - cut[i]) * 0.3f;
        excess[i] += (proc.meterExcess[i].load() - excess[i]) * 0.15f;
    }
    repaint();
}

void Spectrum::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (kPanel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (kEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    auto plot = r.reduced (12.0f, 10.0f).withTrimmedBottom (14.0f).withTrimmedTop (16.0f);
    const float w = plot.getWidth(), h = plot.getHeight();
    const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;

    // grid
    g.setFont (font (10.0f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = plot.getX() + freqToX (f, w);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (kDim.withAlpha (0.7f));
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (x - 20.0f, plot.getBottom() + 1.0f, 40.0f, 12.0f), juce::Justification::centred);
    }

    // spectrum (display tilt +4.5 dB/oct around 1 kHz)
    auto yFor = [&] (float db) { return plot.getY() + h * juce::jlimit (0.0f, 1.0f, (-6.0f - db) / 72.0f); };
    juce::Path line, fill;
    bool started = false;
    for (int i = 0; i <= 180; ++i)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) i / 180.0f);
        // 1/6-octave power average (interpolated below the bin resolution)
        const float lo = (float) (f * 0.944 * 2048.0 / fs), hi = (float) (f * 1.059 * 2048.0 / fs);
        float db;
        if (hi - lo < 2.0f)
        {
            const float kf = (float) (f * 2048.0 / fs);
            const int k = juce::jlimit (1, 1023, (int) kf);
            const float frac = juce::jlimit (0.0f, 1.0f, kf - (float) k);
            db = smooth[(size_t) k] + (smooth[(size_t) k + 1] - smooth[(size_t) k]) * frac;
        }
        else
        {
            double acc = 0; int cnt = 0;
            for (int k = juce::jlimit (1, 1024, (int) lo); k <= juce::jlimit (1, 1024, (int) hi); ++k)
            { acc += std::pow (10.0, smooth[(size_t) k] / 10.0); ++cnt; }
            db = (float) (10.0 * std::log10 (acc / std::max (1, cnt) + 1e-14));
        }
        db += 4.5f * std::log2 (f / 1000.0f);
        const float x = plot.getX() + freqToX (f, w), y = yFor (db);
        if (! started) { line.startNewSubPath (x, y); fill.startNewSubPath (x, plot.getBottom()); fill.lineTo (x, y); started = true; }
        else { line.lineTo (x, y); fill.lineTo (x, y); }
    }
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (kRose.withAlpha (0.35f), plot.getX(), plot.getY(),
                                             kMagenta.withAlpha (0.05f), plot.getX(), plot.getBottom(), false));
    g.fillPath (fill);
    g.setColour (kPeach.withAlpha (0.85f));
    g.strokePath (line, juce::PathStrokeType (1.4f));

    // build-up hints + active cuts
    for (int i = 0; i < fl::Unmask::kBands; ++i)
    {
        const float x = plot.getX() + freqToX (fl::Unmask::kFreqs[i], w);
        const float c = -cut[(size_t) i];
        if (c > 0.15f)
        {
            g.setColour (kGold.withAlpha (0.75f));
            g.fillRoundedRectangle (x - 3.0f, plot.getY(), 6.0f, std::min (h * 0.6f, c * 7.0f), 2.0f);
        }
        const float e = excess[(size_t) i];
        if (e > 2.5f)
        {
            const float a = juce::jlimit (0.25f, 1.0f, (e - 2.5f) / 6.0f);
            g.setColour (kAmber.withAlpha (a));
            juce::Path tri;
            tri.addTriangle (x - 5.0f, plot.getY() - 12.0f, x + 5.0f, plot.getY() - 12.0f, x, plot.getY() - 4.0f);
            g.fillPath (tri);
        }
    }
    g.setFont (font (10.5f, true));
    g.setColour (kDim);
    g.drawText ("OUTPUT SPECTRUM", r.reduced (12.0f, 4.0f).removeFromTop (14.0f), juce::Justification::centredLeft);
    auto legend = r.reduced (12.0f, 4.0f).removeFromTop (14.0f);
    g.setColour (kAmber.withAlpha (0.85f));
    g.drawText (juce::CharPointer_UTF8 ("\xe2\x96\xbc BUILD-UP"), legend.removeFromRight (80.0f), juce::Justification::centredRight);
    legend.removeFromRight (14.0f);
    g.setColour (kGold.withAlpha (0.85f));
    g.drawText (juce::CharPointer_UTF8 ("\xe2\x96\x8c CLEANING"), legend.removeFromRight (80.0f), juce::Justification::centredRight);
}

//==============================================================================
void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto bar = r.removeFromLeft (r.getWidth() * 0.5f - 2.0f);
    auto gr = r.withTrimmedLeft (4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (bar, 3.0f);
    g.fillRoundedRectangle (gr, 3.0f);
    const float lv = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
    auto fillR = bar.withTop (bar.getBottom() - bar.getHeight() * lv);
    g.setGradientFill (juce::ColourGradient (kPeach, bar.getX(), bar.getY(), kMagenta, bar.getX(), bar.getBottom(), false));
    g.fillRoundedRectangle (fillR, 3.0f);
    const float gv = juce::jlimit (0.0f, 1.0f, -grDb / 12.0f);
    g.setColour (kGold);
    g.fillRoundedRectangle (gr.withHeight (gr.getHeight() * gv), 3.0f);
}

//==============================================================================
// Editor
//==============================================================================
ChengFlowerEditor::Content::Content (ChengFlowerProcessor& p)
    : proc (p), s (p.apvts),
      mode (s, "mode", { "Track", "Master" }, kRose),
      sweetChar (s, "sweetChar", { "Pentode", "Tape", "Hot Tube" }, kSweetC),
      autoGain (s, "autoGain", "MATCH", kFocusC),
      bypass (s, "bypass", "BYPASS", kLowC),
      limiter (s, "limiter", "LIMIT", kGold),
      flower (p),
      shine (s, "shine", "SHINE", kShineC), thick (s, "thick", "THICK", kThickC), low (s, "low", "LOW END", kLowC),
      sweet (s, "sweet", "SWEET", kSweetC),
      focus (s, "focus", "FOCUS", kFocusC), harsh (s, "deharsh", "DE-HARSH", kHarshC), width (s, "width", "WIDTH", kWidthC),
      color (s, "color", "COLOR", kColorC),
      output (s, "output", "OUTPUT", kOutC), ceiling (s, "ceiling", "CEILING", kOutC), mix (s, "mix", "MIX", kOutC),
      spectrum (p)
{
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &mode, &sweetChar, &autoGain, &bypass, &limiter, &flower, &shine, &thick, &low, &sweet, &focus, &harsh, &width, &color,
             &output, &ceiling, &mix, &spectrum, &meter })
        addAndMakeVisible (c);
}

void ChengFlowerEditor::Content::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (kBgTop, 0, 0, kBgBot, 0, (float) getHeight(), false));
    g.fillAll();

    // header
    g.setColour (kText);
    g.setFont (font (13.0f, true));
    g.drawText ("C H E N G", 26, 18, 120, 24, juce::Justification::centredLeft);
    g.setFont (font (28.0f, true));
    g.setColour (kRose);
    g.drawText ("Flower", 112, 12, 140, 34, juce::Justification::centredLeft);
    {
        const juce::Rectangle<float> badge (205.0f, 20.0f, 30.0f, 18.0f);
        g.setColour (kGold.withAlpha (0.9f));
        g.drawRoundedRectangle (badge, 9.0f, 1.2f);
        g.setFont (font (11.0f, true));
        g.drawText ("v3", badge, juce::Justification::centred);
    }
    g.setColour (kEdge);
    g.drawHorizontalLine (58, 20.0f, (float) getWidth() - 20.0f);

    g.setFont (font (11.0f));
    g.setColour (kDim);
    g.drawText (infoLine, 260, 18, 330, 24, juce::Justification::centred);

    // output panel
    auto op = juce::Rectangle<float> (712.0f, 542.0f, 248.0f, 164.0f);
    g.setColour (kPanel);
    g.fillRoundedRectangle (op, 10.0f);
    g.setColour (kEdge);
    g.drawRoundedRectangle (op.reduced (0.5f), 10.0f, 1.0f);
}

void ChengFlowerEditor::Content::resized()
{
    bypass.setBounds (600, 17, 90, 26);
    mode.setBounds (700, 17, 150, 26);
    autoGain.setBounds (858, 17, 100, 26);

    flower.setBounds (300, 70, 380, 450);

    const int kw = 104, kh = 106;
    auto place = [&] (juce::Component& k, int cx, int cy) { k.setBounds (cx - kw / 2, cy - kh / 2, kw, kh); };
    place (shine, 215, 125); place (thick, 160, 235); place (low, 160, 345); place (sweet, 215, 455);
    place (focus, 765, 125); place (harsh, 820, 235); place (width, 820, 345); place (color, 765, 455);
    sweetChar.setBounds (96, 512, 238, 22);

    spectrum.setBounds (20, 548, 680, 158);

    const int oy = 556;
    output.setBounds (722, oy, 72, 100);
    ceiling.setBounds (790, oy, 72, 100);
    mix.setBounds (858, oy, 72, 100);
    limiter.setBounds (796, 666, 60, 24);
    meter.setBounds (934, oy + 6, 18, 134);
}

void ChengFlowerEditor::Content::refresh()
{
    flower.tick();
    spectrum.tick();
    mode.refresh();
    sweetChar.refresh();
    meter.db = proc.meterOut.load();
    meter.grDb = proc.meterLimiter.load();

    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
    const float latMs = (float) (proc.getLatencySamples() * 1000.0 / sr);
    juce::String line;
    line << "LATENCY " << juce::String (latMs, 1) << " ms"
         << "    MATCH " << (proc.meterAutoGain.load() >= 0 ? "+" : "") << juce::String (proc.meterAutoGain.load(), 1) << " dB"
         << "    LIMIT " << juce::String (proc.meterLimiter.load(), 1) << " dB";
    if (line != infoLine) { infoLine = line; repaint (260, 18, 330, 24); }
}

ChengFlowerEditor::ChengFlowerEditor (ChengFlowerProcessor& p)
    : AudioProcessorEditor (p), proc (p), content (p)
{
    setLookAndFeel (&lnf);
    content.setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.setBounds (0, 0, baseW, baseH);
    setResizable (true, true);
    setResizeLimits (baseW * 2 / 3, baseH * 2 / 3, baseW * 2, baseH * 2);
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize (baseW, baseH);
    startTimerHz (30);
}

ChengFlowerEditor::~ChengFlowerEditor()
{
    stopTimer();
    content.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void ChengFlowerEditor::paint (juce::Graphics& g) { g.fillAll (kBgBot); }

void ChengFlowerEditor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
}

void ChengFlowerEditor::timerCallback() { content.refresh(); }
