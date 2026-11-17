#pragma once
// CHENG Flower - processing modules (original code).
//
// Every module is program-adaptive: thresholds follow the long-term level of the
// material, so one knob behaves the same on a quiet vocal track and a loud master.
#include "DSP.h"
#include <array>
#include <vector>
#include <memory>

namespace juce { namespace dsp { class FFT; } }

namespace fl
{
/** Long-term power average that freezes during silence. */
struct LongTerm
{
    float c = 0.0f, v = 1.0e-3f; // start around -30 dBFS
    void setup (double rate, double seconds) { c = tauCoef (seconds, rate); }
    inline void push (float power) noexcept
    {
        if (power > 1.0e-7f) v = power + (v - power) * c;
    }
    float db() const noexcept { return 10.0f * std::log10 (std::max (v, 1.0e-12f)); }
};

//==============================================================================
/** Dynamic resonance / build-up suppressor ("Open" + "De-Harsh").
    12 constant-Q detector bands compare each band with its neighbours and with a
    gentle reference tilt; only bands that stick out get a dynamic peaking cut. */
struct Unmask
{
    static constexpr int kBands = 12;
    static constexpr float kFreqs[kBands] = { 180, 260, 380, 550, 800, 1150, 1650, 2400, 3400, 4800, 6800, 9500 };

    void prepare (double fs);
    void reset();
    /** Runs detectors over a sub-block (mono sum), updates cut amounts. */
    void analyse (const float* l, const float* r, int n, float openAmt, float harshAmt);
    void process (float* l, float* r, int n) noexcept;

    std::array<float, kBands> cutDb {};     // current applied cut (<= 0)
    std::array<float, kBands> excessDb {};  // how much each band sticks out (for UI hints)

private:
    double fs = 48000.0;
    std::array<Biquad, kBands> det;
    std::array<PowerEnv, kBands> env;
    std::array<Biquad, kBands> fil[2];
    std::array<float, kBands> lastCut {};
    float atkC = 0.0f, relC = 0.0f;
};

//==============================================================================
/** "Open <-> Dense": 3-band transient / density processor.
    Open: sharper attacks, shorter sustains (space between notes).
    Dense: upward + downward compression around the band's own average, softer spikes, longer sustain. */
struct Bloom
{
    void prepare (double fs);
    void reset();
    /** bloom: -1 (open) .. +1 (dense). */
    void process (float* l, float* r, int n, float bloom) noexcept;
    float activityDb = 0.0f; // mean absolute gain movement (UI)

private:
    struct Band
    {
        PeakEnv fast, slow; PowerEnv rms; LongTerm lt;
        float gDb = 0.0f;
    };
    double fs = 48000.0;
    LR4 x1[2], x2[2], ap2[2];
    Band band[3];
    float smC = 0.0f, eng = 0.0f, engStep = 0.0f;
};

//==============================================================================
/** Tight low end: mono below 120 Hz, adaptive low-band compression, punch bell, upper bass harmonics. */
struct LowEnd
{
    void prepare (double fs);
    void reset();
    void process (float* mid, float* side, int n, float amt) noexcept;
    float grDb = 0.0f;

private:
    double fs = 48000.0;
    LR4 xm, xs;
    Biquad punch, hHp1, hHp2, hLp;
    PowerEnv env; LongTerm lt;
    float lastAmt = -1.0f, gDb = 0.0f, smC = 0.0f, eng = 0.0f, engStep = 0.0f;
};

//==============================================================================
/** Vocal focus: side channel presence ducks whenever the centre presence is active. */
struct Focus
{
    void prepare (double fs);
    void reset();
    void process (float* mid, float* side, int n, float amt) noexcept;
    float duckDb = 0.0f;

private:
    double fs = 48000.0;
    Biquad det, sideBell, midBell;
    PowerEnv env; LongTerm lt;
    float lastAmt = -1.0f, curDuck = 0.0f, lastDuck = 1.0e9f;
};

//==============================================================================
/** Melodic shine: even-harmonic exciter + air shelf, gated by a tonality / sibilance analysis. */
struct Shine
{
    Shine();
    ~Shine();
    void prepare (double fs);
    void reset();
    /** Feed the (pre-processing) mono input for analysis. */
    void analyse (const float* mono, int n);
    void process (float* l, float* r, int n, float amt) noexcept;

