#pragma once
// CHENG Flower - core DSP building blocks (original code, shared with CHENG Strip).
#include <cmath>
#include <algorithm>
#include <vector>

namespace fl
{
constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

/** Time constant (ms) -> one-pole coefficient. */
inline float msToCoef (float ms, double fs) noexcept
{
    const double samples = std::max (1.0e-3, (double) ms * 0.001 * fs);
    return (float) std::exp (-1.0 / samples);
}

struct BiquadCoeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

/** Transposed direct form II biquad, double precision state. */
struct Biquad
{
    BiquadCoeffs c;
    double z1 = 0, z2 = 0;

    inline float process (float xf) noexcept
    {
        const double x = xf;
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return (float) y;
    }
    void reset() noexcept { z1 = z2 = 0; }
};

// ---------- RBJ-style coefficient designs ----------
namespace design
{
    inline BiquadCoeffs normalise (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        BiquadCoeffs c;
        c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0;
        c.a1 = a1 / a0; c.a2 = a2 / a0;
        return c;
    }

    inline double clampFreq (double f, double fs) { return std::clamp (f, 5.0, fs * 0.49); }

    inline BiquadCoeffs peaking (double fs, double f, double q, double gainDb)
    {
        f = clampFreq (f, fs);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / fs;
        const double alpha = std::sin (w) / (2.0 * q);
        const double cw = std::cos (w);
        return normalise (1 + alpha * A, -2 * cw, 1 - alpha * A,
                          1 + alpha / A, -2 * cw, 1 - alpha / A);
    }

    inline double shelfAlpha (double A, double w, double S)
    {
        const double arg = (A + 1.0 / A) * (1.0 / S - 1.0) + 2.0;
        return std::sin (w) / 2.0 * std::sqrt (std::max (arg, 1.0e-6));
    }

    inline BiquadCoeffs lowShelf (double fs, double f, double S, double gainDb)
    {
        f = clampFreq (f, fs);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / fs;
        const double cw = std::cos (w);
        const double alpha = shelfAlpha (A, w, S);
        const double sA = 2.0 * std::sqrt (A) * alpha;
        return normalise (A * ((A + 1) - (A - 1) * cw + sA),
                          2 * A * ((A - 1) - (A + 1) * cw),
                          A * ((A + 1) - (A - 1) * cw - sA),
                          (A + 1) + (A - 1) * cw + sA,
                          -2 * ((A - 1) + (A + 1) * cw),
                          (A + 1) + (A - 1) * cw - sA);
    }

    inline BiquadCoeffs highShelf (double fs, double f, double S, double gainDb)
    {
        f = clampFreq (f, fs);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * f / fs;
        const double cw = std::cos (w);
        const double alpha = shelfAlpha (A, w, S);
        const double sA = 2.0 * std::sqrt (A) * alpha;
        return normalise (A * ((A + 1) + (A - 1) * cw + sA),
                          -2 * A * ((A - 1) + (A + 1) * cw),
                          A * ((A + 1) + (A - 1) * cw - sA),
                          (A + 1) - (A - 1) * cw + sA,
                          2 * ((A - 1) - (A + 1) * cw),
                          (A + 1) - (A - 1) * cw - sA);
    }

    inline BiquadCoeffs lowpass (double fs, double f, double q)
    {
        f = clampFreq (f, fs);
        const double w = 2.0 * kPi * f / fs;
        const double alpha = std::sin (w) / (2.0 * q);
        const double cw = std::cos (w);
        return normalise ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + alpha, -2 * cw, 1 - alpha);
    }

    inline BiquadCoeffs highpass (double fs, double f, double q)
    {
        f = clampFreq (f, fs);
        const double w = 2.0 * kPi * f / fs;
        const double alpha = std::sin (w) / (2.0 * q);
        const double cw = std::cos (w);
        return normalise ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + alpha, -2 * cw, 1 - alpha);
    }

    /** Band-pass with 0 dB peak gain. */
    inline BiquadCoeffs bandpass (double fs, double f, double q)
    {
        f = clampFreq (f, fs);
        const double w = 2.0 * kPi * f / fs;
        const double alpha = std::sin (w) / (2.0 * q);
        const double cw = std::cos (w);
        return normalise (alpha, 0, -alpha, 1 + alpha, -2 * cw, 1 - alpha);
    }

    inline BiquadCoeffs highpass1 (double fs, double f)
    {
        f = clampFreq (f, fs);
        const double K = std::tan (kPi * f / fs);
        BiquadCoeffs c;
        c.b0 = 1.0 / (K + 1.0); c.b1 = -c.b0; c.b2 = 0;
        c.a1 = (K - 1.0) / (K + 1.0); c.a2 = 0;
        return c;
    }
}

// ---------- small helpers ----------
/** One-pole coefficient for a time constant in seconds at a given update rate. */
inline float tauCoef (double seconds, double rate) noexcept
{
    return (float) std::exp (-1.0 / std::max (1.0e-6, seconds * rate));
}

/** Power envelope follower with separate attack / release (operates on x^2). */
struct PowerEnv
{
    float a = 0.0f, r = 0.0f, v = 1.0e-6f;
    void setup (double fs, float attMs, float relMs) { a = msToCoef (attMs, fs); r = msToCoef (relMs, fs); }
    inline float push (float x) noexcept
    {
        const float p = x * x;
        const float c = p > v ? a : r;
        v = p + (v - p) * c;
        return v;
    }
    float db() const noexcept { return 10.0f * std::log10 (std::max (v, 1.0e-12f)); }
};

/** Peak (absolute value) envelope follower. */
struct PeakEnv
{
    float a = 0.0f, r = 0.0f, v = 0.0f;
    void setup (double fs, float attMs, float relMs) { a = msToCoef (attMs, fs); r = msToCoef (relMs, fs); }
    inline float push (float x) noexcept
    {
        const float p = std::abs (x);
        const float c = p > v ? a : r;
        v = p + (v - p) * c;
        return v;
    }
};

/** Linkwitz-Riley 4th order split (two cascaded Butterworth sections per output). LP + HP sums to an all-pass. */
struct LR4
{
    Biquad lp1, lp2, hp1, hp2;
    void setup (double fs, double f)
    {
        lp1.c = lp2.c = design::lowpass  (fs, f, 0.70710678);
        hp1.c = hp2.c = design::highpass (fs, f, 0.70710678);
    }
    void reset() { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }
    inline void split (float x, float& lo, float& hi) noexcept
    {
        lo = lp2.process (lp1.process (x));
        hi = hp2.process (hp1.process (x));
    }
    /** Same phase response as split() summed, i.e. the matching all-pass. */
    inline float allpass (float x) noexcept
    {
        float lo, hi; split (x, lo, hi); return lo + hi;
    }
};

/** Simple fixed-capacity delay line (integer samples). */
struct Delay
{
    std::vector<float> buf; int w = 0, d = 0;
    void prepare (int capacity) { buf.assign ((size_t) std::max (1, capacity + 1), 0.0f); w = 0; }
    void setDelay (int samples) { d = std::clamp (samples, 0, (int) buf.size() - 1); }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    inline float process (float x) noexcept
    {
        buf[(size_t) w] = x;
        int r = w - d; if (r < 0) r += (int) buf.size();
        const float y = buf[(size_t) r];
        if (++w >= (int) buf.size()) w = 0;
        return y;
    }
};
} // namespace fl
