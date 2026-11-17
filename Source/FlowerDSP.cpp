#include "FlowerDSP.h"
#include <juce_dsp/juce_dsp.h>

namespace fl
{
namespace
{
    inline float fastDbToGain (float db) noexcept { return std::exp (db * 0.11512925465f); }
    inline float powDb (float p) noexcept { return 10.0f * std::log10 (std::max (p, 1.0e-12f)); }
}

constexpr float Unmask::kFreqs[Unmask::kBands];

//==============================================================================
// Unmask
//==============================================================================
void Unmask::prepare (double sampleRate)
{
    fs = sampleRate;
    for (int i = 0; i < kBands; ++i)
    {
        det[(size_t) i].c = design::bandpass (fs, kFreqs[i], 2.0);
        env[(size_t) i].setup (fs, 5.0f, 60.0f);
    }
    reset();
}

void Unmask::reset()
{
    for (int i = 0; i < kBands; ++i)
    {
        det[(size_t) i].reset();
        env[(size_t) i].v = 1.0e-9f;
        for (auto& f : fil) { f[(size_t) i].reset(); f[(size_t) i].c = BiquadCoeffs(); }
        cutDb[(size_t) i] = 0.0f; lastCut[(size_t) i] = 0.0f; excessDb[(size_t) i] = 0.0f;
    }
}

void Unmask::analyse (const float* l, const float* r, int n, float openAmt, float harshAmt)
{
    for (int s = 0; s < n; ++s)
    {
        const float m = 0.5f * (l[s] + r[s]);
        for (int i = 0; i < kBands; ++i)
            env[(size_t) i].push (det[(size_t) i].process (m));
    }

    // levels relative to a gentle reference tilt (flat to 500 Hz, -3 dB/oct above)
    std::array<float, kBands> lvl {};
    std::array<bool, kBands> live {};
    float mean = 0.0f; int nLive = 0;
    for (int i = 0; i < kBands; ++i)
    {
        const float f = kFreqs[i];
        const float tilt = f <= 500.0f ? 0.0f : -3.0f * std::log2 (f / 500.0f);
        const float raw = env[(size_t) i].db();
        live[(size_t) i] = raw > -75.0f && f < fs * 0.45;
        lvl[(size_t) i] = raw - tilt;
        if (live[(size_t) i]) { mean += lvl[(size_t) i]; ++nLive; }
    }
    if (nLive > 0) mean /= (float) nLive;

    const float cA = (float) std::exp (-(double) n / (0.010 * fs));
    const float cR = (float) std::exp (-(double) n / (0.120 * fs));

    for (int i = 0; i < kBands; ++i)
    {
        float target = 0.0f, over = 0.0f;
        if (live[(size_t) i])
        {
            float acc = 0.0f, wsum = 0.0f;
            for (int d = -2; d <= 2; ++d)
            {
                const int j = i + d;
                if (d == 0 || j < 0 || j >= kBands || ! live[(size_t) j]) continue;
                const float w = std::abs (d) == 1 ? 0.35f : 0.15f;
                acc += w * lvl[(size_t) j]; wsum += w;
            }
            const float local = wsum > 0.0f ? lvl[(size_t) i] - acc / wsum : 0.0f;
            const float broad = lvl[(size_t) i] - mean;

            const float overO = std::max (0.0f, local - 2.0f) + 0.5f * std::max (0.0f, broad - 4.0f);
            const float overH = std::max (0.0f, local - 1.5f) + 0.5f * std::max (0.0f, broad - 3.0f);
            over = overO;

            const float f = kFreqs[i];
            const float openW  = f < 1300.0f ? 1.0f : (f < 4000.0f ? 0.55f : 0.2f);
            const float harshW = f < 1800.0f ? 0.0f : (f < 2500.0f ? 0.5f : 1.0f);

            const float cutO = std::min (overO * 1.3f, 6.0f * openW * openAmt);
            const float cutH = std::min (overH * 0.9f, 7.0f * harshW * harshAmt);
            target = -std::min (12.0f, cutO + cutH);
        }
        excessDb[(size_t) i] = over;

        auto& c = cutDb[(size_t) i];
        c = target + (c - target) * (target < c ? cA : cR);

        if (std::abs (c - lastCut[(size_t) i]) > 0.02f)
        {
            const auto co = std::abs (c) < 0.01f ? BiquadCoeffs() : design::peaking (fs, kFreqs[i], 1.6, c);
            fil[0][(size_t) i].c = fil[1][(size_t) i].c = co;
            lastCut[(size_t) i] = c;
        }
    }
}

void Unmask::process (float* l, float* r, int n) noexcept
{
    for (int i = 0; i < kBands; ++i)
    {
        if (std::abs (lastCut[(size_t) i]) < 0.01f) continue; // identity coefficients
        auto& fl0 = fil[0][(size_t) i];
        auto& fl1 = fil[1][(size_t) i];
        for (int s = 0; s < n; ++s) { l[s] = fl0.process (l[s]); r[s] = fl1.process (r[s]); }
    }
}

//==============================================================================
// Bloom
//==============================================================================
void Bloom::prepare (double sampleRate)
{
    fs = sampleRate;
    for (int c = 0; c < 2; ++c) { x1[c].setup (fs, 220.0); x2[c].setup (fs, 2800.0); ap2[c].setup (fs, 2800.0); }
    const float fa[3] = { 2.0f, 0.3f, 0.2f },  fr[3] = { 50.0f, 20.0f, 15.0f };
    const float sa[3] = { 40.0f, 20.0f, 15.0f }, sr[3] = { 400.0f, 300.0f, 250.0f };
    for (int b = 0; b < 3; ++b)
    {
        band[b].fast.setup (fs, fa[b], fr[b]);
        band[b].slow.setup (fs, sa[b], sr[b]);
        band[b].rms.setup (fs, 5.0f, 80.0f);
        band[b].lt.setup (fs, 2.0);
    }
    smC = msToCoef (5.0f, fs);
    engStep = (float) (1.0 / (0.02 * fs));
    reset();
}

void Bloom::reset()
{
    for (int c = 0; c < 2; ++c) { x1[c].reset(); x2[c].reset(); ap2[c].reset(); }
    for (auto& b : band) { b.fast.v = b.slow.v = 0.0f; b.rms.v = 1.0e-9f; b.lt.v = 1.0e-3f; b.gDb = 0.0f; }
    eng = 0.0f;
}

void Bloom::process (float* l, float* r, int n, float bloom) noexcept
{
    const float o = std::max (0.0f, -bloom), dn = std::max (0.0f, bloom);
    const float wT[3] = { 0.7f, 1.0f, 0.9f }, wD[3] = { 0.85f, 1.0f, 0.85f };
    float act = 0.0f;
    const float engTarget = (o > 0.0005f || dn > 0.0005f) ? 1.0f : 0.0f;

    for (int s = 0; s < n; ++s)
    {
        eng = engTarget > eng ? std::min (1.0f, eng + engStep) : std::max (0.0f, eng - engStep);
        float bl[3], br[3], rest;
        x1[0].split (l[s], bl[0], rest); x2[0].split (rest, bl[1], bl[2]); bl[0] = ap2[0].allpass (bl[0]);
        x1[1].split (r[s], br[0], rest); x2[1].split (rest, br[1], br[2]); br[0] = ap2[1].allpass (br[0]);

        float outL = 0.0f, outR = 0.0f;
        for (int b = 0; b < 3; ++b)
        {
            auto& B = band[b];
            const float d = std::max (std::abs (bl[b]), std::abs (br[b]));
            const float fa = B.fast.push (d), sl = B.slow.push (d);
            B.rms.push (d);
            B.lt.push (d * d);

            float g = 0.0f;
            if (o > 0.0f || dn > 0.0f)
            {
                const float ratioDb = std::clamp (20.0f * std::log10 ((fa + 1.0e-6f) / (sl + 1.0e-6f)), -18.0f, 18.0f);
                if (o > 0.0f)
                    g += std::clamp (0.4f * o * ratioDb, -12.0f, 9.0f) * wT[b];
                if (dn > 0.0f)
                {
                    g += -0.25f * dn * std::max (ratioDb, 0.0f) + 0.15f * dn * std::max (-ratioDb, 0.0f);
                    // level: RMS, but jumps instantly with the peak follower so onsets are never lifted
                    const float envDb = std::max (B.rms.db(), 20.0f * std::log10 (fa + 1.0e-9f) - 3.0f), ltDb = B.lt.db();
                    const float taper = std::clamp ((envDb - (ltDb - 40.0f)) / 10.0f, 0.0f, 1.0f);
                    const float up   =  std::clamp ((ltDb - 3.0f) - envDb, 0.0f, 12.0f) * 0.55f * dn * taper;
                    const float down = -std::clamp (envDb - (ltDb + 1.0f), 0.0f, 12.0f) * 0.5f * dn;
                    g += (up + down) * wD[b];
                }
                g = std::clamp (g, -14.0f, 10.0f);
            }
            B.gDb = g < B.gDb ? g : g + (B.gDb - g) * smC; // gain reductions act instantly
            const float lin = fastDbToGain (B.gDb);
            outL += bl[b] * lin; outR += br[b] * lin;
            act += std::abs (B.gDb);
        }
        l[s] += eng * (outL - l[s]);
        r[s] += eng * (outR - r[s]);
    }
    activityDb = n > 0 ? act / (3.0f * (float) n) : 0.0f;
}

//==============================================================================
// LowEnd
//==============================================================================
void LowEnd::prepare (double sampleRate)
{
    fs = sampleRate;
    xm.setup (fs, 120.0); xs.setup (fs, 120.0);
    hHp1.c = hHp2.c = design::highpass (fs, 150.0, 0.7071);
    hLp.c = design::lowpass (fs, 600.0, 0.7071);
    env.setup (fs, 12.0f, 140.0f);
    lt.setup (fs, 2.0);
    smC = msToCoef (3.0f, fs);
    engStep = (float) (1.0 / (0.02 * fs));
    reset();
}

void LowEnd::reset()
{
    xm.reset(); xs.reset(); punch.reset(); hHp1.reset(); hHp2.reset(); hLp.reset();
    env.v = 1.0e-9f; lt.v = 1.0e-3f; gDb = 0.0f; lastAmt = -1.0f; eng = 0.0f;
}

void LowEnd::process (float* mid, float* side, int n, float amt) noexcept
{
    if (std::abs (amt - lastAmt) > 0.002f)
    {
        punch.c = amt < 0.001f ? BiquadCoeffs() : design::peaking (fs, 60.0, 0.8, 3.5 * amt);
        lastAmt = amt;
    }
    const float ratio = 1.0f + 3.0f * amt;
    const float slope = 1.0f - 1.0f / ratio;
    float minGr = 0.0f;
    const float engTarget = amt > 0.0005f ? 1.0f : 0.0f;

    for (int s = 0; s < n; ++s)
    {
        eng = engTarget > eng ? std::min (1.0f, eng + engStep) : std::max (0.0f, eng - engStep);
        float ml, mh, sl, sh;
        xm.split (mid[s], ml, mh);
        xs.split (side[s], sl, sh);
        side[s] += eng * ((sh + (1.0f - std::min (amt, 1.0f)) * sl) - side[s]);

        ml = punch.process (ml);
        const float p = env.push (ml);
        lt.push (ml * ml);
        const float gr = std::max (-12.0f, -std::max (0.0f, powDb (p) - (lt.db() + 1.0f)) * slope);
        minGr = std::min (minGr, gr);
        const float target = gr + 1.5f * amt;
        gDb = target + (gDb - target) * smC;
        const float mlp = ml * fastDbToGain (gDb);

        // upper-bass harmonics (even), level-normalised, band-limited 150-600 Hz
        float hf = 0.0f;
        if (amt > 0.0f)
        {
            const float k = 2.0f / (std::sqrt (lt.v) * 1.414f + 1.0e-4f);
            const float h = std::abs (std::tanh (k * mlp)) / k;
            hf = hLp.process (hHp2.process (hHp1.process (h)));
        }
        mid[s] += eng * ((mh + mlp + amt * 0.5f * hf) - mid[s]);
    }
    grDb = minGr;
}

//==============================================================================
// Focus
//==============================================================================
void Focus::prepare (double sampleRate)
{
    fs = sampleRate;
    det.c = design::bandpass (fs, 2600.0, 0.8);
    env.setup (fs, 8.0f, 150.0f);
    lt.setup (fs, 3.0);
    reset();
}

void Focus::reset()
{
    det.reset(); sideBell.reset(); midBell.reset();
    sideBell.c = midBell.c = BiquadCoeffs();
    env.v = 1.0e-9f; lt.v = 1.0e-3f; curDuck = 0.0f; lastDuck = 1.0e9f; lastAmt = -1.0f;
}

void Focus::process (float* mid, float* side, int n, float amt) noexcept
{
    if (std::abs (amt - lastAmt) > 0.002f)
    {
        midBell.c = amt < 0.001f ? BiquadCoeffs() : design::peaking (fs, 3200.0, 1.0, 1.8 * amt);
        lastAmt = amt;
    }
    for (int s = 0; s < n; ++s)
    {
        const float d = det.process (mid[s]);
        env.push (d);
        lt.push (d * d);
    }
    const float activity = std::clamp ((env.db() - (lt.db() - 8.0f)) / 10.0f, 0.0f, 1.0f);
    const float target = env.db() > -70.0f ? -6.0f * amt * activity : 0.0f;
    const float c = (float) std::exp (-(double) n / (0.03 * fs));
    curDuck = target + (curDuck - target) * c;
    if (std::abs (curDuck - lastDuck) > 0.02f)
    {
        sideBell.c = std::abs (curDuck) < 0.01f ? BiquadCoeffs() : design::peaking (fs, 2600.0, 0.8, curDuck);
        lastDuck = curDuck;
    }
    for (int s = 0; s < n; ++s)
    {
        mid[s]  = midBell.process (mid[s]);
        side[s] = sideBell.process (side[s]);
    }
    duckDb = curDuck;
}

//==============================================================================
// Shine
//==============================================================================
Shine::Shine() = default;
Shine::~Shine() = default;

void Shine::prepare (double sampleRate)
{
    fs = sampleRate;
    for (int c = 0; c < 2; ++c)
    {
        bHp[c].c  = design::highpass (fs, 2200.0, 0.7071);
        bLp[c].c  = design::lowpass  (fs, std::min (9000.0, fs * 0.42), 0.7071);
        hHp1[c].c = hHp2[c].c = design::highpass (fs, 4500.0, 0.7071);
    }
    fft = std::make_unique<juce::dsp::FFT> (10);
    ring.assign (1024, 0.0f);
    work.assign (2048, 0.0f);
    window.resize (1024);
    for (int i = 0; i < 1024; ++i)
        window[(size_t) i] = 0.5f - 0.5f * (float) std::cos (2.0 * kPi * i / 1023.0);
    reset();
}

void Shine::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        bHp[c].reset(); bLp[c].reset(); hHp1[c].reset(); hHp2[c].reset(); air[c].reset();
        air[c].c = BiquadCoeffs();
    }
    std::fill (ring.begin(), ring.end(), 0.0f);
    ringPos = sinceFft = 0;
    tonal = tonalTarget = 0.5f; sib = sibTarget = 0.0f; lastAir = 1.0e9f;
}

