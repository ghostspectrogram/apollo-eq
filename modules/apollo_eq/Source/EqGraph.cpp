#include "EqGraph.h"

namespace apollo
{
    namespace
    {
        constexpr double graphMinFreq = 20.0, graphMaxFreq = 22000.0;
        constexpr float analyserTop = 6.0f, analyserBottom = -84.0f;
        constexpr float nodeRadius = 5.5f, hitRadius = 11.0f;
    }

    EqGraph::EqGraph (ApolloProcessor& p) : proc (p)
    {
        auto get = [this] (const juce::String& id) { return proc.apvts.getParameter (id); };

        for (int b = 0; b < numBands; ++b)
        {
            const auto type = bandSpecs[(size_t) b].type;
            auto& bp = params[(size_t) b];
            bp.on = get (ids::on (b));
            bp.freq = get (ids::freq (b));
            bp.gain = hasGain (type) ? get (ids::gain (b)) : nullptr;
            bp.q = hasQ (type) ? get (ids::q (b)) : nullptr;
            bp.slope = hasSlope (type) ? get (ids::slope (b)) : nullptr;
            bp.stereo = get (ids::stereo (b));
        }

        range96 = get (ids::range96);
        analyserOn = get (ids::analyser);
        constantWidth = get (ids::constantWidth);
        setOpaque (true);
        startTimerHz (30);
    }

    EqGraph::~EqGraph()
    {
        proc.analyser.active = false;
    }

    void EqGraph::resized()
    {
        plot = getLocalBounds().toFloat().withTrimmedRight (30.0f).withTrimmedBottom (20.0f).reduced (1.0f);
    }

    //==============================================================================
    dsp::BandSettings EqGraph::settingsFor (int b) const
    {
        const auto& bp = params[(size_t) b];
        dsp::BandSettings s;
        s.freq = valueOf (bp.freq);
        s.gainDb = bp.gain != nullptr ? valueOf (bp.gain) : 0.0;
        s.q = bp.q != nullptr ? valueOf (bp.q) : shelfQ;
        s.order = bp.slope != nullptr ? juce::roundToInt (valueOf (bp.slope)) + 1 : 2;
        return s;
    }

    double EqGraph::bandDb (int b, double f) const
    {
        // Ultra-Quiet runs the filters at twice the host rate; the curve follows what is really running.
        const double fs = proc.analyser.sampleRate.load() * (valueOf (proc.apvts.getParameter (ids::ultraQuiet)) > 0.5f ? 2.0 : 1.0);
        return dsp::magnitudeDb (bandSpecs[(size_t) b].type, settingsFor (b), f, fs);
    }

    bool EqGraph::anyMidSide() const
    {
        for (int b = 0; b < numBands; ++b)
            if (isOn (b) && partOf (b) != StereoPart::stereo)
                return true;

        return false;
    }

    double EqGraph::totalDb (double f, StereoPart which) const
    {
        double db = 0.0;

        for (int b = 0; b < numBands; ++b)
            if (isOn (b) && (partOf (b) == StereoPart::stereo || partOf (b) == which))
                db += bandDb (b, f);

        return db;
    }

    float EqGraph::xForFreq (double f) const
    {
        const double t = std::log (juce::jlimit (graphMinFreq, graphMaxFreq, f) / graphMinFreq) / std::log (graphMaxFreq / graphMinFreq);
        return plot.getX() + (float) t * plot.getWidth();
    }

    double EqGraph::freqForX (float x) const
    {
        const double t = (double) ((x - plot.getX()) / plot.getWidth());
        return graphMinFreq * std::pow (graphMaxFreq / graphMinFreq, t);
    }

    float EqGraph::yForDb (double db) const
    {
        const double r = displayRange();
        return juce::jmap ((float) juce::jlimit (-r * 1.2, r * 1.2, db), (float) -r, (float) r, plot.getBottom() - 8.0f, plot.getY() + 8.0f);
    }

    double EqGraph::dbForY (float y) const
    {
        const double r = displayRange();
        return juce::jmap ((double) y, (double) plot.getBottom() - 8.0, (double) plot.getY() + 8.0, -r, r);
    }

    juce::Point<float> EqGraph::nodePosition (int b) const
    {
        const auto type = bandSpecs[(size_t) b].type;
        const double f = valueOf (params[(size_t) b].freq);
        const double db = hasGain (type) ? valueOf (params[(size_t) b].gain) : bandDb (b, f);
        return { xForFreq (f), yForDb (db) };
    }

