#pragma once

#include "Params.h"
#include <complex>

// Filters: trapezoidal (Cytomic/Simper) state-variable sections in double precision. They are
// the bilinear transform of the analog prototypes below, stay clean under per-sample
// coefficient changes, and keep no assumptions about block size.
namespace apollo::dsp
{
    inline constexpr double pi = 3.14159265358979323846;

    inline double clampCutoff (double fc, double fs) { return juce::jlimit (1.0, fs * 0.48, fc); }

    /** Q of section k (1-based) of an order-n Butterworth filter. */
    inline double butterworthQ (int order, int k) { return 1.0 / (2.0 * std::sin ((2.0 * k - 1.0) * pi / (2.0 * order))); }

    struct Svf
    {
        double a1 = 1, a2 = 0, a3 = 0, m0 = 1, m1 = 0, m2 = 0;

        void set (double g, double k, double c0, double c1, double c2)
        {
            a1 = 1.0 / (1.0 + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
            m0 = c0; m1 = c1; m2 = c2;
        }
    };

    struct SvfState { double ic1 = 0, ic2 = 0; };

    inline double tick (const Svf& c, SvfState& s, double v0)
    {
        const double v3 = v0 - s.ic2;
        const double v1 = c.a1 * s.ic1 + c.a2 * v3;     // band
        const double v2 = s.ic2 + c.a2 * s.ic1 + c.a3 * v3; // low
        s.ic1 = 2.0 * v1 - s.ic1;
        s.ic2 = 2.0 * v2 - s.ic2;
        return c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
    }

    struct OnePole { double G = 0; bool high = false; };
    struct OnePoleState { double s = 0; };

    inline double tick (const OnePole& c, OnePoleState& st, double x)
    {
        const double v = (x - st.s) * c.G;
        const double lp = v + st.s;
        st.s = lp + v;
        return c.high ? x - lp : lp;
    }

    struct BandSettings
    {
        double freq = 1000.0, gainDb = 0.0, q = shelfQ;
        int order = 4; // HP/LP only: slope / 6
    };

    //==============================================================================
    class BandFilter
    {
    public:
        static constexpr int maxChannels = 8, maxSections = 4;

        void configure (BandType type, const BandSettings& s, double fs)
        {
            const double t = std::tan (pi * clampCutoff (s.freq, fs) / fs);
            numSections = 0;
            useOnePole = false;

            switch (type)
            {
                case BandType::highPass:
                case BandType::lowPass:
                {
                    const bool high = type == BandType::highPass;
                    const int order = juce::jlimit (1, 2 * maxSections, s.order);

                    for (int k = 1; k <= order / 2; ++k)
                    {
                        const double kk = 1.0 / butterworthQ (order, k);
                        sections[(size_t) numSections++].set (t, kk, high ? 1.0 : 0.0, high ? -kk : 0.0, high ? -1.0 : 1.0);
                    }

                    if (order % 2 == 1)
                    {
                        useOnePole = true;
                        onePole = { t / (1.0 + t), high };
                    }
                    break;
                }

                case BandType::peak:
                {
                    const double A = std::pow (10.0, s.gainDb / 40.0);
                    const double k = 1.0 / (s.q * A);
                    sections[(size_t) numSections++].set (t, k, 1.0, k * (A * A - 1.0), 0.0);
                    break;
                }

                case BandType::lowShelf:
                {
                    const double A = std::pow (10.0, s.gainDb / 40.0);
                    const double k = 1.0 / shelfQ;
                    sections[(size_t) numSections++].set (t / std::sqrt (A), k, 1.0, k * (A - 1.0), A * A - 1.0);
                    break;
                }

                case BandType::highShelf:
                {
                    const double A = std::pow (10.0, s.gainDb / 40.0);
                    const double k = 1.0 / shelfQ;
                    sections[(size_t) numSections++].set (t * std::sqrt (A), k, A * A, k * (1.0 - A) * A, 1.0 - A * A);
                    break;
                }
            }
        }

        double process (int channel, double x)
        {
            auto& st = state[(size_t) channel];

            for (int i = 0; i < numSections; ++i)
                x = tick (sections[(size_t) i], st.svf[(size_t) i], x);

            if (useOnePole)
                x = tick (onePole, st.onePole, x);

            return x;
        }

        void reset() { state = {}; }

    private:
        struct ChannelState
        {
            std::array<SvfState, maxSections> svf {};
            OnePoleState onePole {};
        };

        std::array<Svf, maxSections> sections {};
        OnePole onePole {};
        int numSections = 0;
        bool useOnePole = false;
        std::array<ChannelState, maxChannels> state {};
    };

    //==============================================================================
    /** Exact magnitude of BandFilter at frequency f (bilinear-warped analog prototype). */
    inline double magnitudeDb (BandType type, const BandSettings& s, double f, double fs)
    {
        using C = std::complex<double>;
        f = juce::jlimit (1.0, fs * 0.4999, f);
        const C sj (0.0, std::tan (pi * f / fs) / std::tan (pi * clampCutoff (s.freq, fs) / fs));
        C h (1.0, 0.0);

        switch (type)
        {
            case BandType::highPass:
            case BandType::lowPass:
            {
                const bool high = type == BandType::highPass;
                const int order = juce::jlimit (1, 8, s.order);

                for (int k = 1; k <= order / 2; ++k)
                    h *= (high ? sj * sj : C (1.0)) / (sj * sj + sj / butterworthQ (order, k) + 1.0);

                if (order % 2 == 1)
                    h *= (high ? sj : C (1.0)) / (sj + 1.0);
                break;
            }

            case BandType::peak:
            {
                const double A = std::pow (10.0, s.gainDb / 40.0);
                const double k = 1.0 / (s.q * A);
                h = (sj * sj + k * A * A * sj + 1.0) / (sj * sj + k * sj + 1.0);
                break;
            }

            case BandType::lowShelf:
            {
                const double A = std::pow (10.0, s.gainDb / 40.0);
                const double k = 1.0 / shelfQ;
                const C sp = sj * std::sqrt (A);
                h = (sp * sp + k * A * sp + A * A) / (sp * sp + k * sp + 1.0);
                break;
            }

            case BandType::highShelf:
            {
                const double A = std::pow (10.0, s.gainDb / 40.0);
                const double k = 1.0 / shelfQ;
                const C sp = sj / std::sqrt (A);
                h = (A * A * sp * sp + k * A * sp + 1.0) / (sp * sp + k * sp + 1.0);
                break;
            }
        }

        return 20.0 * std::log10 (std::max (std::abs (h), 1e-12));
    }
}
