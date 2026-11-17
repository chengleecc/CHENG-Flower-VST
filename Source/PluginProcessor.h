#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "FlowerDSP.h"

class ChengFlowerProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    ChengFlowerProcessor();
    ~ChengFlowerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CHENG Flower v2"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    enum Mode { TrackMode = 0, MasterMode = 1 };
    /** Total latency the given mode reports to the host. */
    int latencyFor (int mode) const;
    /** Called on the message thread (timer) - also callable directly by tests. */
    void updateLatency();

    // ---- metering (read by the editor) ----
    std::atomic<float> meterIn { -100.0f }, meterOut { -100.0f };
    std::atomic<float> meterAutoGain { 0.0f }, meterLimiter { 0.0f };
    std::atomic<float> meterBloom { 0.0f }, meterLow { 0.0f }, meterFocus { 0.0f };
    std::atomic<float> meterShineGate { 0.0f }, meterTonal { 0.5f };
    std::array<std::atomic<float>, fl::Unmask::kBands> meterCut {}, meterExcess {};

    // ---- output scope for the spectrum display ----
    static constexpr int kScopeSize = 4096;
    std::array<float, kScopeSize> scope {};
    std::atomic<int> scopePos { 0 };

private:
    void timerCallback() override { updateLatency(); }
    void processChunk (int n);
    void switchMode (int newMode);

    std::atomic<float>* prm (const char* id) const { return apvts.getRawParameterValue (id); }

    std::atomic<float> *pMode, *pBloom, *pShine, *pLow, *pSweet, *pSweetChar, *pFocus, *pHarsh, *pWidth,
                       *pThick, *pColor, *pAuto, *pOut, *pCeil, *pLimit, *pMix, *pBypass;

    double fs = 48000.0;
    int maxBlock = 512;
    int curMode = -1;

    fl::Unmask unmask;
    fl::Bloom bloom;
    fl::LowEnd lowEnd;
    fl::Focus focus;
    fl::Shine shine;
    fl::Depth3D depth;
    fl::Analog analogTrack, analogMaster;
    fl::AutoGain autoGain;
    fl::Limiter limiter;
    fl::Biquad widthHp[2];

    std::unique_ptr<juce::dsp::Oversampling<float>> osTrack, osMaster;
    int osLatTrack = 0, osLatMaster = 0;

    fl::Delay dryMix[2], dryBypass[2];

    juce::AudioBuffer<float> work;
    std::vector<float> dry[2], mono, drive, norm, mid, side;

    juce::SmoothedValue<float> smBloom, smShine, smLow, smSweet, smFocus, smHarsh, smWidth, smThick, smColor, smMix, smOut, smBypass;
    float fadeIn = 1.0f, fadeStep = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChengFlowerProcessor)
};
