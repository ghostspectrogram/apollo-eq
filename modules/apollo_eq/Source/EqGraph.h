#pragma once

#include "PluginProcessor.h"
#include "ApolloLookAndFeel.h"

namespace apollo
{
    /** Response curve with draggable band nodes over a live output spectrum. */
    class EqGraph : public juce::Component, private juce::Timer
    {
    public:
        explicit EqGraph (ApolloProcessor&);
        ~EqGraph() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        int getSelectedBand() const { return selected; }
        void setSelectedBand (int b) { selected = b; repaint(); }
        std::function<void (int)> onSelectionChanged;

    private:
        struct BandParams
        {
            juce::RangedAudioParameter* on = nullptr;
            juce::RangedAudioParameter* freq = nullptr;
            juce::RangedAudioParameter* gain = nullptr;
            juce::RangedAudioParameter* q = nullptr;
            juce::RangedAudioParameter* slope = nullptr;
            juce::RangedAudioParameter* stereo = nullptr;
        };

        void timerCallback() override;
        void updateAnalyser();

        static float valueOf (const juce::RangedAudioParameter* p) { return p->convertFrom0to1 (p->getValue()); }
        static void set (juce::RangedAudioParameter* p, float v) { p->setValueNotifyingHost (p->convertTo0to1 (v)); }
        static void setGesture (juce::RangedAudioParameter* p, float v) { p->beginChangeGesture(); set (p, v); p->endChangeGesture(); }

        bool isOn (int b) const { return valueOf (params[(size_t) b].on) > 0.5f; }
        dsp::BandSettings settingsFor (int b) const;
        double bandDb (int b, double f) const;
        StereoPart partOf (int b) const { return (StereoPart) juce::jlimit (0, 2, juce::roundToInt (valueOf (params[(size_t) b].stereo))); }
        bool anyMidSide() const;
        double totalDb (double f, StereoPart which = StereoPart::mid) const; // the mid's response (all of it with no Mid / Side bands), or the side's
        double displayRange() const { return valueOf (range96) > 0.5f ? 48.0 : 15.0; }

        float xForFreq (double f) const;
        double freqForX (float x) const;
        float yForDb (double db) const;
        double dbForY (float y) const;
        juce::Point<float> nodePosition (int b) const;
        int bandAt (juce::Point<float>) const;
        void select (int b);
        void showBandMenu (int b);
        juce::String describe (int b) const;

        ApolloProcessor& proc;
        std::array<BandParams, numBands> params;
        juce::RangedAudioParameter* range96 = nullptr;
        juce::RangedAudioParameter* analyserOn = nullptr;
        juce::RangedAudioParameter* constantWidth = nullptr;
        juce::Rectangle<float> plot;

        int selected = -1, hovered = -1, dragging = -1;
        juce::Point<float> dragPos;
        juce::Point<float> lastMouse;

        static constexpr int fftOrder = 12, fftSize = 1 << fftOrder, hop = 1024;
        juce::dsp::FFT fft { fftOrder };
        juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false };
        std::vector<float> history = std::vector<float> (fftSize, 0.0f);
        std::vector<float> incoming = std::vector<float> (1 << 15, 0.0f);
        std::vector<float> fftData = std::vector<float> (2 * fftSize, 0.0f);
        std::vector<float> spectrumDb = std::vector<float> (fftSize / 2 + 1, -120.0f);
        int pendingSamples = 0;
        bool hasSpectrum = false;
    };
}
