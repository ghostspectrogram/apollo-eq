#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>
#include <optional>

// Band layout and parameters, modelled on Adobe Audition's Parametric Equalizer:
// HP, low shelf, five peaking bands, high shelf, LP.
namespace apollo
{
    enum class BandType { highPass, lowShelf, peak, highShelf, lowPass };

    struct BandSpec
    {
        const char* id;
        const char* label; // short label on nodes and buttons
        const char* name;  // long name for the host
        BandType type;
        float defaultFreq;
        bool defaultOn;
    };

    inline constexpr int numBands = 9;

    namespace band { enum : int { hp, ls, b1, b2, b3, b4, b5, hs, lp }; }

    inline constexpr std::array<BandSpec, numBands> bandSpecs { {
        { "hp", "HP", "High-Pass",  BandType::highPass,  40.0f,    false },
        { "ls", "L",  "Low Shelf",  BandType::lowShelf,  40.0f,    true  },
        { "b1", "1",  "Band 1",     BandType::peak,      50.0f,    true  },
        { "b2", "2",  "Band 2",     BandType::peak,      200.0f,   true  },
        { "b3", "3",  "Band 3",     BandType::peak,      800.0f,   true  },
        { "b4", "4",  "Band 4",     BandType::peak,      3200.0f,  true  },
        { "b5", "5",  "Band 5",     BandType::peak,      12800.0f, true  },
        { "hs", "H",  "High Shelf", BandType::highShelf, 18000.0f, true  },
        { "lp", "LP", "Low-Pass",   BandType::lowPass,   18000.0f, false } } };

    /** Which part of a stereo signal a band works on: both channels as they are, only the mid (L+R), or only the side (L-R).
        Mono input is all mid, so a Side band does nothing there. */
    enum class StereoPart { stereo, mid, side };
    inline const juce::StringArray stereoPartNames { "Stereo", "Mid", "Side" };
    inline const char* stereoPartShort (StereoPart p) { return p == StereoPart::mid ? "M" : p == StereoPart::side ? "S" : ""; }

    constexpr bool hasGain (BandType t)  { return t == BandType::lowShelf || t == BandType::peak || t == BandType::highShelf; }
    constexpr bool hasQ (BandType t)     { return t == BandType::peak; }
    constexpr bool hasSlope (BandType t) { return t == BandType::highPass || t == BandType::lowPass; }

    inline constexpr float minFreq = 20.0f, maxFreq = 22000.0f;
    inline constexpr float minQ = 0.1f, maxQ = 40.0f, defaultQ = 2.0f;
    inline constexpr float maxBandGain = 48.0f;
    inline constexpr float minMaster = -60.0f, maxMaster = 24.0f;
    inline constexpr int numSlopes = 8, defaultSlope = 3; // index -> (index + 1) * 6 dB/oct
    inline constexpr double shelfQ = 0.7071;

    namespace ids
    {
        inline juce::String bandParam (int b, const char* suffix) { return juce::String (bandSpecs[(size_t) b].id) + "_" + suffix; }
        inline juce::String on (int b)    { return bandParam (b, "on"); }
        inline juce::String freq (int b)  { return bandParam (b, "freq"); }
        inline juce::String gain (int b)  { return bandParam (b, "gain"); }
        inline juce::String q (int b)     { return bandParam (b, "q"); }
        inline juce::String slope (int b) { return bandParam (b, "slope"); }
        inline juce::String stereo (int b) { return bandParam (b, "stereo"); } // Stereo / Mid / Side

        inline constexpr const char* master        = "master";
        inline constexpr const char* ultraQuiet    = "ultra_quiet";
        inline constexpr const char* constantWidth = "constant_width"; // display only: Q or bandwidth in octaves
        inline constexpr const char* range96       = "range_96";       // display only: graph spans ±15 or ±48 dB
        inline constexpr const char* analyser      = "analyser";
    }