void Shine::runFft()
{
    for (int i = 0; i < 1024; ++i)
        work[(size_t) i] = ring[(size_t) ((ringPos + i) & 1023)] * window[(size_t) i];
    std::fill (work.begin() + 1024, work.end(), 0.0f);
    fft->performFrequencyOnlyForwardTransform (work.data(), true);

    const double binHz = fs / 1024.0;
    const double nyq = fs * 0.47;
    auto bin = [binHz] (double f) { return std::clamp ((int) std::round (f / binHz), 1, 511); };

    // spectral flatness over the melodic-brightness region
    const int k0 = bin (2000.0), k1 = bin (std::min (9000.0, nyq));
    double logSum = 0.0, sum = 0.0; int cnt = 0;
    for (int k = k0; k <= k1; ++k)
    {
        const double p = (double) work[(size_t) k] * work[(size_t) k] + 1.0e-14;
        logSum += std::log (p); sum += p; ++cnt;
    }
    if (cnt < 4 || sum / cnt < 1.0e-11)
        return; // too quiet to judge: keep previous estimate

    const double sf = std::exp (logSum / cnt) / (sum / cnt);
    tonalTarget = (float) std::clamp ((0.5 - sf) / 0.35, 0.0, 1.0);

    double eMid = 0.0, eHi = 0.0;
    for (int k = bin (1500.0); k < bin (5500.0); ++k) eMid += (double) work[(size_t) k] * work[(size_t) k];
    for (int k = bin (5500.0); k <= bin (std::min (12000.0, nyq)); ++k) eHi += (double) work[(size_t) k] * work[(size_t) k];
    const double ratioDb = 10.0 * std::log10 ((eHi + 1.0e-14) / (eMid + 1.0e-14));
    sibTarget = (float) std::clamp ((ratioDb + 8.0) / 8.0, 0.0, 1.0);
}

