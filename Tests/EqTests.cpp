// Runs the real processor the way Adobe Audition does (1024-sample blocks alternating with
// 4-sample blocks) and checks the measured response against the model the graph draws.

#include <apollo_eq/apollo_eq.h>
#include <iostream>

using namespace apollo;

namespace
{
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        if (! ok)
        {
            ++failures;
            std::cout << "  FAIL  " << what << "\n";
        }
    }

    void setParam (ApolloProcessor& p, const juce::String& id, float v)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (v));
    }

    float getParam (ApolloProcessor& p, const juce::String& id)
    {
        auto* param = p.apvts.getParameter (id);
        return param->convertFrom0to1 (param->getValue());
    }

    /** Processes the buffer with Audition's block pattern, or fixed blocks of blockSize. */
    void run (ApolloProcessor& p, juce::AudioBuffer<float>& buffer, int blockSize = 0)
    {
        juce::MidiBuffer midi;
        int pos = 0, n = 0;

        while (pos < buffer.getNumSamples())
        {
            const int want = blockSize > 0 ? blockSize : (n++ % 2 == 0 ? 1024 : 4);
            const int len = std::min (want, buffer.getNumSamples() - pos);
            juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), pos, len);
            p.processBlock (view, midi);
            pos += len;
        }
    }

    std::unique_ptr<ApolloProcessor> makeProcessor (int preset, bool ultraQuiet, double fs)
    {
        auto p = std::make_unique<ApolloProcessor>();
        p->presets.loadFactory (preset);
        setParam (*p, ids::ultraQuiet, ultraQuiet ? 1.0f : 0.0f);
        p->prepareToPlay (fs, 1024);
        return p;
    }

    double measureDb (ApolloProcessor& p, double f, double fs)
    {
        const int total = (int) (fs * 1.0), settle = (int) (fs * 0.6);
        juce::AudioBuffer<float> buf (2, total);

        for (int i = 0; i < total; ++i)
            for (int ch = 0; ch < 2; ++ch)
                buf.setSample (ch, i, 0.1f * (float) std::sin (2.0 * dsp::pi * f * i / fs));

        run (p, buf);

        double in = 0, out = 0;

        for (int i = settle; i < total; ++i)
        {
            const double x = 0.1 * std::sin (2.0 * dsp::pi * f * i / fs);
            in += x * x;
            out += (double) buf.getSample (0, i) * buf.getSample (0, i);
        }

        return 10.0 * std::log10 (out / in);
    }

    double modelDb (ApolloProcessor& p, double f, double filterRate)
    {
        double db = getParam (p, ids::master);

        for (int b = 0; b < numBands; ++b)
        {
            const auto type = bandSpecs[(size_t) b].type;

            if (getParam (p, ids::on (b)) < 0.5f)
                continue;

            dsp::BandSettings s;
            s.freq = getParam (p, ids::freq (b));
            s.gainDb = hasGain (type) ? getParam (p, ids::gain (b)) : 0.0;
            s.q = hasQ (type) ? getParam (p, ids::q (b)) : shelfQ;
            s.order = hasSlope (type) ? juce::roundToInt (getParam (p, ids::slope (b))) + 1 : 2;
            db += dsp::magnitudeDb (type, s, f, filterRate);
        }

        return db;
    }

    void testResponse (bool ultraQuiet)
    {
        std::cout << "Response vs model, Ultra-Quiet " << (ultraQuiet ? "on" : "off") << "\n";
        const double fs = 44100.0;
        const double freqs[] = { 40, 80, 150, 300, 600, 1000, 1700, 2600, 4000, 7000, 10000, 15000 };
        double worst = 0;

        for (int preset = 0; preset < (int) factoryPresets().size(); ++preset)
        {
            for (double f : freqs)
            {
                if (ultraQuiet && f > 12000)
                    continue; // the half-band decimator rolls off near Nyquist

                auto p = makeProcessor (preset, ultraQuiet, fs);
                const double expected = modelDb (*p, f, ultraQuiet ? fs * 2 : fs);

                if (expected < -45.0)
                    continue; // below the sine's measurement floor

                const double got = measureDb (*p, f, fs);
                const double err = std::abs (got - expected);
                worst = std::max (worst, err);
                check (err < (ultraQuiet ? 0.35 : 0.1), factoryPresets()[(size_t) preset].name + " @ " + juce::String (f) + " Hz: got "
                                                            + juce::String (got, 2) + " dB, model " + juce::String (expected, 2) + " dB");
            }
        }

        std::cout << "  worst error " << juce::String (worst, 3) << " dB\n";
    }

    void testBlockIndependence()
    {
        std::cout << "Block-size independence\n";

        for (bool uq : { false, true })
        {
            juce::Random rng (42);
            juce::AudioBuffer<float> a (2, 44100);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < a.getNumSamples(); ++i)
                    a.setSample (ch, i, rng.nextFloat() * 0.5f - 0.25f);

            juce::AudioBuffer<float> b (a);
            auto p1 = makeProcessor (1, uq, 44100.0), p2 = makeProcessor (1, uq, 44100.0);
            run (*p1, a, 512);
            run (*p2, b);

            float maxDiff = 0;

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < a.getNumSamples(); ++i)
                    maxDiff = std::max (maxDiff, std::abs (a.getSample (ch, i) - b.getSample (ch, i)));

            // The oversampler's filters run in float and see different chunk lengths, so allow rounding there.
            check (maxDiff < (uq ? 1e-5f : 1e-7f), "Vocal Enhancer, UQ " + juce::String (uq ? "on" : "off") + ": max diff " + juce::String (maxDiff));
        }
    }

    void testExtremes()
    {
        std::cout << "Extreme settings and automation stay finite\n";
        auto p = makeProcessor (0, false, 44100.0);
        juce::Random rng (7);
        juce::AudioBuffer<float> buf (2, 4096);

        for (int round = 0; round < 200; ++round)
        {
            for (int b = 0; b < numBands; ++b)
            {
                const auto type = bandSpecs[(size_t) b].type;
                setParam (*p, ids::on (b), rng.nextBool() ? 1.0f : 0.0f);
                setParam (*p, ids::freq (b), rng.nextBool() ? maxFreq : minFreq + rng.nextFloat() * (maxFreq - minFreq));

                if (hasGain (type))  setParam (*p, ids::gain (b), rng.nextBool() ? maxBandGain : -maxBandGain);
                if (hasQ (type))     setParam (*p, ids::q (b), rng.nextBool() ? maxQ : minQ);
                if (hasSlope (type)) setParam (*p, ids::slope (b), (float) rng.nextInt (numSlopes));
            }

            setParam (*p, ids::ultraQuiet, rng.nextBool() ? 1.0f : 0.0f);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < buf.getNumSamples(); ++i)
                    buf.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);

            run (*p, buf);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < buf.getNumSamples(); ++i)
                    if (! std::isfinite (buf.getSample (ch, i)))
                    {
                        check (false, "non-finite output in round " + juce::String (round));
                        return;
                    }
        }
    }

    void testState()
    {
        std::cout << "State save / restore and presets\n";

        for (int preset = 0; preset < (int) factoryPresets().size(); ++preset)
        {
            ApolloProcessor a;
            a.presets.loadFactory (preset);
            check (! a.presets.isModified(), "preset load marks modified");
            check (a.presets.getCurrentName() == factoryPresets()[(size_t) preset].name, "preset name");

            juce::MemoryBlock state;
            a.getStateInformation (state);
            ApolloProcessor b;
            b.setStateInformation (state.getData(), (int) state.getSize());

            for (auto* param : a.getParameters())
            {
                auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param);
                const float va = rp->getValue(), vb = b.apvts.getParameter (rp->getParameterID())->getValue();
                check (std::abs (va - vb) < 1e-6f, "restored " + rp->getParameterID());
            }

            check (b.presets.getCurrentName() == a.presets.getCurrentName() && ! b.presets.isModified(), "restored preset name");
        }

        ApolloProcessor c;
        c.presets.loadFactory (2);
        setParam (c, ids::gain (band::b3), 1.0f);
        check (c.presets.isModified(), "edit marks preset modified");

        const auto walkie = factoryPresets()[2];
        check (walkie.name == "Walkie Talkie", "Walkie Talkie preset present");
    }

    void testLatency()
    {
        std::cout << "Latency reporting\n";
        auto p = makeProcessor (0, false, 48000.0);
        check (p->getLatencySamples() == 0, "no latency without Ultra-Quiet");
        auto q = makeProcessor (0, true, 48000.0);
        check (q->getLatencySamples() > 0 && q->getLatencySamples() < 16, "Ultra-Quiet latency " + juce::String (q->getLatencySamples()));
    }
}