    //==============================================================================
    inline double qToOctaves (double q) { return 2.0 / std::log (2.0) * std::asinh (1.0 / (2.0 * q)); }
    inline double octavesToQ (double n) { return 1.0 / (2.0 * std::sinh (std::log (2.0) * 0.5 * n)); }

    /** Up to maxDecimals places, trailing zeros dropped, never "-0". */
    inline juce::String formatNumber (double v, int maxDecimals)
    {
        const double scale = std::pow (10.0, maxDecimals);
        double r = std::round (v * scale) / scale;

        if (r == 0.0)
            r = 0.0;

        if (std::abs (r - std::round (r)) < 1e-9)
            return juce::String ((int) std::round (r));

        return juce::String (r, maxDecimals).trimCharactersAtEnd ("0");
    }

    inline juce::String formatFreq (float hz)  { return juce::String (juce::roundToInt (hz)) + " Hz"; }
    inline juce::String formatGain (float db)  { return formatNumber (db, 1) + " dB"; }
    inline juce::String formatQ (float q)      { return formatNumber (q, q < 10.0f ? 2 : 1); }
    inline juce::String formatWidth (float q)  { return formatNumber (qToOctaves (q), 2) + " oct"; }
    inline juce::String formatSlope (int index) { return juce::String ((index + 1) * 6) + " dB/Oct"; }

    /** Reads "1.2k", "1200 Hz", "-3dB" and so on. */
    inline std::optional<double> parseNumber (const juce::String& text)
    {
        const auto t = text.trim().toLowerCase();
        const auto digits = t.retainCharacters ("0123456789.-");

        if (digits.isEmpty() || ! digits.containsAnyOf ("0123456789"))
            return std::nullopt;

        return digits.getDoubleValue() * (t.containsChar ('k') ? 1000.0 : 1.0);
    }

    //==============================================================================
    inline juce::NormalisableRange<float> logRange (float lo, float hi)
    {
        return { lo, hi,
                 [] (float s, float e, float t) { return s * std::pow (e / s, t); },
                 [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
                 [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;

        auto textAttrs = [] (auto toText)
        {
            return AudioParameterFloatAttributes()
                .withStringFromValueFunction ([toText] (float v, int) { return toText (v); })
                .withValueFromStringFunction ([] (const String& s) { return (float) parseNumber (s).value_or (0.0); });
        };

        for (int b = 0; b < numBands; ++b)
        {
            const auto& spec = bandSpecs[(size_t) b];
            const String name (spec.name);

            layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::on (b), 1 }, name + " On", spec.defaultOn));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::freq (b), 1 }, name + " Frequency",
                                                               logRange (minFreq, maxFreq), spec.defaultFreq, textAttrs (formatFreq)));

            if (hasGain (spec.type))
                layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::gain (b), 1 }, name + " Gain",
                                                                   NormalisableRange<float> (-maxBandGain, maxBandGain, 0.01f), 0.0f,
                                                                   textAttrs (formatGain)));

            if (hasQ (spec.type))
                layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::q (b), 1 }, name + " Q",
                                                                   logRange (minQ, maxQ), defaultQ, textAttrs (formatQ)));

            if (hasSlope (spec.type))
            {
                StringArray slopes;
                for (int i = 0; i < numSlopes; ++i)
                    slopes.add (formatSlope (i));

                layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::slope (b), 1 }, name + " Slope", slopes, defaultSlope));
            }
        }

        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::master, 1 }, "Output Gain",
                                                           NormalisableRange<float> (minMaster, maxMaster, 0.01f), 0.0f, textAttrs (formatGain)));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::ultraQuiet, 1 }, "Ultra-Quiet", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::constantWidth, 1 }, "Constant Width", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::range96, 1 }, "96 dB Range", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::analyser, 1 }, "Analyzer", true));

        // Added in 1.1, after everything else so the older parameters keep their place in hosts that count them.
        for (int b = 0; b < numBands; ++b)
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::stereo (b), 2 }, String (bandSpecs[(size_t) b].name) + " Channels",
                                                                stereoPartNames, 0));
        return layout;
    }
}