void Shine::analyse (const float* mono, int n)
{
    for (int s = 0; s < n; ++s)
    {
        ring[(size_t) ringPos] = mono[s];
        ringPos = (ringPos + 1) & 1023;
        if (++sinceFft >= 512) { sinceFft = 0; runFft(); }
    }
    const float c = (float) std::exp (-(double) n / (0.08 * fs));
    tonal = tonalTarget + (tonal - tonalTarget) * c;
    sib   = sibTarget   + (sib   - sibTarget)   * c;
}

void Shine::process (float* l, float* r, int n, float amt) noexcept
{
    gate = 0.25f + 0.75f * (tonal * (1.0f - 0.7f * sib));
    const float airDb = 3.0f * amt * (1.0f - 0.8f * sib);
    if (std::abs (airDb - lastAir) > 0.02f)
    {
        const auto co = airDb < 0.01f ? BiquadCoeffs() : design::highShelf (fs, std::min (10000.0, fs * 0.42), 0.9, airDb);
        air[0].c = air[1].c = co;
        lastAir = airDb;
    }
    const float k = amt * 0.4f * gate;
    float* ch[2] = { l, r };
    for (int c = 0; c < 2; ++c)
        for (int s = 0; s < n; ++s)
        {
            const float x = ch[c][s];
            const float b = bLp[c].process (bHp[c].process (x));
            const float h = hHp2[c].process (hHp1[c].process (std::abs (b)));
            ch[c][s] = air[c].process (x + k * h);
        }
}

