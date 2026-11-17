// Offline checks for CHENG Flower: transparency, latency, each module's effect, limiter, stability, CPU.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>
#include <chrono>
#include <random>
#include <functional>

static int failures = 0;

static void set (ChengFlowerProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

/** Defaults, but with the always-on Color off so module tests start from a clean path. */
static void resetAll (ChengFlowerProcessor& p)
{
    for (auto* prm : p.getParameters())
        prm->setValueNotifyingHost (prm->getDefaultValue());
    set (p, "color", 0.0f);
}

static void check (const char* name, bool ok, const char* fmt, double a, double b = 0)
{
    if (! ok) ++failures;
    char msg[160]; std::snprintf (msg, sizeof msg, fmt, a, b);
    std::printf ("  [%s] %-50s %s\n", ok ? " OK " : "FAIL", name, msg);
}

using Gen = std::function<void (int idx, float& l, float& r)>;

/** Runs `seconds` of generated audio; returns output (both channels) of the last `keep` seconds. */
static void run (ChengFlowerProcessor& p, double fs, double seconds, const Gen& gen,
                 std::vector<float>& outL, std::vector<float>& outR, std::vector<float>* inL = nullptr,
                 double keep = 1.0, int block = 512)
{
    p.prepareToPlay (fs, block);
    const int total = (int) (seconds * fs), keepFrom = total - (int) (keep * fs);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    outL.clear(); outR.clear(); if (inL) inL->clear();
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        buf.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i) { float l, r; gen (pos + i, l, r); buf.setSample (0, i, l); buf.setSample (1, i, r); }
        if (inL && pos + n > keepFrom) for (int i = 0; i < n; ++i) if (pos + i >= keepFrom) inL->push_back (buf.getSample (0, i));
        p.processBlock (buf, midi);
        for (int i = 0; i < n; ++i)
            if (pos + i >= keepFrom) { outL.push_back (buf.getSample (0, i)); outR.push_back (buf.getSample (1, i)); }
    }
}

static double rms (const std::vector<float>& v) { double s = 0; for (float x : v) s += (double) x * x; return std::sqrt (s / std::max<size_t> (1, v.size())); }
static double peak (const std::vector<float>& v) { double m = 0; for (float x : v) m = std::max (m, (double) std::abs (x)); return m; }
static double db (double g) { return 20.0 * std::log10 (std::max (g, 1e-12)); }
static bool allFinite (const std::vector<float>& v) { for (float x : v) if (! std::isfinite (x)) return false; return true; }

/** Amplitude of a frequency component (single-bin DFT, Hann). */
static double tone (const std::vector<float>& v, double fs, double f)
{
    double re = 0, im = 0, wsum = 0; const size_t n = v.size();
    for (size_t i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2 * fl::kPi * i / (n - 1));
        re += w * v[i] * std::cos (2 * fl::kPi * f * i / fs);
        im += w * v[i] * std::sin (2 * fl::kPi * f * i / fs);
        wsum += w;
    }
    return 2.0 * std::sqrt (re * re + im * im) / wsum;
}