/** ApolloTests --snapshot <out.png> <preset index>: renders the editor, feeding pink-ish noise to the analyser. */
static int snapshot (const juce::String& path, int preset)
{
    auto p = makeProcessor (preset, false, 44100.0);
    std::unique_ptr<juce::AudioProcessorEditor> editor (p->createEditor());
    p->analyser.active = true;

    juce::Random rng (3);
    juce::AudioBuffer<float> buf (2, 44100);
    float b0 = 0, b1 = 0;

    for (int i = 0; i < buf.getNumSamples(); ++i)
    {
        const float w = rng.nextFloat() * 2.0f - 1.0f;
        b0 = 0.99f * b0 + 0.05f * w;
        b1 = 0.6f * b1 + 0.3f * w;
        buf.setSample (0, i, 0.4f * (b0 + b1));
        buf.setSample (1, i, 0.4f * (b0 + b1));
    }

    run (*p, buf);

    for (int i = 0; i < 4; ++i) // let the graph's timer pull the analyser data
        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    juce::File file (path);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat().writeImageToStream (image, out);
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;

    if (argc >= 4 && juce::String (argv[1]) == "--snapshot")
        return snapshot (argv[2], juce::String (argv[3]).getIntValue());

    testState();
    testLatency();
    testBlockIndependence();
    testExtremes();
    testResponse (false);
    testResponse (true);

    std::cout << (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILURE(S)") << "\n";
    return failures == 0 ? 0 : 1;
}