//==============================================================================
// Depth3D
//==============================================================================
void Depth3D::prepare (double sampleRate)
{
    fs = sampleRate;
    hp.c = design::highpass (fs, 700.0, 0.7071);
    // first-order all-pass decorrelator, corner frequencies spread across the mids/highs
    const double fc[4] = { 900.0, 2300.0, 4700.0, 9000.0 };
    for (int i = 0; i < 4; ++i)
    {
        const double t = std::tan (kPi * std::min (fc[i], fs * 0.45) / fs);
        ap[i] = (float) ((t - 1.0) / (t + 1.0));
    }
    reset();
}

void Depth3D::reset()
{
    hp.reset(); shelf.reset(); shelf.c = BiquadCoeffs();
    for (auto& v : z) v = 0.0f;
    lastShelf = -1.0f;
}

void Depth3D::process (const float* mid, float* side, int n, float amt) noexcept
{
    const float shelfDb = 2.5f * amt;
    if (std::abs (shelfDb - lastShelf) > 0.02f)
    {
        shelf.c = shelfDb < 0.01f ? BiquadCoeffs() : design::highShelf (fs, 3500.0, 0.8, shelfDb);
        lastShelf = shelfDb;
    }
    const float k = 0.2f * amt;
    for (int s = 0; s < n; ++s)
    {
        float x = hp.process (mid[s]);
        for (int i = 0; i < 4; ++i)
        {
            const float y = ap[i] * x + z[i];
            z[i] = x - ap[i] * y;
            x = y;
        }
        side[s] = shelf.process (side[s] + k * x);
    }
}