static Gen sine (double fs, double f, float a) { return [=] (int i, float& l, float& r) { l = r = a * (float) std::sin (2 * fl::kPi * f * i / fs); }; }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        ChengFlowerProcessor p;
        const double fs = 48000.0;
        set (p, "bloom", argc > 3 ? juce::String (argv[3]).getFloatValue() : -35.0f);
        set (p, "shine", 40.0f); set (p, "sweet", 30.0f); set (p, "low", 55.0f); set (p, "deharsh", 25.0f); set (p, "thick", 45.0f);
        // feed some music-like signal so meters and spectrum have content
        std::mt19937 rng (3); std::normal_distribution<float> nd (0.0f, 1.0f);
        float pk = 0.0f, b1 = 0.0f;
        Gen g = [&] (int i, float& l, float& r)
        {
            const double t = i / fs;
            const float kick = (float) std::exp (-std::fmod (t, 0.5) * 18.0) * (float) std::sin (2 * fl::kPi * 55 * t);
            b1 = 0.97f * b1 + 0.03f * nd (rng); pk = b1;
            const float pad = 0.15f * (float) (std::sin (2 * fl::kPi * 220 * t) + 0.6 * std::sin (2 * fl::kPi * 330 * t) + 0.3 * std::sin (2 * fl::kPi * 380 * t));
            l = 0.5f * kick + pad + 0.6f * pk + 0.01f * nd (rng);
            r = 0.5f * kick + pad * 0.8f + 0.6f * pk + 0.01f * nd (rng);
        };
        std::vector<float> a, b;
        run (p, fs, 2.0, g, a, b, nullptr, 0.1);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (ChengFlowerEditor::baseW, ChengFlowerEditor::baseH);
        auto* fe = dynamic_cast<ChengFlowerEditor*> (ed.get());
        for (int i = 0; i < 40; ++i) { std::vector<float> x, y; run (p, fs, 0.03, g, x, y, nullptr, 0.03); fe->refresh(); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (argv[2]);
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot written: %s\n", argv[2]);
        return 0;
    }

    std::vector<float> L, R, IN;

    if (argc > 1 && juce::String (argv[1]) == "--ir")
    {
        for (int mode : { 0, 1 })
        {
            ChengFlowerProcessor p; resetAll (p); set (p, "mode", (float) mode); set (p, "autoGain", 0.0f);
            const double fs = 48000.0;
            run (p, fs, 1.0, [&] (int i, float& l, float& r) { l = r = (i == 24000 ? 1.0f : 0.0f); }, L, R, nullptr, 0.5);
            int am = 0; for (size_t i = 0; i < L.size(); ++i) if (std::abs (L[i]) > std::abs (L[(size_t) am])) am = (int) i;
            std::printf ("mode %d latency %d  ir peak at %d  value %.3f\n", mode, p.getLatencySamples(), am, L[(size_t) am]);
            for (int i = 0; i < 12; ++i) std::printf ("  %d: %.4f\n", i, L[(size_t) i]);
        }
        return 0;
    }

    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int mode : { 0, 1 })
        {
            std::printf ("\n== fs %.0f  mode %s ==\n", fs, mode ? "Master" : "Track");
            ChengFlowerProcessor p;
            resetAll (p); set (p, "mode", (float) mode);
            p.prepareToPlay (fs, 512);
            const int lat = p.getLatencySamples();
            std::printf ("  latency: %d samples (%.2f ms)\n", lat, lat * 1000.0 / fs);

            // 1. transparency at default settings
            for (double f : { 60.0, 1000.0, 8000.0 })
            {
                run (p, fs, 3.0, sine (fs, f, 0.25f), L, R);
                char nm[64]; std::snprintf (nm, sizeof nm, "neutral gain @ %.0f Hz", f);
                check (nm, std::abs (db (rms (L) / (0.25 / std::sqrt (2.0)))) < 0.3, "%+.2f dB", db (rms (L) / (0.25 / std::sqrt (2.0))));
            }

            // 2. bypass is latency-compensated and bit-transparent
            {
                set (p, "bypass", 1.0f); set (p, "bloom", 60.0f); set (p, "sweet", 80.0f);
                std::mt19937 rng (1); std::uniform_real_distribution<float> u (-0.5f, 0.5f);
                std::vector<float> src (200000);
                for (auto& x : src) x = u (rng);
                run (p, fs, 1.5, [&] (int i, float& l, float& r) { l = r = src[(size_t) i]; }, L, R, &IN, 1.0);
                double err = 0;
                for (size_t i = (size_t) lat + 2000; i < L.size(); ++i) err = std::max (err, (double) std::abs (L[i] - IN[i - (size_t) lat]));
                check ("bypass == input delayed by latency", err < 1e-6, "max err %.2e", err);
                set (p, "bypass", 0.0f); set (p, "bloom", 0.0f); set (p, "sweet", 0.0f);

                // processed path: impulse lands on the reported latency (all modules neutral)
                set (p, "autoGain", 0.0f);
                run (p, fs, 1.0, [&] (int i, float& l, float& r) { l = r = (i == (int) (fs * 0.5) ? 1.0f : 0.0f); }, L, R, nullptr, 0.5);
                int am = 0; for (size_t i = 0; i < L.size(); ++i) if (std::abs (L[i]) > std::abs (L[(size_t) am])) am = (int) i;
                check ("processed impulse peak at latency", std::abs (am - lat) <= 2 && L[(size_t) am] > 0.5f, "peak at %.0f, value %.3f", am, L[(size_t) am]);
                set (p, "autoGain", 1.0f);
            }

            // 3. limiter ceiling
            {
                set (p, "output", 9.0f); set (p, "ceiling", -1.0f); set (p, "autoGain", 0.0f);
                std::mt19937 rng (5); std::normal_distribution<float> nd (0.0f, 0.3f);
                run (p, fs, 2.0, [&] (int, float& l, float& r) { l = nd (rng); r = nd (rng); }, L, R);
                const double pk = db (std::max (peak (L), peak (R)));
                check ("limiter holds ceiling (-1 dB, +9 dB drive)", pk <= -0.99, "peak %+.2f dBFS", pk);
                resetAll (p); set (p, "mode", (float) mode);
            }

            // 4. Low End: side bass goes mono
            {
                set (p, "low", 100.0f); set (p, "autoGain", 0.0f);
                run (p, fs, 3.0, [&] (int i, float& l, float& r) { const float s = 0.2f * (float) std::sin (2 * fl::kPi * 50 * i / fs); l = s; r = -s; }, L, R);
                std::vector<float> sd (L.size()); for (size_t i = 0; i < L.size(); ++i) sd[i] = 0.5f * (L[i] - R[i]);
                const double red = db (tone (sd, fs, 50) / 0.2);
                check ("low end: 50 Hz side reduced", red < -20.0, "%+.1f dB", red);
                resetAll (p); set (p, "mode", (float) mode);
            }

            // 5. Width: 3 kHz side lifted, mid untouched
            {
                set (p, "width", 100.0f); set (p, "autoGain", 0.0f);
                run (p, fs, 2.0, [&] (int i, float& l, float& r)
                     { const float m = 0.1f * (float) std::sin (2 * fl::kPi * 500 * i / fs), s = 0.1f * (float) std::sin (2 * fl::kPi * 3000 * i / fs); l = m + s; r = m - s; }, L, R);
                std::vector<float> sd (L.size()), md (L.size());
                for (size_t i = 0; i < L.size(); ++i) { sd[i] = 0.5f * (L[i] - R[i]); md[i] = 0.5f * (L[i] + R[i]); }
                check ("width: 3 kHz side lifted", db (tone (sd, fs, 3000) / 0.1) > 4.5, "%+.1f dB", db (tone (sd, fs, 3000) / 0.1));
                check ("width: mid unchanged", std::abs (db (tone (md, fs, 500) / 0.1)) < 0.3, "%+.2f dB", db (tone (md, fs, 500) / 0.1));
                resetAll (p); set (p, "mode", (float) mode);
            }

            // 6. Shine adds octave harmonics on a tone
            {
                set (p, "autoGain", 0.0f);
                run (p, fs, 2.0, sine (fs, 3000, 0.2f), L, R);
                const double off = tone (L, fs, 6000);
                set (p, "shine", 100.0f);
                run (p, fs, 2.0, sine (fs, 3000, 0.2f), L, R);
                const double on = tone (L, fs, 6000);
                check ("shine: 6 kHz harmonic generated (rel. 3k)", db (on / 0.2) > -30.0 && db (off / 0.2) < -80.0, "on %+.1f dB / off %+.1f dB", db (on / 0.2), db (off / 0.2));
                resetAll (p); set (p, "mode", (float) mode);
            }

            // 7. Sweet: clearly audible harmonics, level kept in range
            for (int ch : { 0, 1, 2 })
            {
                set (p, "autoGain", 0.0f); set (p, "sweet", 100.0f); set (p, "sweetChar", (float) ch);
                run (p, fs, 3.0, sine (fs, 200, 0.3f), L, R);
                const double f0 = tone (L, fs, 200), h2 = tone (L, fs, 400), h3 = tone (L, fs, 600);
                const double thd = std::sqrt (h2 * h2 + h3 * h3) / f0;
                const char* nm[] = { "sweet pentode: harmonics / level", "sweet tape: harmonics / level", "sweet hot tube: harmonics / level" };
                check (nm[ch], thd > 0.03 && std::abs (db (f0 / 0.3)) < 3.5 && allFinite (L), "THD %.1f%%  level %+.2f dB", thd * 100.0, db (f0 / 0.3));
                resetAll (p); set (p, "mode", (float) mode);
            }

            // 7b. Color (default 35%): audible harmonics + left/right tolerance
            {
                set (p, "color", 35.0f);
                run (p, fs, 3.0, sine (fs, 100, 0.5f), L, R);
                const double thd = std::sqrt (std::pow (tone (L, fs, 200), 2) + std::pow (tone (L, fs, 300), 2)) / tone (L, fs, 100);
                std::vector<float> d (L.size()); for (size_t i = 0; i < L.size(); ++i) d[i] = L[i] - R[i];
                check ("color: harmonics at default", thd > 0.003, "THD %.2f%%", thd * 100.0);
                check ("color: L/R tolerance (analog stereo)", db (rms (d) / rms (L)) > -70.0, "L-R %.1f dB", db (rms (d) / rms (L)));
                resetAll (p); set (p, "mode", (float) mode);
            }

            // 7c. Dense drive adds harmonics; Thick inflates quiet parts
            {
                set (p, "autoGain", 0.0f); set (p, "limiter", 0.0f); set (p, "bloom", 100.0f);
                run (p, fs, 3.0, sine (fs, 300, 0.3f), L, R);
                const double thd = std::sqrt (std::pow (tone (L, fs, 600), 2) + std::pow (tone (L, fs, 900), 2)) / tone (L, fs, 300);
                check ("dense: HG-style harmonics", thd > 0.02, "THD %.1f%%", thd * 100.0);
                resetAll (p); set (p, "mode", (float) mode);

                set (p, "autoGain", 0.0f); set (p, "limiter", 0.0f);
                auto quietRatio = [&] (float th)
                {
                    set (p, "thick", th);
                    // loud burst then quiet passage, repeating
                    run (p, fs, 4.0, [&] (int i, float& l, float& r)
                         { const double t = (double) i / fs; const float a = std::fmod (t, 0.5) < 0.1 ? 0.8f : 0.08f;
                           l = r = a * (float) std::sin (2 * fl::kPi * 440 * t); }, L, R, nullptr, 2.0);
                    double loud = 0, quiet = 0;
                    for (size_t i = 0; i < L.size(); ++i)
                    {
                        const double t = std::fmod ((double) i / fs, 0.5);
                        if (t > 0.02 && t < 0.09) loud = std::max (loud, (double) std::abs (L[i]));
                        if (t > 0.2 && t < 0.45) quiet = std::max (quiet, (double) std::abs (L[i]));
                    }
                    return db (quiet / loud);
                };
                const double q0 = quietRatio (0.0f), q1 = quietRatio (100.0f);
                check ("thick: quiet parts inflated vs loud", q1 > q0 + 2.5, "%+.1f dB vs %+.1f", q1, q0);
                resetAll (p); set (p, "mode", (float) mode);
            }

            // 7d. Open adds 3D: a mono source gets a decorrelated side
            {
                set (p, "autoGain", 0.0f); set (p, "bloom", -100.0f);
                std::mt19937 rng (4); std::normal_distribution<float> nd (0.0f, 0.1f);
                run (p, fs, 2.0, [&] (int, float& l, float& r) { l = r = nd (rng); }, L, R);
                std::vector<float> sd (L.size()), md (L.size());
                for (size_t i = 0; i < L.size(); ++i) { sd[i] = 0.5f * (L[i] - R[i]); md[i] = 0.5f * (L[i] + R[i]); }
                check ("open: mono source gains 3D side", db (rms (sd) / rms (md)) > -20.0, "side %+.1f dB", db (rms (sd) / rms (md)));
                resetAll (p); set (p, "mode", (float) mode);
            }

            // 8. Open / Dense change crest factor of a percussive signal
            {
                set (p, "autoGain", 0.0f); set (p, "limiter", 0.0f);
                std::mt19937 rng (9); std::normal_distribution<float> nd (0.0f, 1.0f);
                std::vector<float> src ((size_t) (fs * 4));
                for (size_t i = 0; i < src.size(); ++i)
                {
                    const double t = (double) i / fs;
                    const double ph = std::fmod (t, 0.25);
                    src[i] = 0.3f * (float) std::exp (-ph * 25.0) * (float) std::sin (2 * fl::kPi * 900 * t) + 0.25f * (float) std::exp (-ph * 40.0) * nd (rng);
                }
                // attack-vs-tail energy of every hit: Open should widen it, Dense should narrow it
                auto attackTail = [&] (float bloomV)
                {
                    set (p, "bloom", bloomV);
                    run (p, fs, 4.0, [&] (int i, float& l, float& r) { l = r = src[(size_t) i]; }, L, R, nullptr, 2.0);
                    double acc = 0; int cnt = 0;
                    for (double t0 = 0.25; t0 + 0.25 <= 2.0; t0 += 0.25)
                    {
                        auto seg = [&] (double a0, double a1)
                        {
                            double e = 0; int k = 0;
                            for (int i = (int) ((t0 + a0) * fs); i < (int) ((t0 + a1) * fs); ++i) { e += (double) L[(size_t) i] * L[(size_t) i]; ++k; }
                            return std::sqrt (e / std::max (1, k));
                        };
                        acc += db (seg (0.0, 0.015) / seg (0.08, 0.2)); ++cnt;
                    }
                    return acc / cnt;
                };
                const double c0 = attackTail (0.0f), cO = attackTail (-100.0f), cD = attackTail (100.0f);
                check ("open: attacks stand out more vs tails", cO > c0 + 1.5, "%.1f dB vs %.1f", cO, c0);
                check ("dense: tails fuller vs attacks", cD < c0 - 1.5, "%.1f dB vs %.1f", cD, c0);
                resetAll (p); set (p, "mode", (float) mode);
            }

            // 9. Unmask: resonance in noise gets cut (Open) / harsh resonance (De-Harsh)
            for (int which : { 0, 1 })
            {
                const double rf = which == 0 ? 380.0 : 4800.0;
                set (p, "autoGain", 0.0f);
                std::mt19937 rng (11); std::normal_distribution<float> nd (0.0f, 0.08f);
                float pk1 = 0, pk2 = 0;
                Gen g = [&] (int i, float& l, float& r)
                {
                    // roughly pink noise + a sticking-out resonance
                    const float w = nd (rng); pk1 = 0.99f * pk1 + 0.1f * w; pk2 = 0.6f * pk2 + 0.4f * w;
                    l = r = pk1 * 0.5f + pk2 + 0.12f * (float) std::sin (2 * fl::kPi * rf * i / fs);
                };
                run (p, fs, 3.0, g, L, R);
                const double off = tone (L, fs, rf);
                set (p, which == 0 ? "bloom" : "deharsh", which == 0 ? -100.0f : 100.0f);
                rng.seed (11); pk1 = pk2 = 0;
                run (p, fs, 3.0, g, L, R);
                const double on = tone (L, fs, rf);
                // Open also shapes transients; measure the steady resonance only
                check (which == 0 ? "open: 380 Hz build-up reduced" : "de-harsh: 4.8 kHz resonance reduced", db (on / off) < -2.0, "%+.1f dB", db (on / off));
                resetAll (p); set (p, "mode", (float) mode);
            }

            // 10. Focus ducks side presence while the centre is active
            {
                set (p, "autoGain", 0.0f); set (p, "focus", 100.0f);
                run (p, fs, 3.0, [&] (int i, float& l, float& r)
                     { const float m = 0.15f * (float) std::sin (2 * fl::kPi * 2600 * i / fs), s = 0.08f * (float) std::sin (2 * fl::kPi * 2650 * i / fs); l = m + s; r = m - s; }, L, R);
                std::vector<float> sd (L.size()); for (size_t i = 0; i < L.size(); ++i) sd[i] = 0.5f * (L[i] - R[i]);
                check ("focus: side presence ducked", db (tone (sd, fs, 2650) / 0.08) < -2.5, "%+.1f dB", db (tone (sd, fs, 2650) / 0.08));
                resetAll (p); set (p, "mode", (float) mode);
            }

            // 11. Match keeps loudness matched with everything pushed
            {
                set (p, "autoGain", 1.0f); set (p, "color", 50.0f); set (p, "thick", 60.0f);
                set (p, "bloom", 80.0f); set (p, "low", 80.0f); set (p, "shine", 80.0f); set (p, "sweet", 60.0f); set (p, "width", 50.0f);
                std::mt19937 rng (13); std::normal_distribution<float> nd (0.0f, 0.1f);
                float pk = 0;
                run (p, fs, 8.0, [&] (int, float& l, float& r) { const float w = nd (rng); pk = 0.95f * pk + 0.3f * w; l = r = pk + w * 0.3f; }, L, R, &IN, 3.0);
                const double d = db (rms (L) / rms (IN));
                check ("match: loudness matched", std::abs (d) < 1.5, "%+.2f dB", d);
                resetAll (p); set (p, "mode", (float) mode);
            }

            // 12. stability with random settings and loud input
            {
                std::mt19937 rng (21); std::uniform_real_distribution<float> u (0.0f, 1.0f);
                std::normal_distribution<float> nd (0.0f, 0.7f);
                bool ok = true;
                for (int trial = 0; trial < 6 && ok; ++trial)
                {
                    for (auto* prm : p.getParameters())
                        if (prm != p.getBypassParameter() && prm != p.apvts.getParameter ("mode"))
                            prm->setValueNotifyingHost (u (rng));
                    run (p, fs, 1.0, [&] (int i, float& l, float& r) { l = nd (rng) + (i % 4800 < 10 ? 3.0f : 0.0f); r = nd (rng); }, L, R);
                    ok = allFinite (L) && allFinite (R) && peak (L) < 16.0;
                }
                check ("random settings: finite, bounded", ok, "%.0f", 0);
                resetAll (p); set (p, "mode", (float) mode);
            }

            // 13. CPU
            {
                set (p, "bloom", 50.0f); set (p, "low", 50.0f); set (p, "shine", 50.0f); set (p, "sweet", 50.0f);
                set (p, "focus", 50.0f); set (p, "deharsh", 50.0f); set (p, "width", 50.0f);
                std::mt19937 rng (2); std::normal_distribution<float> nd (0.0f, 0.2f);
                const auto t0 = std::chrono::steady_clock::now();
                run (p, fs, 10.0, [&] (int, float& l, float& r) { l = nd (rng); r = nd (rng); }, L, R, nullptr, 0.1);
                const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
                std::printf ("  cpu: %.1f%% of one core (10 s audio in %.2f s)\n", sec / 10.0 * 100.0, sec);
            }
        }

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