    int EqGraph::bandAt (juce::Point<float> pos) const
    {
        int best = -1;
        float bestDist = hitRadius;

        for (int b = 0; b < numBands; ++b)
        {
            // Enabled bands win ties, so a hidden HP under the low shelf doesn't steal the click.
            const float d = nodePosition (b).getDistanceFrom (pos) + (isOn (b) ? 0.0f : 3.0f);

            if (d < bestDist)
            {
                bestDist = d;
                best = b;
            }
        }

        return best;
    }

    juce::String EqGraph::describe (int b) const
    {
        const auto& spec = bandSpecs[(size_t) b];
        const auto& bp = params[(size_t) b];
        juce::String s = juce::String (spec.name) + "   " + formatFreq (valueOf (bp.freq));

        if (bp.gain != nullptr)  s << "   " << formatGain (valueOf (bp.gain));
        if (bp.q != nullptr)     s << "   " << (valueOf (constantWidth) > 0.5f ? formatWidth (valueOf (bp.q)) : "Q " + formatQ (valueOf (bp.q)));
        if (bp.slope != nullptr) s << "   " << formatSlope (juce::roundToInt (valueOf (bp.slope)));
        if (partOf (b) != StereoPart::stereo) s << "   " << stereoPartNames[(int) partOf (b)] << " only";
        if (! isOn (b))          s << "   (off)";

        return s;
    }

    void EqGraph::select (int b)
    {
        if (b == selected)
            return;

        selected = b;

        if (onSelectionChanged)
            onSelectionChanged (b);
    }

    //==============================================================================
    void EqGraph::timerCallback()
    {
        const bool wantAnalyser = valueOf (analyserOn) > 0.5f && isShowing();
        proc.analyser.active = wantAnalyser;

        if (wantAnalyser)
            updateAnalyser();

        repaint();
    }

    void EqGraph::updateAnalyser()
    {
        const int n = proc.analyser.pull (incoming.data(), (int) incoming.size());

        if (n > 0)
        {
            const int keep = std::min (n, fftSize);
            std::move (history.begin() + keep, history.end(), history.begin());
            std::copy_n (incoming.begin() + (n - keep), keep, history.end() - keep);
            pendingSamples += n;
        }

        for (auto& v : spectrumDb) // release
            v -= 1.0f;

        if (pendingSamples < hop)
            return;

        pendingSamples = 0;
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        std::copy (history.begin(), history.end(), fftData.begin());
        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

        const float norm = 4.0f / (float) fftSize; // Hann coherent gain: full-scale sine reads 0 dB

        for (size_t k = 0; k < spectrumDb.size(); ++k)
            spectrumDb[k] = std::max (spectrumDb[k], juce::Decibels::gainToDecibels (fftData[k] * norm, -140.0f));

        hasSpectrum = true;
    }

