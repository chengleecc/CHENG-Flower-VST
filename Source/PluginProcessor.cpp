#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int kSub = 32;              // control-rate sub-block
    constexpr double kLookaheadMs = 1.5;       // Master-mode limiter look-ahead
    constexpr double kTrackLookaheadMs = 0.5;  // Track mode: short look-ahead so the limiter never hard-clips

    /** "Bold" knob taper: the first half stays fine-grained, the top half accelerates to ~2.2x. */
    inline float bold (float x) noexcept { return x + 0.4f * x * x * x; }
}

//==============================================================================
ChengFlowerProcessor::ChengFlowerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pMode = prm ("mode");     pBloom = prm ("bloom");  pShine = prm ("shine");  pLow = prm ("low");
    pSweet = prm ("sweet");   pSweetChar = prm ("sweetChar"); pFocus = prm ("focus"); pHarsh = prm ("deharsh");
    pWidth = prm ("width");   pAuto = prm ("autoGain"); pOut = prm ("output"); pCeil = prm ("ceiling");
    pLimit = prm ("limiter"); pMix = prm ("mix");      pBypass = prm ("bypass");
    pThick = prm ("thick");   pColor = prm ("color");
    startTimerHz (10);
}

ChengFlowerProcessor::~ChengFlowerProcessor() { stopTimer(); }

juce::AudioProcessorValueTreeState::ParameterLayout ChengFlowerProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> v;
    auto pct = [] (const char* id, const char* name, float lo, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, 100.0f, 0.1f), def,
                                                      AudioParameterFloatAttributes().withLabel ("%"));
    };
    v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", StringArray { "Track", "Master" }, 0));
    v.push_back (pct ("bloom",   "Open / Dense", -100.0f, 0.0f));
    v.push_back (pct ("shine",   "Shine",    0.0f, 0.0f));
    v.push_back (pct ("low",     "Low End",  0.0f, 0.0f));
    v.push_back (pct ("sweet",   "Sweet",    0.0f, 0.0f));
    v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "sweetChar", 1 }, "Sweet Character", StringArray { "Pentode", "Tape", "Hot Tube" }, 0));
    v.push_back (pct ("focus",   "Focus",    0.0f, 0.0f));
    v.push_back (pct ("deharsh", "De-Harsh", 0.0f, 0.0f));
    v.push_back (pct ("width",   "Width",    0.0f, 0.0f));
    v.push_back (pct ("thick",   "Thick",    0.0f, 0.0f));
    v.push_back (pct ("color",   "Color",    0.0f, 15.0f));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "autoGain", 1 }, "Match", false));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                                        AudioParameterFloatAttributes().withLabel ("dB")));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "ceiling", 1 }, "Ceiling", NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -0.3f,
                                                        AudioParameterFloatAttributes().withLabel ("dB")));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "limiter", 1 }, "Limiter", true));
    v.push_back (pct ("mix", "Mix", 0.0f, 100.0f));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return { v.begin(), v.end() };
}

juce::AudioProcessorParameter* ChengFlowerProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

bool ChengFlowerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

int ChengFlowerProcessor::latencyFor (int mode) const
{
    return mode == MasterMode ? osLatMaster + (int) std::round (kLookaheadMs * 0.001 * fs)
                              : osLatTrack + (int) std::round (kTrackLookaheadMs * 0.001 * fs);
}

void ChengFlowerProcessor::updateLatency()
{
    const int mode = (int) pMode->load();
    const int lat = latencyFor (mode);
    if (lat != getLatencySamples())
        setLatencySamples (lat);
}

