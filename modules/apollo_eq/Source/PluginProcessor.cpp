#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace apollo
{
    ApolloProcessor::ApolloProcessor()
        : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "ApolloEQ", createLayout())
    {
        for (int b = 0; b < numBands; ++b)
        {
            const auto type = bandSpecs[(size_t) b].type;
            auto& p = params[(size_t) b];
            p.on = apvts.getRawParameterValue (ids::on (b));
            p.freq = apvts.getRawParameterValue (ids::freq (b));
            p.gain = hasGain (type) ? apvts.getRawParameterValue (ids::gain (b)) : nullptr;
            p.q = hasQ (type) ? apvts.getRawParameterValue (ids::q (b)) : nullptr;
            p.slope = hasSlope (type) ? apvts.getRawParameterValue (ids::slope (b)) : nullptr;
            bands[(size_t) b].type = type;
        }

        masterParam = apvts.getRawParameterValue (ids::master);
        ultraQuietParam = apvts.getRawParameterValue (ids::ultraQuiet);
    }

    bool ApolloProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto& out = layouts.getMainOutputChannelSet();

        if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
            return false;

        return out == layouts.getMainInputChannelSet();
    }

    void ApolloProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        baseRate = sampleRate;
        maxBlock = std::max (1, samplesPerBlock);
        preparedChannels = juce::jlimit (1, dsp::BandFilter::maxChannels, getTotalNumOutputChannels());

        // Ultra-Quiet runs the filters at 2x so the curve near Nyquist keeps its analog shape.
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) preparedChannels, 1,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversampler->initProcessing ((size_t) maxBlock);

        monoScratch.assign ((size_t) maxBlock, 0.0f);
        ultraQuietActive = ultraQuietParam->load() > 0.5f;
        setLatencySamples (ultraQuietActive ? (int) std::lround (oversampler->getLatencyInSamples()) : 0);

        masterGain.reset (sampleRate, 0.02);
        masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (masterParam->load(), -200.0f));
        analyser.sampleRate = sampleRate;
        resetFilters();
    }

    void ApolloProcessor::readTargets()
    {
        for (int b = 0; b < numBands; ++b)
        {
            const auto& p = params[(size_t) b];
            auto& r = bands[(size_t) b];

            r.target.freq = p.freq->load();
            r.target.gainDb = p.gain != nullptr ? p.gain->load() : 0.0;
            r.target.q = p.q != nullptr ? p.q->load() : shelfQ;
            r.target.order = p.slope != nullptr ? (int) p.slope->load() + 1 : 2;
            r.targetMix = p.on->load() > 0.5f ? 1.0 : 0.0;

            if (r.target.order != r.current.order)
            {
                // A slope change alters the filter structure, so it can't glide.
                r.current.order = r.target.order;
                r.filter.reset();
                r.filter.configure (r.type, r.current, filterRate);
            }

            if (r.target.freq != r.current.freq || r.target.gainDb != r.current.gainDb
                || r.target.q != r.current.q || r.targetMix != r.mix)
                r.smoothing = true;
        }
    }

    void ApolloProcessor::resetFilters()
    {
        filterRate = baseRate * (ultraQuietActive ? 2.0 : 1.0);
        smoothCoeff = 1.0 - std::exp (-1.0 / (0.015 * filterRate)); // ~15 ms glide
        readTargets();

        for (auto& r : bands)
        {
            r.current = r.target;
            r.mix = r.targetMix;
            r.smoothing = false;
            r.filter.reset();
            r.filter.configure (r.type, r.current, filterRate);
        }

        if (oversampler != nullptr)
            oversampler->reset();
    }

    void ApolloProcessor::advance (BandRuntime& r, double fs)
    {
        bool done = true;

        auto approach = [this, &done] (double& cur, double target, double eps)
        {
            cur += (target - cur) * smoothCoeff;

            if (std::abs (target - cur) < eps)
                cur = target;
            else
                done = false;
        };

        // Frequency and Q glide on a log scale so sweeps sound even.
        double logFreq = std::log (r.current.freq), logQ = std::log (r.current.q);
        approach (logFreq, std::log (r.target.freq), 1e-5);
        approach (logQ, std::log (r.target.q), 1e-5);
        r.current.freq = logFreq == std::log (r.target.freq) ? r.target.freq : std::exp (logFreq);
        r.current.q = logQ == std::log (r.target.q) ? r.target.q : std::exp (logQ);
        approach (r.current.gainDb, r.target.gainDb, 1e-4);

        if (r.mix == 0.0 && r.targetMix > 0.0)
            r.filter.reset(); // coming back from bypass: start from silence, not stale state

        approach (r.mix, r.targetMix, 1e-4);

        r.filter.configure (r.type, r.current, fs);
        r.smoothing = ! done;
    }

    void ApolloProcessor::runFilters (juce::dsp::AudioBlock<float>& block, double fs)
    {
        const auto numCh = std::min ((int) block.getNumChannels(), dsp::BandFilter::maxChannels);
        const auto numSamples = block.getNumSamples();

        for (size_t i = 0; i < numSamples; ++i)
        {
            for (auto& r : bands)
                if (r.smoothing)
                    advance (r, fs);

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = block.getChannelPointer ((size_t) ch);
                double x = d[i];

                for (auto& r : bands)
                {
                    if (r.mix <= 0.0)
                        continue;

                    const double y = r.filter.process (ch, x);
                    x += r.mix * (y - x);
                }

                d[i] = (float) x;
            }
        }
    }

    void ApolloProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;
        const int total = buffer.getNumSamples();

        for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
            buffer.clear (ch, 0, total);

        if (oversampler == nullptr || total == 0)
            return;

        if (const bool uq = ultraQuietParam->load() > 0.5f; uq != ultraQuietActive)
        {
            ultraQuietActive = uq;
            setLatencySamples (uq ? (int) std::lround (oversampler->getLatencyInSamples()) : 0);
            resetFilters();
        }

        readTargets();
        masterGain.setTargetValue (juce::Decibels::decibelsToGain (masterParam->load(), -200.0f));

        const int numCh = std::min (buffer.getNumChannels(), preparedChannels);
        juce::dsp::AudioBlock<float> whole (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) total);
        const bool feedAnalyser = analyser.active.load();

        // Hosts may exceed the announced block size (and Audition alternates 1024 / 4), so work in chunks.
        for (int start = 0; start < total; start += maxBlock)
        {
            const int len = std::min (maxBlock, total - start);
            auto block = whole.getSubBlock ((size_t) start, (size_t) len);

            if (ultraQuietActive)
            {
                auto up = oversampler->processSamplesUp (block);
                runFilters (up, filterRate);
                oversampler->processSamplesDown (block);
            }
            else
            {
                runFilters (block, filterRate);
            }

            for (int i = 0; i < len; ++i)
            {
                const float g = masterGain.getNextValue();
                float sum = 0.0f;

                for (int ch = 0; ch < numCh; ++ch)
                {
                    auto& s = buffer.getWritePointer (ch)[start + i];
                    s *= g;
                    sum += s;
                }

                monoScratch[(size_t) i] = sum / (float) numCh;
            }

            if (feedAnalyser)
                analyser.push (monoScratch.data(), len);
        }
    }

    //==============================================================================
    void ApolloProcessor::getStateInformation (juce::MemoryBlock& dest)
    {
        auto state = apvts.copyState();
        state.setProperty ("presetModified", presets.isModified(), nullptr);

        if (const auto xml = state.createXml())
            copyXmlToBinary (*xml, dest);
    }

    void ApolloProcessor::setStateInformation (const void* data, int size)
    {
        if (const auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName (apvts.state.getType()))
            presets.restoreState (juce::ValueTree::fromXml (*xml));
    }

    juce::AudioProcessorEditor* ApolloProcessor::createEditor() { return new ApolloEditor (*this); }
}