    //==============================================================================
    void EqGraph::paint (juce::Graphics& g)
    {
        using namespace colours;
        g.fillAll (panel);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0f13), 0.0f, plot.getY(), juce::Colour (0xff07080a), 0.0f, plot.getBottom(), false));
        g.fillRect (plot);

        // Frequency grid and labels, as Audition marks them.
        g.setFont (juce::FontOptions (10.5f));

        for (double decade = 10.0; decade <= 10000.0; decade *= 10.0)
        {
            for (int m = 1; m <= 9; ++m)
            {
                const double f = decade * m;

                if (f < graphMinFreq || f > graphMaxFreq)
                    continue;

                const float x = xForFreq (f);
                g.setColour (m == 1 ? edge : grid);
                g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
            }
        }

        const std::pair<double, const char*> freqLabels[] = {
            { 30, "30" }, { 40, "40" }, { 50, "50" }, { 60, "60" }, { 80, "80" }, { 100, "100" }, { 200, "200" },
            { 300, "300" }, { 400, "400" }, { 500, "500" }, { 700, "700" }, { 1000, "1k" }, { 2000, "2k" }, { 3000, "3k" },
            { 4000, "4k" }, { 5000, "5k" }, { 7000, "7k" }, { 10000, "10k" }, { 20000, "20k" } };

        g.setColour (textDim);
        g.drawText ("Hz", juce::Rectangle<float> (plot.getX() + 2.0f, plot.getBottom() + 3.0f, 20.0f, 14.0f), juce::Justification::centredLeft);

        for (const auto& [f, label] : freqLabels)
        {
            const bool major = f == 100 || f == 1000 || f == 10000;
            g.setColour (major ? text : textDim);
            g.drawText (label, juce::Rectangle<float> (40.0f, 14.0f).withCentre ({ xForFreq (f), plot.getBottom() + 10.0f }), juce::Justification::centred);
        }

        // dB grid and labels on the right.
        const double r = displayRange();
        const double step = r > 20.0 ? 12.0 : 5.0;

        for (double db = -std::floor (r / step) * step; db <= r + 0.01; db += step)
        {
            const float y = yForDb (db);
            g.setColour (std::abs (db) < 0.01 ? edge.brighter (0.25f) : grid);
            g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
            g.setColour (std::abs (db) < 0.01 ? text : textDim);
            g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (plot.getRight() + 3.0f, y - 7.0f, 26.0f, 14.0f), juce::Justification::centredLeft);
        }

        g.setColour (textDim);
        g.drawText ("dB", juce::Rectangle<float> (plot.getRight() + 3.0f, plot.getBottom() + 3.0f, 26.0f, 14.0f), juce::Justification::centredLeft);

        g.saveState();
        g.reduceClipRegion (plot.toNearestInt());

        // Live output spectrum, tilted +3 dB/oct so speech and music read flat.
        if (hasSpectrum && valueOf (analyserOn) > 0.5f)
        {
            const double fs = proc.analyser.sampleRate.load();
            juce::Path spectrum;
            spectrum.startNewSubPath (plot.getX(), plot.getBottom());

            for (float x = plot.getX(); x <= plot.getRight() + 1.0f; x += 2.0f)
            {
                const double lo = freqForX (x - 1.0f) * fftSize / fs, hi = freqForX (x + 1.0f) * fftSize / fs;
                const int maxBin = (int) spectrumDb.size() - 1;
                float db;

                if (hi - lo < 1.0)
                {
                    const double centre = juce::jlimit (0.0, (double) maxBin - 1.0, 0.5 * (lo + hi));
                    const int i = (int) centre;
                    const float frac = (float) (centre - i);
                    db = spectrumDb[(size_t) i] + frac * (spectrumDb[(size_t) i + 1] - spectrumDb[(size_t) i]);
                }
                else
                {
                    db = -140.0f;
                    for (int i = juce::jlimit (0, maxBin, (int) lo); i <= juce::jlimit (0, maxBin, (int) hi); ++i)
                        db = std::max (db, spectrumDb[(size_t) i]);
                }

                db += 3.0f * (float) std::log2 (freqForX (x) / 1000.0);
                spectrum.lineTo (x, juce::jmap (juce::jlimit (analyserBottom, analyserTop, db), analyserBottom, analyserTop, plot.getBottom(), plot.getY()));
            }

            spectrum.lineTo (plot.getRight(), plot.getBottom());
            spectrum.closeSubPath();
            g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.20f), 0.0f, plot.getY(), accent.withAlpha (0.03f), 0.0f, plot.getBottom(), false));
            g.fillPath (spectrum);
            g.setColour (accent.withAlpha (0.25f));
            g.strokePath (spectrum, juce::PathStrokeType (1.0f));
        }

        // Selected band's own contribution, shaded from the 0 dB line.
        const float zeroY = yForDb (0.0);

        if (juce::isPositiveAndBelow (selected, numBands) && isOn (selected))
        {
            juce::Path area;
            area.startNewSubPath (plot.getX(), zeroY);

            for (float x = plot.getX(); x <= plot.getRight() + 1.0f; x += 2.0f)
                area.lineTo (x, yForDb (bandDb (selected, freqForX (x))));

            area.lineTo (plot.getRight(), zeroY);
            area.closeSubPath();
            g.setColour (accentSoft.withAlpha (0.10f));
            g.fillPath (area);
        }

        // Combined response. With Mid or Side bands: the mid's (blue) and the side's (orange), each named at the top left.
        auto responsePath = [this] (StereoPart which)
        {
            juce::Path curve;

            for (float x = plot.getX(); x <= plot.getRight() + 1.0f; x += 1.5f)
            {
                const float y = yForDb (totalDb (freqForX (x), which));

                if (x == plot.getX())
                    curve.startNewSubPath (x, y);
                else
                    curve.lineTo (x, y);
            }

            return curve;
        };

        const bool split = anyMidSide();

        if (split)
        {
            const auto sideCurve = responsePath (StereoPart::side);
            g.setColour (side.withAlpha (0.16f));
            g.strokePath (sideCurve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (side);
            g.strokePath (sideCurve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const auto curve = responsePath (StereoPart::mid);
        g.setColour (accent.withAlpha (0.18f)); // glow
        g.strokePath (curve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (split)
        {
            g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
            g.setColour (accent);
            g.drawText ("MID", juce::Rectangle<float> (plot.getX() + 10.0f, plot.getY() + 8.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
            g.setColour (side);
            g.drawText ("SIDE", juce::Rectangle<float> (plot.getX() + 44.0f, plot.getY() + 8.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
        }

        // Nodes.
        for (int b = 0; b < numBands; ++b)
        {
            const bool on = isOn (b);
            const auto pos = nodePosition (b);
            const bool hot = b == selected || b == hovered || b == dragging;
            const auto node = juce::Rectangle<float> (nodeRadius * 2.0f, nodeRadius * 2.0f).withCentre (pos);

            if (on)
            {
                if (hot)
                {
                    g.setColour (accent.withAlpha (0.35f));
                    g.fillEllipse (node.expanded (4.0f));
                }

                g.setColour (b == selected ? accentSoft : bright);
                g.fillEllipse (node);
                g.setColour (black);
                g.drawEllipse (node, 1.0f);
            }
            else
            {
                g.setColour (textDim.withAlpha (hot ? 0.9f : 0.45f));
                g.drawEllipse (node.reduced (0.5f), 1.2f);
            }

            g.setColour (on ? (hot ? bright : text) : textDim.withAlpha (0.5f));
            g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
            g.drawText (juce::String (bandSpecs[(size_t) b].label) + stereoPartShort (partOf (b)),
                        juce::Rectangle<float> (30.0f, 14.0f).withCentre (pos.translated (0.0f, -15.0f)), juce::Justification::centred);
        }

        g.restoreState();

        // Readout for the band under the mouse or being dragged.
        if (const int b = dragging >= 0 ? dragging : hovered; b >= 0)
        {
            const auto label = describe (b);
            const juce::Font font (juce::FontOptions (12.0f));
            const float w = (float) juce::GlyphArrangement::getStringWidthInt (font, label) + 18.0f;
            auto pos = nodePosition (b);
            auto box = juce::Rectangle<float> (w, 22.0f).withCentre (pos.translated (0.0f, pos.y > plot.getCentreY() ? -38.0f : 30.0f));
            box = box.constrainedWithin (plot.reduced (4.0f));

            g.setColour (raised.withAlpha (0.95f));
            g.fillRoundedRectangle (box, 5.0f);
            g.setColour (edge);
            g.drawRoundedRectangle (box, 5.0f, 1.0f);
            g.setColour (bright);
            g.setFont (font);
            g.drawText (label, box, juce::Justification::centred);
        }

        g.setColour (edge);
        g.drawRect (plot, 1.0f);
    }

    //==============================================================================
    void EqGraph::mouseMove (const juce::MouseEvent& e)
    {
        const int b = bandAt (e.position);

        if (b != hovered)
        {
            hovered = b;
            setMouseCursor (b >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void EqGraph::mouseExit (const juce::MouseEvent&)
    {
        hovered = -1;
        repaint();
    }

    void EqGraph::mouseDown (const juce::MouseEvent& e)
    {
        const int b = bandAt (e.position);

        if (b < 0)
            return;

        select (b);

        if (e.mods.isPopupMenu())
        {
            showBandMenu (b);
            return;
        }

        if (! isOn (b))
            setGesture (params[(size_t) b].on, 1.0f); // grabbing a sleeping band wakes it

        dragging = b;
        dragPos = nodePosition (b);
        lastMouse = e.position;
        params[(size_t) b].freq->beginChangeGesture();

        if (params[(size_t) b].gain != nullptr)
            params[(size_t) b].gain->beginChangeGesture();
    }

    void EqGraph::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragging < 0)
            return;

        const float scale = e.mods.isShiftDown() ? 0.15f : 1.0f;
        dragPos += (e.position - lastMouse) * scale;
        lastMouse = e.position;

        const auto& bp = params[(size_t) dragging];
        set (bp.freq, (float) juce::jlimit ((double) minFreq, (double) maxFreq, freqForX (juce::jlimit (plot.getX(), plot.getRight(), dragPos.x))));

        if (bp.gain != nullptr)
        {
            const double r = displayRange();
            set (bp.gain, (float) juce::jlimit (-r, r, dbForY (dragPos.y)));
        }
    }

    void EqGraph::mouseUp (const juce::MouseEvent&)
    {
        if (dragging < 0)
            return;

        params[(size_t) dragging].freq->endChangeGesture();

        if (params[(size_t) dragging].gain != nullptr)
            params[(size_t) dragging].gain->endChangeGesture();

        dragging = -1;
    }

    void EqGraph::mouseDoubleClick (const juce::MouseEvent& e)
    {
        if (const int b = bandAt (e.position); b >= 0)
        {
            // Flatten a gain band; switch a pass filter off.
            if (params[(size_t) b].gain != nullptr)
                setGesture (params[(size_t) b].gain, 0.0f);
            else
                setGesture (params[(size_t) b].on, 0.0f);

            return;
        }

        if (! plot.contains (e.position))
            return;

        // Empty space: put the first unused bell band here.
        for (int b = band::b1; b <= band::b5; ++b)
        {
            const auto& bp = params[(size_t) b];

            if (! isOn (b) || std::abs (valueOf (bp.gain)) < 0.01f)
            {
                setGesture (bp.freq, (float) juce::jlimit ((double) minFreq, (double) maxFreq, freqForX (e.position.x)));
                setGesture (bp.gain, (float) juce::jlimit (-displayRange(), displayRange(), dbForY (e.position.y)));
                setGesture (bp.on, 1.0f);
                select (b);
                return;
            }
        }
    }

    void EqGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
    {
        int b = bandAt (e.position);

        if (b < 0)
            b = selected;

        if (b < 0)
            return;

        const auto& bp = params[(size_t) b];
        const float delta = (w.isReversed ? -1.0f : 1.0f) * (w.deltaY != 0.0f ? w.deltaY : w.deltaX);

        if (bp.q != nullptr)
            setGesture (bp.q, juce::jlimit (minQ, maxQ, valueOf (bp.q) * std::pow (1.25f, delta * 4.0f)));
        else if (bp.slope != nullptr && std::abs (delta) > 0.0f)
            setGesture (bp.slope, (float) juce::jlimit (0, numSlopes - 1, juce::roundToInt (valueOf (bp.slope)) + (delta > 0 ? 1 : -1)));
    }

    void EqGraph::showBandMenu (int b)
    {
        const auto& bp = params[(size_t) b];
        juce::PopupMenu menu;
        menu.setLookAndFeel (&getLookAndFeel());
        menu.addSectionHeader (bandSpecs[(size_t) b].name);
        menu.addItem (1, "Enabled", true, isOn (b));

        if (bp.slope != nullptr)
        {
            menu.addSeparator();
            const int current = juce::roundToInt (valueOf (bp.slope));

            for (int i = 0; i < numSlopes; ++i)
                menu.addItem (100 + i, formatSlope (i), true, i == current);
        }

        menu.addSeparator();

        for (int i = 0; i < 3; ++i)
            menu.addItem (200 + i, i == 0 ? juce::String ("Stereo (both channels)") : stereoPartNames[i] + " only", true, (int) partOf (b) == i);

        menu.addSeparator();
        menu.addItem (2, "Reset Band");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
            [this, b, safe = juce::Component::SafePointer<EqGraph> (this)] (int result)
            {
                if (safe == nullptr || result == 0)
                    return;

                const auto& p = params[(size_t) b];

                if (result == 1)
                    setGesture (p.on, isOn (b) ? 0.0f : 1.0f);
                else if (result >= 200)
                    setGesture (p.stereo, (float) (result - 200));
                else if (result >= 100)
                    setGesture (p.slope, (float) (result - 100));
                else if (result == 2)
                    for (auto* param : { p.freq, p.gain, p.q, p.slope, p.stereo })
                        if (param != nullptr)
                            setGesture (param, param->convertFrom0to1 (param->getDefaultValue()));
            });
    }
}