//==============================================================================
void ChengFlowerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    maxBlock = std::max (64, samplesPerBlock);

    osTrack = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    osMaster = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    osTrack->initProcessing ((size_t) maxBlock);
    osMaster->initProcessing ((size_t) maxBlock);
    osLatTrack  = (int) std::round (osTrack->getLatencyInSamples());
    osLatMaster = (int) std::round (osMaster->getLatencyInSamples());

    unmask.prepare (fs); bloom.prepare (fs); lowEnd.prepare (fs); focus.prepare (fs); shine.prepare (fs);
    depth.prepare (fs);
    analogTrack.prepare (fs, 2); analogMaster.prepare (fs, 4);
    autoGain.prepare (fs);
    const int maxLa = (int) std::ceil (kLookaheadMs * 0.001 * fs) + 4;
    limiter.prepare (fs, maxLa);
    for (auto& h : widthHp) { h.c = fl::design::highpass1 (fs, 300.0); h.reset(); }

    const int maxLat = std::max (osLatTrack, osLatMaster) + maxLa + 16;
    for (int c = 0; c < 2; ++c) { dryMix[c].prepare (maxLat); dryBypass[c].prepare (maxLat); }

    work.setSize (2, maxBlock);
    for (auto& d : dry) d.assign ((size_t) maxBlock, 0.0f);
    mono.assign ((size_t) maxBlock, 0.0f);
    drive.assign ((size_t) maxBlock, 0.0f);
    norm.assign ((size_t) maxBlock, 0.0f);
    mid.assign ((size_t) kSub, 0.0f);
    side.assign ((size_t) kSub, 0.0f);

    for (auto* s : { &smBloom, &smShine, &smLow, &smSweet, &smFocus, &smHarsh, &smWidth, &smThick, &smColor, &smMix })
        s->reset (fs, 0.05);
    smOut.reset (fs, 0.05); smBypass.reset (fs, 0.02);
    smBloom.setCurrentAndTargetValue (pBloom->load() * 0.01f);
    smShine.setCurrentAndTargetValue (pShine->load() * 0.01f);
    smLow.setCurrentAndTargetValue (pLow->load() * 0.01f);
    smSweet.setCurrentAndTargetValue (pSweet->load() * 0.01f);
    smFocus.setCurrentAndTargetValue (pFocus->load() * 0.01f);
    smHarsh.setCurrentAndTargetValue (pHarsh->load() * 0.01f);
    smWidth.setCurrentAndTargetValue (pWidth->load() * 0.01f);
    smThick.setCurrentAndTargetValue (pThick->load() * 0.01f);
    smColor.setCurrentAndTargetValue (pColor->load() * 0.01f);
    smMix.setCurrentAndTargetValue (pMix->load() * 0.01f);
    smOut.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    smBypass.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);

    curMode = -1;
    switchMode ((int) pMode->load());
    fadeIn = 1.0f; fadeStep = 0.0f;
    setLatencySamples (latencyFor (curMode));
}

void ChengFlowerProcessor::switchMode (int newMode)
{
    curMode = newMode;
    osTrack->reset(); osMaster->reset();
    analogTrack.reset(); analogMaster.reset();
    const int osLat = curMode == MasterMode ? osLatMaster : osLatTrack;
    limiter.setLookahead ((int) std::round ((curMode == MasterMode ? kLookaheadMs : kTrackLookaheadMs) * 0.001 * fs));
    for (int c = 0; c < 2; ++c)
    {
        dryMix[c].reset(); dryMix[c].setDelay (osLat);
        dryBypass[c].reset(); dryBypass[c].setDelay (osLat + limiter.getLatency());
    }
    // short fade-in to hide the state reset
    fadeIn = 0.0f; fadeStep = (float) (1.0 / (0.02 * fs));
}

//==============================================================================
void ChengFlowerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int total = buffer.getNumSamples();
    if (numIn == 0) return;

    const int mode = (int) pMode->load();
    if (mode != curMode) switchMode (mode);

    for (int pos = 0; pos < total; pos += maxBlock)
    {
        const int n = std::min (maxBlock, total - pos);
        for (int c = 0; c < 2; ++c)
            work.copyFrom (c, 0, buffer, std::min (c, numIn - 1), pos, n);
        processChunk (n);
        for (int c = 0; c < numOut; ++c)
        {
            if (numOut == 1)
            {
                auto* d = buffer.getWritePointer (0, pos);
                const auto* l = work.getReadPointer (0);
                const auto* r = work.getReadPointer (1);
                for (int s = 0; s < n; ++s) d[s] = 0.5f * (l[s] + r[s]);
            }
            else
                buffer.copyFrom (c, pos, work, std::min (c, 1), 0, n);
        }
    }
}