    float tonal = 0.5f, sib = 0.0f, gate = 0.0f;

private:
    void runFft();
    double fs = 48000.0;
    Biquad bHp[2], bLp[2], hHp1[2], hHp2[2], air[2];
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> ring, work, window;
    int ringPos = 0, sinceFft = 0;
    float tonalTarget = 0.5f, sibTarget = 0.0f;
    float lastAir = 1.0e9f;
};

//==============================================================================
/** Decorrelated "3D" side injection used by Open (Oven-style size / depth). */
struct Depth3D
{
    void prepare (double fs);
    void reset();
    /** Adds a decorrelated copy of the centre highs to the side channel. mono-compatible (side cancels in mono). */
    void process (const float* mid, float* side, int n, float amt) noexcept;

private:
    double fs = 48000.0;
    Biquad hp, shelf;
    float ap[4] {}, z[4] {};
    float lastShelf = -1.0f;
};

//==============================================================================
/** Everything that runs at the oversampled rate: the analog colour of CHENG.
      Color   - transformer iron + class-A amp, with left/right component tolerance (always-on character)
      Dense   - parallel pentode + triode drive and a soft "enhance" clip (glossy, forward, loud)
      Thick   - inflator curve + Neve-style low iron + fat drive at the top of the knob
      Sweet   - Pentode / Tape / Hot Tube saturation, can get dirty                              */
struct Analog
{
    enum SweetChar { Pentode = 0, Tape = 1, HotTube = 2 };
    struct Params { float color = 0, dense = 0, thick = 0, sweet = 0; int sweetChar = 0; };

    void prepare (double baseFs, int factor);
    void reset();
    /** Base-rate level analysis: sweet drive and peak normalisation per sample. */
    void computeLevels (const float* l, const float* r, int n, float sweetAmt, float* sweetDrive, float* norm) noexcept;
    void process (float* const* ch, int numOs, int factor, const float* sweetDrive, const float* norm, const Params& p) noexcept;
    void postDc (float* l, float* r, int n) noexcept;

private:
    struct OnePole { float a = 0.0f, z = 0.0f; inline float lp (float x) noexcept { z += a * (x - z); return z; } };
    double fs = 48000.0, osFs = 96000.0;
    PowerEnv fastEnv; LongTerm lt; float pk = 0.1f, pkRel = 0.0f;
    OnePole iron[2], hfSoft[2], thickIron[2];
    Biquad preEmph[2], deEmph[2], bump[2], air[2], dc[2];
    Biquad emDe[2], emPre[2];   // highs are lowered before the colour/dense/thick curves and restored after (less harsh)
    float tapePrev[2] {}, hotEnv[2] {}, hotA = 0.0f, hotR = 0.0f;
    float lastBump = -1.0f, lastAir = -1.0f, lastSoft = -1.0f;
    // tolerance between the two channels (TMT-style): drive, bias, iron corner, HF corner
    static constexpr float tolDrive[2] = { 1.0f, 1.07f };
    static constexpr float tolBias[2]  = { 0.012f, -0.010f };
    static constexpr float tolIron[2]  = { 1.0f, 1.06f };
    static constexpr float tolHf[2]    = { 1.0f, 0.93f };
};

//==============================================================================
/** Loudness-matching gain: keeps output loudness equal to input loudness (K-weighted, ~2 s). */
struct AutoGain
{
    void prepare (double fs);
    void reset();
    void pushInput (const float* l, const float* r, int n) noexcept;
    /** Measures the processed signal and applies matching gain in place. */
    void process (float* l, float* r, int n, bool enabled) noexcept;
    float gainDb = 0.0f;

private:
    struct KW { Biquad hp, shelf; inline float run (float x) noexcept { return shelf.process (hp.process (x)); } };
    double fs = 48000.0;
    KW kin, kout;
    float inP = 1.0e-4f, outP = 1.0e-4f, c = 0.0f, gSm = 1.0f, gC = 0.0f;
};

//==============================================================================
/** Look-ahead peak limiter with an approximate inter-sample (true-peak) detector. */
struct Limiter
{
    void prepare (double fs, int maxLookahead);
    void reset();
    void setLookahead (int samples);
    void process (float* l, float* r, int n, float ceilingDb, bool truePeak) noexcept;
    int getLatency() const noexcept { return la; }
    float grDb = 0.0f;

private:
    double fs = 48000.0;
    int la = 0;
    std::vector<float> gReq, gEnv; int gPos = 0;
    Delay dl, dr;
    float hist[2][3] {};
    float env = 1.0f, relC = 0.0f, boxSum = 0.0f;
};
} // namespace fl