//==============================================================================
// Analog
//==============================================================================
constexpr float Analog::tolDrive[2];
constexpr float Analog::tolBias[2];
constexpr float Analog::tolIron[2];
constexpr float Analog::tolHf[2];

namespace
{
    /** Rational tanh approximation (smooth, saturates exactly at +/-1 beyond |x| = 3). */
    inline float ftanh (float x) noexcept
    {
        if (x > 3.0f) return 1.0f;
        if (x < -3.0f) return -1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    inline float onePoleA (double f, double rate) { return (float) (1.0 - std::exp (-2.0 * kPi * f / rate)); }

    /** Asymmetric class-A curve with unity slope at 0. */
    inline float asym (float u, float b) noexcept
    {
        const float tb = ftanh (b);
        return (ftanh (u + b) - tb) / (1.0f - tb * tb);
    }

    /** Oxford-Inflator style curve (curve = 0): lifts low levels ~+3.5 dB, lands smoothly on 1.0. */
    inline float inflate (float u) noexcept
    {
        const float a = std::min (std::abs (u), 1.0f);
        const float a2 = a * a, a3 = a2 * a, a4 = a2 * a2;
        const float y = 1.5f * a - 0.5f * a3 - 0.0625f * (a2 - 2.0f * a3 + a4);
        return u < 0.0f ? -y : y;
    }
}

void Analog::prepare (double baseFs, int factor)
{
    fs = baseFs; osFs = baseFs * factor;
    fastEnv.setup (fs, 10.0f, 30.0f);
    lt.setup (fs, 2.0);
    pkRel = tauCoef (1.5, fs);
    hotA = msToCoef (3.0f, osFs); hotR = msToCoef (60.0f, osFs);
    for (int c = 0; c < 2; ++c)
    {
        iron[c].a      = onePoleA (110.0 * tolIron[c], osFs);
        thickIron[c].a = onePoleA (160.0 * tolIron[c], osFs);
        emDe[c].c  = design::highShelf (osFs, 3500.0, 0.8, -6.0);
        emPre[c].c = design::highShelf (osFs, 3500.0, 0.8,  6.0);
        preEmph[c].c = design::highShelf (osFs, 3000.0, 1.0,  4.0);
        deEmph[c].c  = design::highShelf (osFs, 3000.0, 1.0, -5.0);
        dc[c].c      = design::highpass1 (fs, 8.0);
    }
    reset();
}

void Analog::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        iron[c].z = hfSoft[c].z = thickIron[c].z = 0.0f;
        preEmph[c].reset(); deEmph[c].reset(); bump[c].reset(); air[c].reset(); dc[c].reset();
        emDe[c].reset(); emPre[c].reset();
        bump[c].c = air[c].c = BiquadCoeffs();
        tapePrev[c] = hotEnv[c] = 0.0f;
    }
    fastEnv.v = 1.0e-9f; lt.v = 1.0e-3f; pk = 0.1f;
    lastBump = lastAir = lastSoft = -1.0f;
}