void ChengFlowerProcessor::processChunk (int n)
{
    float* L = work.getWritePointer (0);
    float* R = work.getWritePointer (1);

    float inPeak = 0.0f;
    for (int s = 0; s < n; ++s)
    {
        dry[0][(size_t) s] = L[s]; dry[1][(size_t) s] = R[s];
        mono[(size_t) s] = 0.5f * (L[s] + R[s]);
        inPeak = std::max (inPeak, std::max (std::abs (L[s]), std::abs (R[s])));
    }
    autoGain.pushInput (L, R, n);
    shine.analyse (mono.data(), n);

    smBloom.setTargetValue (pBloom->load() * 0.01f);
    smShine.setTargetValue (pShine->load() * 0.01f);
    smLow.setTargetValue (pLow->load() * 0.01f);
    smSweet.setTargetValue (pSweet->load() * 0.01f);
    smFocus.setTargetValue (pFocus->load() * 0.01f);
    smHarsh.setTargetValue (pHarsh->load() * 0.01f);
    smWidth.setTargetValue (pWidth->load() * 0.01f);
    smThick.setTargetValue (pThick->load() * 0.01f);
    smColor.setTargetValue (pColor->load() * 0.01f);
    smMix.setTargetValue (pMix->load() * 0.01f);
    smOut.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    smBypass.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);

    float lowGr = 0.0f, bloomAct = 0.0f; int subs = 0;
    for (int o = 0; o < n; o += kSub)
    {
        const int k = std::min (kSub, n - o);
        const float bRaw = smBloom.skip (k);
        const float b  = bRaw < 0.0f ? -bold (-bRaw) : bold (bRaw);
        const float sh = bold (smShine.skip (k)), lo = bold (smLow.skip (k));
        const float fo = std::min (1.6f, bold (smFocus.skip (k))), hr = bold (smHarsh.skip (k)), wd = bold (smWidth.skip (k));
        const float open = std::max (0.0f, -b);
        float* l = L + o; float* r = R + o;

        // 1. dynamic unmasking (Open + De-Harsh)
        unmask.analyse (l, r, k, open, hr);
        unmask.process (l, r, k);

        // 2. Open <-> Dense transient / density
        bloom.process (l, r, k, b);

        // 3. M/S stage: low end, focus, width
        for (int s = 0; s < k; ++s) { mid[(size_t) s] = 0.5f * (l[s] + r[s]); side[(size_t) s] = 0.5f * (l[s] - r[s]); }
        lowEnd.process (mid.data(), side.data(), k, lo);
        focus.process (mid.data(), side.data(), k, fo);
        depth.process (mid.data(), side.data(), k, std::min (open, 1.6f));
        const float widen = 0.9f * std::min (1.8f, wd + 0.3f * open);
        for (int s = 0; s < k; ++s)
        {
            const float sHi = widthHp[0].process (side[(size_t) s]);
            const float sv = side[(size_t) s] + widen * sHi;
            l[s] = mid[(size_t) s] + sv;
            r[s] = mid[(size_t) s] - sv;
        }

        // 4. melodic shine
        shine.process (l, r, k, sh);

        lowGr = std::min (lowGr, lowEnd.grDb);
        bloomAct += bloom.activityDb; ++subs;
    }

    // 5. analog stage at the oversampled rate: Color, Dense drive, Thick, Sweet
    //    (always run through the oversampler so the latency stays constant)
    {
        fl::Analog::Params ap;
        const float sw = smSweet.skip (n);
        const float bNow = smBloom.getCurrentValue();
        ap.sweet = sw;
        ap.sweetChar = (int) pSweetChar->load();
        ap.color = smColor.skip (n) + 0.35f * bold (std::max (0.0f, -bNow)); // Open also adds tube bloom
        ap.dense = bold (std::max (0.0f, bNow));
        ap.thick = bold (smThick.skip (n));
        auto& os = curMode == MasterMode ? *osMaster : *osTrack;
        auto& analog = curMode == MasterMode ? analogMaster : analogTrack;
        const int factor = curMode == MasterMode ? 4 : 2;
        analog.computeLevels (L, R, n, sw, drive.data(), norm.data());
        juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 2, (size_t) n);
        auto up = os.processSamplesUp (block);
        float* chs[2] = { up.getChannelPointer (0), up.getChannelPointer (1) };
        analog.process (chs, (int) up.getNumSamples(), factor, drive.data(), norm.data(), ap);
        os.processSamplesDown (block);
        analog.postDc (L, R, n);
    }

    // 6. dry/wet (latency-matched dry)
    for (int s = 0; s < n; ++s)
    {
        const float m = smMix.getNextValue();
        const float dl = dryMix[0].process (dry[0][(size_t) s]);
        const float dr = dryMix[1].process (dry[1][(size_t) s]);
        L[s] = dl + m * (L[s] - dl);
        R[s] = dr + m * (R[s] - dr);
    }

    // 7. loudness match, output gain, limiter
    autoGain.process (L, R, n, pAuto->load() > 0.5f);
    for (int s = 0; s < n; ++s) { const float g = smOut.getNextValue(); L[s] *= g; R[s] *= g; }
    const bool limOn = pLimit->load() > 0.5f;
    limiter.process (L, R, n, limOn ? pCeil->load() : 24.0f, curMode == MasterMode);

    // 8. latency-compensated bypass + mode-switch fade
    float outPeak = 0.0f;
    int sp = scopePos.load (std::memory_order_relaxed);
    for (int s = 0; s < n; ++s)
    {
        const float bp = smBypass.getNextValue();
        const float dl = dryBypass[0].process (dry[0][(size_t) s]);
        const float dr = dryBypass[1].process (dry[1][(size_t) s]);
        float l = L[s] + bp * (dl - L[s]);
        float r = R[s] + bp * (dr - R[s]);
        if (fadeIn < 1.0f) { fadeIn = std::min (1.0f, fadeIn + fadeStep); l *= fadeIn; r *= fadeIn; }
        L[s] = l; R[s] = r;
        outPeak = std::max (outPeak, std::max (std::abs (l), std::abs (r)));
        scope[(size_t) sp] = 0.5f * (l + r);
        sp = (sp + 1) & (kScopeSize - 1);
    }
    scopePos.store (sp, std::memory_order_release);

    // meters
    auto fall = [] (std::atomic<float>& m, float db) { m.store (std::max (db, m.load() - 1.5f)); };
    fall (meterIn, juce::Decibels::gainToDecibels (inPeak, -100.0f));
    fall (meterOut, juce::Decibels::gainToDecibels (outPeak, -100.0f));
    meterAutoGain.store (autoGain.gainDb);
    meterLimiter.store (limiter.grDb);
    meterBloom.store (subs > 0 ? bloomAct / (float) subs : 0.0f);
    meterLow.store (lowGr);
    meterFocus.store (focus.duckDb);
    meterShineGate.store (shine.gate);
    meterTonal.store (shine.tonal);
    for (int i = 0; i < fl::Unmask::kBands; ++i)
    {
        meterCut[(size_t) i].store (unmask.cutDb[(size_t) i]);
        meterExcess[(size_t) i].store (unmask.excessDb[(size_t) i]);
    }
}

//==============================================================================
void ChengFlowerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ChengFlowerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ChengFlowerProcessor::createEditor() { return new ChengFlowerEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChengFlowerProcessor(); }
