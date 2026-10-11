#pragma once

#include "EqDsp.h"
#include "Presets.h"
#include <juce_dsp/juce_dsp.h>

namespace apollo
{
    /** Lock-free hand-off of the output (mono sum) to the editor's analyser. */
    class AnalyserFifo
    {
    public:
        void push (const float* data, int num)
        {
            const auto scope = fifo.write (num);

            if (scope.blockSize1 > 0) std::copy_n (data, scope.blockSize1, buffer.begin() + scope.startIndex1);
            if (scope.blockSize2 > 0) std::copy_n (data + scope.blockSize1, scope.blockSize2, buffer.begin() + scope.startIndex2);
        }

        int pull (float* dest, int max)
        {
            const auto scope = fifo.read (max);

            if (scope.blockSize1 > 0) std::copy_n (buffer.begin() + scope.startIndex1, scope.blockSize1, dest);
            if (scope.blockSize2 > 0) std::copy_n (buffer.begin() + scope.startIndex2, scope.blockSize2, dest + scope.blockSize1);

            return scope.blockSize1 + scope.blockSize2;
        }

        std::atomic<bool> active { false };
        std::atomic<double> sampleRate { 44100.0 };

    private:
        static constexpr int size = 1 << 15;
        juce::AbstractFifo fifo { size };
        std::vector<float> buffer = std::vector<float> (size);
    };

    //==============================================================================
    class ApolloProcessor : public juce::AudioProcessor
    {
    public:
        ApolloProcessor();
        ~ApolloProcessor() override = default;

        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override;
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
        using AudioProcessor::processBlock;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }

        const juce::String getName() const override { return "Apollo EQ"; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }

        // Presets live in the editor's own browser. A host program list would let hosts that
        // call setCurrentProgram (0) on load overwrite the restored state.
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock&) override;
        void setStateInformation (const void*, int) override;

        juce::AudioProcessorValueTreeState apvts;
        PresetManager presets { apvts };
        AnalyserFifo analyser;

    private:
        struct BandParams
        {
            std::atomic<float>* on = nullptr;
            std::atomic<float>* freq = nullptr;
            std::atomic<float>* gain = nullptr;
            std::atomic<float>* q = nullptr;
            std::atomic<float>* slope = nullptr;
        };

        struct BandRuntime
        {
            BandType type = BandType::peak;
            dsp::BandFilter filter;
            dsp::BandSettings current, target;
            double mix = 0.0, targetMix = 0.0;
            bool smoothing = false;
        };

        void readTargets();
        void resetFilters();
        void advance (BandRuntime&, double fs);
        void runFilters (juce::dsp::AudioBlock<float>&, double fs);

        std::array<BandParams, numBands> params;
        std::array<BandRuntime, numBands> bands;
        std::atomic<float>* masterParam = nullptr;
        std::atomic<float>* ultraQuietParam = nullptr;

        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> masterGain;
        std::vector<float> monoScratch;
        double baseRate = 44100.0, filterRate = 44100.0, smoothCoeff = 0.01;
        int maxBlock = 512, preparedChannels = 2;
        bool ultraQuietActive = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApolloProcessor)
    };
}