void Analog::computeLevels (const float* l, const float* r, int n, float amt, float* sweetDrive, float* norm) noexcept
{
    for (int s = 0; s < n; ++s)
    {
        const float m = 0.5f * (l[s] + r[s]);
        const float p = fastEnv.push (m);
        lt.push (m * m);
        const float am = std::max (std::abs (l[s]), std::abs (r[s]));
        pk = am > pk ? am : am + (pk - am) * pkRel;
        const float ltR = std::sqrt (lt.v);
        const float df = std::clamp (std::sqrt (ltR / (std::sqrt (p) + 1.0e-6f)), 0.7f, 1.5f);
        sweetDrive[s] = (0.2f + 2.0f * std::pow (amt, 1.3f)) / std::max (ltR * 1.41f, 0.003f) * df;
        norm[s] = 0.95f / std::max (pk, 0.003f);
    }
}

void Analog::process (float* const* ch, int numOs, int factor, const float* sweetDrive, const float* norm, const Params& p) noexcept
{
    const float color = std::clamp (p.color, 0.0f, 1.5f);
    const float dense = std::clamp (p.dense, 0.0f, 2.2f);
    const float thick = std::clamp (p.thick, 0.0f, 2.2f);
    const float sweet = std::clamp (p.sweet, 0.0f, 1.0f);
    if (color < 1.0e-4f && dense < 1.0e-4f && thick < 1.0e-4f && sweet < 5.0e-4f)
        return;

    // control-rate coefficient updates
    const float softHz = (float) std::min (osFs * 0.45, 30000.0 - 9000.0 * std::min (color, 1.0f));
    if (std::abs (softHz - lastSoft) > 50.0f)
    {
        for (int c = 0; c < 2; ++c) hfSoft[c].a = onePoleA (softHz * tolHf[c], osFs);
        lastSoft = softHz;
    }
    const float airDb = 1.2f * std::min (dense, 1.0f);
    if (std::abs (airDb - lastAir) > 0.02f)
    {
        air[0].c = air[1].c = airDb < 0.01f ? BiquadCoeffs() : design::highShelf (osFs, 14000.0, 0.8, airDb);
        lastAir = airDb;
    }
    const float bumpDb = p.sweetChar == Tape ? 1.8f * sweet : 0.0f;
    if (std::abs (bumpDb - lastBump) > 0.01f)
    {
        bump[0].c = bump[1].c = bumpDb < 0.01f ? BiquadCoeffs() : design::peaking (osFs, 60.0, 0.9, bumpDb);
        lastBump = bumpDb;
    }

    const float colorMix = std::min (1.0f, color * 1.5f);
    const float colorDrive = 0.35f + 0.65f * std::min (color, 1.0f);  // relative to the programme peak
    const float denseSat = std::min (1.0f, 0.35f * dense), denseEnh = std::min (1.0f, 0.3f * dense);
    const float thickMix = std::min (1.0f, thick * 1.1f);
    const float fat = std::clamp ((thick - 0.85f) / 0.45f, 0.0f, 1.0f);
    const float fatK = 1.0f + 0.8f * fat, fatN = 1.0f / ftanh (fatK);
    const float sweetMix = std::min (1.0f, sweet * 1.6f);
    // keep the level up when it gets dirty: makeup for the fundamental lost in the curve
    const float u0 = 0.2f + 2.0f * std::pow (sweet, 1.3f);
    const float sweetMakeup = p.sweetChar == HotTube ? 1.0f : std::pow (u0 / std::tanh (u0), 0.75f);
    constexpr float tp1 = 0.09966799f, pentN = 1.0f / (1.6f * (1.0f - tp1 * tp1)); // tanh(0.1)

    for (int c = 0; c < 2; ++c)
    {
        float* xs = ch[c];
        for (int i = 0; i < numOs; ++i)
        {
            float x = xs[i];
            const float gn = norm[i / factor];
            const bool shaping = colorMix > 0.0f || dense > 0.0f || thickMix > 0.0f;
            if (shaping) x = emDe[c].process (x);

            // 1. CHENG Color: iron + class-A amp, slightly different per channel
            if (colorMix > 0.0f)
            {
                const float g = gn * colorDrive * tolDrive[c];
                float u = g * x;
                const float lf = iron[c].lp (u);
                u += (ftanh (1.8f * lf) / 1.8f - lf) * 0.5f;
                float y = asym (u, 0.06f + tolBias[c]) / g;
                y = hfSoft[c].lp (y);
                x += colorMix * (y - x);
            }

            // 2. Dense: parallel pentode + triode, then enhance-style soft clip and air
            if (dense > 0.0f)
            {
                const float u = gn * x;
                const float pent = (ftanh (1.6f * u + 0.1f) - tp1) * pentN;
                const float tri  = asym (0.8f * u, 0.25f) / 0.8f;
                x += denseSat * ((0.55f * pent + 0.45f * tri) / gn - x);
                const float ge = gn * (0.85f + 0.35f * dense);
                x += denseEnh * (ftanh (ge * x) / ge - x);
                x = air[c].process (x);
            }

            // 3. Thick: inflator + low iron + fat
            if (thickMix > 0.0f)
            {
                const float gi = gn * (1.0f + 0.8f * fat);
                float u = gi * x;
                if (fat > 0.0f) u = ftanh (fatK * u) * fatN;
                const float lf = thickIron[c].lp (u);
                u += (ftanh (2.2f * lf) / 2.2f - lf) * 0.5f * std::min (thick, 1.0f);
                x += thickMix * (inflate (u) / gi - x);
            }
            if (shaping) x = emPre[c].process (x);

            // 4. Sweet
            if (sweetMix > 0.0f)
            {
                const float gs = sweetDrive[i / factor];
                float y;
                switch (p.sweetChar)
                {
                    case Tape:
                    {
                        const float e = preEmph[c].process (x);
                        const float v = gs * (e + 0.25f * (e - tapePrev[c]));
                        tapePrev[c] = e;
                        y = bump[c].process (deEmph[c].process (ftanh (v) / gs));
                        break;
                    }
                    case HotTube:
                    {
                        const float g2 = gs * 1.3f;
                        const float u = g2 * x;
                        const float au = std::abs (u);
                        hotEnv[c] = au + (hotEnv[c] - au) * (au > hotEnv[c] ? hotA : hotR);
                        const float b = 0.15f + 0.5f * std::min (hotEnv[c], 2.0f);
                        y = asym (u, b) / g2 / (1.0f + 0.12f * hotEnv[c]);
                        break;
                    }
                    default: // Pentode
                        y = (ftanh (gs * x + 0.05f) - 0.04995837f) / (gs * 0.99750416f);
                        break;
                }
                x += sweetMix * (y * sweetMakeup - x);
            }
            xs[i] = x;
        }
    }
}

void Analog::postDc (float* l, float* r, int n) noexcept
{
    for (int s = 0; s < n; ++s) { l[s] = dc[0].process (l[s]); r[s] = dc[1].process (r[s]); }
}

//==============================================================================
// AutoGain
//==============================================================================
void AutoGain::prepare (double sampleRate)
{
    fs = sampleRate;
    for (auto* k : { &kin, &kout })
    {
        k->hp.c = design::highpass (fs, 60.0, 0.5);
        k->shelf.c = design::highShelf (fs, 1500.0, 1.0, 4.0);
    }
    c = tauCoef (2.0, fs);
    gC = tauCoef (0.4, fs);
    reset();
}

void AutoGain::reset()
{
    kin.hp.reset(); kin.shelf.reset(); kout.hp.reset(); kout.shelf.reset();
    inP = outP = 1.0e-4f; gSm = 1.0f; gainDb = 0.0f;
}

void AutoGain::pushInput (const float* l, const float* r, int n) noexcept
{
    for (int s = 0; s < n; ++s)
    {
        const float k = kin.run (0.5f * (l[s] + r[s]));
        inP = k * k + (inP - k * k) * c;
    }
}

void AutoGain::process (float* l, float* r, int n, bool enabled) noexcept
{
    for (int s = 0; s < n; ++s)
    {
        const float k = kout.run (0.5f * (l[s] + r[s]));
        outP = k * k + (outP - k * k) * c;
        float target = 1.0f;
        if (enabled)
            target = inP > 1.0e-7f ? std::clamp (std::sqrt (inP / std::max (outP, 1.0e-12f)), 0.25f, 4.0f) : gSm;
        gSm = target + (gSm - target) * gC;
        l[s] *= gSm; r[s] *= gSm;
    }
    gainDb = gainToDb (gSm);
}

//==============================================================================
// Limiter
//==============================================================================
void Limiter::prepare (double sampleRate, int maxLookahead)
{
    fs = sampleRate;
    const int cap = maxLookahead + 8;
    gReq.assign ((size_t) cap, 1.0f);
    gEnv.assign ((size_t) cap, 1.0f);
    dl.prepare (cap); dr.prepare (cap);
    relC = msToCoef (60.0f, fs);
    setLookahead (la);
}

void Limiter::setLookahead (int samples)
{
    la = std::clamp (samples, 0, (int) gReq.size() - 8);
    dl.setDelay (la); dr.setDelay (la);
    reset();
}

void Limiter::reset()
{
    std::fill (gReq.begin(), gReq.end(), 1.0f);
    std::fill (gEnv.begin(), gEnv.end(), 1.0f);
    dl.reset(); dr.reset();
    gPos = 0; env = 1.0f; boxSum = (float) la; grDb = 0.0f;
    for (auto& h : hist) h[0] = h[1] = h[2] = 0.0f;
}

void Limiter::process (float* l, float* r, int n, float ceilingDb, bool truePeak) noexcept
{
    const float ceil = dbToGain (ceilingDb);
    const int size = (int) gReq.size();
    const int win = la + (truePeak ? 3 : 1);
    float minG = 1.0f;

    for (int s = 0; s < n; ++s)
    {
        float in[2] = { l[s], r[s] };
        float peak = std::max (std::abs (in[0]), std::abs (in[1]));

        if (truePeak)
            for (int c = 0; c < 2; ++c)
            {
                // Catmull-Rom estimate of the waveform between the two previous samples
                const float p0 = hist[c][0], p1 = hist[c][1], p2 = hist[c][2], p3 = in[c];
                for (float t : { 0.25f, 0.5f, 0.75f })
                {
                    const float t2 = t * t, t3 = t2 * t;
                    const float v = 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
                                            + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
                    peak = std::max (peak, std::abs (v));
                }
                hist[c][0] = p1; hist[c][1] = p2; hist[c][2] = p3;
            }

        const float req = peak > ceil ? ceil / peak : 1.0f;

        float g;
        if (la == 0)
        {
            env = req < env ? req : req + (env - req) * relC;
            g = env;
            l[s] = in[0] * g; r[s] = in[1] * g;
        }
        else
        {
            gReq[(size_t) gPos] = req;
            float t = 1.0f;
            for (int k = 0; k < win; ++k)
            {
                int idx = gPos - k; if (idx < 0) idx += size;
                t = std::min (t, gReq[(size_t) idx]);
            }
            env = t < env ? t : t + (env - t) * relC;

            // box average of the last `la` envelope values
            int old = gPos - la; if (old < 0) old += size;
            boxSum += env - gEnv[(size_t) old];
            gEnv[(size_t) gPos] = env;
            g = std::min (1.0f, boxSum / (float) la);

            l[s] = dl.process (in[0]) * g;
            r[s] = dr.process (in[1]) * g;
            if (++gPos >= size) gPos = 0;
        }
        l[s] = std::clamp (l[s], -ceil, ceil);
        r[s] = std::clamp (r[s], -ceil, ceil);
        minG = std::min (minG, g);
    }
    grDb = gainToDb (minG);
}
} // namespace fl
