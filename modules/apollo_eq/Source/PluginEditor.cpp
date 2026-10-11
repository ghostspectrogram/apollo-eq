#include "PluginEditor.h"

namespace apollo
{
    namespace
    {
        constexpr int userIdBase = 1000;
        const char* rowLabels[] = { "Frequency", "Gain", "Q / Width", "Band" };
    }

    ApolloEditor::ApolloEditor (ApolloProcessor& p)
        : AudioProcessorEditor (p), proc (p),
          analyserAttachment (param (ids::analyser), analyserButton),
          masterAttachment (param (ids::master), masterSlider),
          masterBox (param (ids::master)),
          graph (p),
          constantSwitch (param (ids::constantWidth), "Q", "Width"),
          rangeSwitch (param (ids::range96), "30 dB", "96 dB"),
          ultraQuietAttachment (param (ids::ultraQuiet), ultraQuietButton)
    {
        setLookAndFeel (&lnf);

        // Top bar
        presetBox.setTooltip ("Presets");
        presetBox.onChange = [this]
        {
            const int id = presetBox.getSelectedId();

            if (id <= 0)
                return;

            if (id >= userIdBase)
                proc.presets.loadUser (proc.presets.userPresetNames()[id - userIdBase]);
            else
                proc.presets.loadFactory (id - 1);

            refreshPresetText();
        };
        prevButton.onClick = [this] { stepPreset (-1); };
        nextButton.onClick = [this] { stepPreset (1); };
        saveButton.onClick = [this] { savePreset(); };
        deleteButton.onClick = [this] { deletePreset(); };
        analyserButton.setClickingTogglesState (true);
        analyserButton.setTooltip ("Show the output spectrum behind the curve");

        for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevButton, &nextButton, &saveButton, &deleteButton, &analyserButton })
            addAndMakeVisible (c);

        // Output gain
        masterSlider.setDoubleClickReturnValue (true, 0.0);
        masterSlider.setTooltip ("Output gain");
        masterBox.toText = [] (float v) { return formatGain (v); };
        masterBox.fromText = [] (const juce::String& s) -> std::optional<float> { if (auto v = parseNumber (s)) return (float) *v; return std::nullopt; };
        addAndMakeVisible (masterSlider);
        addAndMakeVisible (masterBox);

        addAndMakeVisible (graph);
        graph.onSelectionChanged = [this] (int) { repaint (gridArea); };

        // Band columns
        auto numberParser = [] (const juce::String& s) -> std::optional<float> { if (auto v = parseNumber (s)) return (float) *v; return std::nullopt; };

        for (int b = 0; b < numBands; ++b)
        {
            const auto type = bandSpecs[(size_t) b].type;
            auto& col = columns[(size_t) b];

            col.freq = std::make_unique<ValueBox> (param (ids::freq (b)));
            col.freq->toText = [] (float v) { return formatFreq (v); };
            col.freq->fromText = numberParser;
            addAndMakeVisible (*col.freq);

            if (hasGain (type))
            {
                col.gain = std::make_unique<ValueBox> (param (ids::gain (b)));
                col.gain->toText = [] (float v) { return formatGain (v); };
                col.gain->fromText = numberParser;
                addAndMakeVisible (*col.gain);
            }

            if (hasQ (type))
            {
                col.q = std::make_unique<ValueBox> (param (ids::q (b)));
                col.q->toText = [this] (float v) { return param (ids::constantWidth).getValue() > 0.5f ? formatWidth (v) : formatQ (v); };
                col.q->fromText = [this] (const juce::String& s) -> std::optional<float>
                {
                    const auto v = parseNumber (s);

                    if (! v.has_value() || *v <= 0.0)
                        return std::nullopt;

                    return (float) (param (ids::constantWidth).getValue() > 0.5f ? octavesToQ (*v) : *v);
                };
                addAndMakeVisible (*col.q);
            }

            if (hasSlope (type))
            {
                col.slope = std::make_unique<juce::ComboBox>();

                for (int i = 0; i < numSlopes; ++i)
                    col.slope->addItem (formatSlope (i), i + 1);

                col.slopeAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (param (ids::slope (b)), *col.slope);
                addAndMakeVisible (*col.slope);
            }

            col.toggle = std::make_unique<juce::TextButton> (bandSpecs[(size_t) b].label);
            col.toggle->setClickingTogglesState (true);
            col.toggle->setTooltip (juce::String ("Turn ") + bandSpecs[(size_t) b].name + " on or off");
            col.toggleAttachment = std::make_unique<juce::ButtonParameterAttachment> (param (ids::on (b)), *col.toggle);
            col.toggle->onClick = [this, b] { graph.setSelectedBand (b); repaint (gridArea); };
            addAndMakeVisible (*col.toggle);
        }

        // Bottom bar
        constantSwitch.setTooltip ("Show bell bandwidth as Q or as a width in octaves");
        rangeSwitch.setTooltip ("Vertical range of the graph");
        ultraQuietButton.setTooltip ("Runs the filters at twice the sample rate: cleaner highs, a few samples of latency, more CPU");
        addAndMakeVisible (constantSwitch);
        addAndMakeVisible (ultraQuietButton);
        addAndMakeVisible (rangeSwitch);

        refreshPresetList();

        setResizable (true, true);
        setResizeLimits (820, 520, 1800, 1100);
        setSize (940, 600);
        startTimerHz (10);
    }

    ApolloEditor::~ApolloEditor()
    {
        dialog.reset();
        setLookAndFeel (nullptr);
    }

    //==============================================================================
    void ApolloEditor::refreshPresetList()
    {
        presetBox.clear (juce::dontSendNotification);
        presetBox.addSectionHeading ("Factory");

        const auto& factory = factoryPresets();

        for (size_t i = 0; i < factory.size(); ++i)
            presetBox.addItem (factory[i].name, (int) i + 1);

        const auto user = proc.presets.userPresetNames();

        if (! user.isEmpty())
        {
            presetBox.addSeparator();
            presetBox.addSectionHeading ("User");

            for (int i = 0; i < user.size(); ++i)
                presetBox.addItem (user[i], userIdBase + i);
        }

        shownPreset = {};
        refreshPresetText();
    }

    void ApolloEditor::refreshPresetText()
    {
        const auto name = proc.presets.getCurrentName();
        const bool modified = proc.presets.isModified();
        const bool user = proc.presets.isCurrentUser();

        deleteButton.setEnabled (user && proc.presets.userPresetNames().contains (name));

        if (name == shownPreset && modified == shownModified)
            return;

        shownPreset = name;
        shownModified = modified;

        int id = 0;

        if (user)
        {
            if (const int i = proc.presets.userPresetNames().indexOf (name); i >= 0)
                id = userIdBase + i;
        }
        else
        {
            const auto& factory = factoryPresets();

            for (size_t i = 0; i < factory.size(); ++i)
                if (factory[i].name == name)
                    id = (int) i + 1;
        }

        if (modified || id == 0)
            presetBox.setText (name + (modified ? "  (edited)" : ""), juce::dontSendNotification);
        else
            presetBox.setSelectedId (id, juce::dontSendNotification);
    }

    void ApolloEditor::stepPreset (int delta)
    {
        const int numFactory = (int) factoryPresets().size();
        const auto user = proc.presets.userPresetNames();
        const int total = numFactory + user.size();

        int current = 0;
        const auto name = proc.presets.getCurrentName();

        if (proc.presets.isCurrentUser() && user.contains (name))
            current = numFactory + user.indexOf (name);
        else
            for (int i = 0; i < numFactory; ++i)
                if (factoryPresets()[(size_t) i].name == name)
                    current = i;

        const int next = (current + delta + total) % total;

        if (next < numFactory)
            proc.presets.loadFactory (next);
        else
            proc.presets.loadUser (user[next - numFactory]);

        shownPreset = {};
        refreshPresetText();
    }

    void ApolloEditor::savePreset()
    {
        dialog = std::make_unique<juce::AlertWindow> ("Save Preset", "Name this preset:", juce::MessageBoxIconType::NoIcon, this);
        dialog->setLookAndFeel (&lnf);
        dialog->addTextEditor ("name", proc.presets.isCurrentUser() ? proc.presets.getCurrentName() : juce::String());
        dialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        addAndMakeVisible (*dialog);
        dialog->setCentrePosition (getLocalBounds().getCentre());

        dialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<ApolloEditor> (this)] (int result)
        {
            if (safe == nullptr || safe->dialog == nullptr)
                return;

            const auto name = safe->dialog->getTextEditorContents ("name");
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->dialog.reset(); });

            if (result == 1 && name.trim().isNotEmpty())
            {
                safe->proc.presets.saveUser (name);
                safe->refreshPresetList();
            }
        }), false);
    }

    void ApolloEditor::deletePreset()
    {
        const auto name = proc.presets.getCurrentName();

        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::NoIcon)
                                          .withTitle ("Delete Preset")
                                          .withMessage ("Delete \"" + name + "\"? This can't be undone.")
                                          .withButton ("Delete")
                                          .withButton ("Cancel")
                                          .withAssociatedComponent (this),
                                      [safe = juce::Component::SafePointer<ApolloEditor> (this), name] (int result)
                                      {
                                          if (safe != nullptr && result == 1 && safe->proc.presets.deleteUser (name))
                                              safe->refreshPresetList();
                                      });
    }

    //==============================================================================
    void ApolloEditor::timerCallback()
    {
        refreshPresetText();

        for (int b = 0; b < numBands; ++b)
        {
            const float alpha = param (ids::on (b)).getValue() > 0.5f ? 1.0f : 0.4f;
            auto& col = columns[(size_t) b];

            for (auto* c : std::initializer_list<juce::Component*> { col.freq.get(), col.gain.get(), col.q.get(), col.slope.get() })
                if (c != nullptr && c->getAlpha() != alpha)
                    c->setAlpha (alpha);

            if (col.q != nullptr)
                col.q->repaint(); // Q / Width mode may have changed
        }
    }

    void ApolloEditor::paint (juce::Graphics& g)
    {
        using namespace colours;
        g.fillAll (panel);

        // Top bar with wordmark
        g.setColour (black);
        g.fillRect (topBar);
        g.setColour (edge);
        g.drawHorizontalLine (topBar.getBottom(), 0.0f, (float) getWidth());

        auto mark = topBar.withTrimmedLeft (16).withWidth (150);
        g.setFont (juce::Font (juce::FontOptions (19.0f, juce::Font::bold)).withExtraKerningFactor (0.18f));
        g.setColour (bright);
        g.drawText ("APOLLO", mark, juce::Justification::centredLeft);
        g.setColour (accent);
        g.drawText ("EQ", mark.withTrimmedLeft (100), juce::Justification::centredLeft);

        g.setColour (textDim);
        g.setFont (juce::FontOptions (12.5f));
        g.drawText ("Presets", presetBox.getBounds().translated (-62 - prevButton.getWidth(), 0).withWidth (56), juce::Justification::centredRight);

        // Output gain scale
        g.setFont (juce::FontOptions (12.5f));
        g.drawText ("Gain", masterSlider.getBounds().withHeight (16).translated (0, -22).withSizeKeepingCentre (60, 16), juce::Justification::centred);
        g.setFont (juce::FontOptions (10.5f));

        for (int v = 20; v >= -60; v -= 20)
        {
            const int y = (int) masterSlider.getPositionOfValue (v) + masterSlider.getY();
            g.setColour (v == 0 ? text : textDim);
            g.drawText (v > 0 ? "+" + juce::String (v) : juce::String (v), masterSlider.getX() - 34, y - 7, 26, 14, juce::Justification::centredRight);
            g.setColour (edge);
            g.drawHorizontalLine (y, (float) masterSlider.getX() - 6.0f, (float) masterSlider.getX() + 2.0f);
        }

        // Band grid
        if (const int sel = graph.getSelectedBand(); juce::isPositiveAndBelow (sel, numBands))
        {
            g.setColour (accent.withAlpha (0.07f));
            g.fillRoundedRectangle (columns[(size_t) sel].bounds.toFloat().expanded (0.0f, 4.0f), 6.0f);
        }

        g.setColour (textDim);
        g.setFont (juce::FontOptions (12.5f));

        for (int row = 0; row < 4; ++row)
            g.drawText (rowLabels[row], gridArea.getX(), gridArea.getY() + row * rowHeight, labelWidth - 12, rowHeight, juce::Justification::centredRight);

        for (int b = 1; b < numBands; ++b)
        {
            g.setColour (grid);
            g.drawVerticalLine (columns[(size_t) b].bounds.getX(), (float) gridArea.getY() + 4.0f, (float) gridArea.getBottom() - 4.0f);
        }

        // Bottom bar
        g.setColour (black);
        g.fillRect (bottomBar);
        g.setColour (edge);
        g.drawHorizontalLine (bottomBar.getY(), 0.0f, (float) getWidth());
        g.setColour (textDim);
        g.setFont (juce::FontOptions (12.5f));
        g.drawText ("Constant", constantSwitch.getBounds().translated (-78, 0).withWidth (70), juce::Justification::centredRight);
        g.drawText ("Range", rangeSwitch.getBounds().translated (-58, 0).withWidth (50), juce::Justification::centredRight);
    }

    void ApolloEditor::resized()
    {
        auto area = getLocalBounds();

        topBar = area.removeFromTop (48);
        {
            auto bar = topBar.reduced (12, 10);
            analyserButton.setBounds (bar.removeFromRight (92));
            bar.removeFromLeft (170 + 62);
            prevButton.setBounds (bar.removeFromLeft (28));
            presetBox.setBounds (bar.removeFromLeft (290).reduced (2, 0));
            nextButton.setBounds (bar.removeFromLeft (28));
            bar.removeFromLeft (8);
            saveButton.setBounds (bar.removeFromLeft (28));
            deleteButton.setBounds (bar.removeFromLeft (28));
        }

        bottomBar = area.removeFromBottom (46);
        {
            const int total = 78 + 120 + 40 + 120 + 40 + 58 + 140;
            auto bar = bottomBar.withSizeKeepingCentre (total, 26);
            bar.removeFromLeft (78);
            constantSwitch.setBounds (bar.removeFromLeft (120));
            bar.removeFromLeft (40);
            ultraQuietButton.setBounds (bar.removeFromLeft (120));
            bar.removeFromLeft (40 + 58);
            rangeSwitch.setBounds (bar.removeFromLeft (140));
        }

        auto content = area.reduced (12, 0);
        gridArea = content.removeFromBottom (4 * rowHeight + 14).withTrimmedBottom (8);
        content.removeFromTop (12);
        content.removeFromBottom (8);

        auto left = content.removeFromLeft (labelWidth);
        masterBox.setBounds (left.removeFromBottom (24).withSizeKeepingCentre (labelWidth - 8, 24).translated (-6, 0));
        left.removeFromTop (28);
        left.removeFromBottom (12);
        masterSlider.setBounds (left.withSizeKeepingCentre (22, left.getHeight()).translated (16, 0));

        graph.setBounds (content);

        // Columns line up with the plot (the graph keeps 30 px on the right for its dB scale).
        auto cols = gridArea.withTrimmedLeft (labelWidth).withTrimmedRight (30);
        const float colWidth = (float) cols.getWidth() / (float) numBands;

        for (int b = 0; b < numBands; ++b)
        {
            auto& col = columns[(size_t) b];
            const int x0 = cols.getX() + juce::roundToInt ((float) b * colWidth);
            const int x1 = cols.getX() + juce::roundToInt ((float) (b + 1) * colWidth);
            col.bounds = { x0, cols.getY(), x1 - x0, 4 * rowHeight };

            auto cell = [&] (int row) { return juce::Rectangle<int> (x0, cols.getY() + row * rowHeight, x1 - x0, rowHeight).reduced (4, 2); };

            col.freq->setBounds (cell (0));

            if (col.gain != nullptr)  col.gain->setBounds (cell (1));
            if (col.slope != nullptr) col.slope->setBounds (cell (1).reduced (0, 1));
            if (col.q != nullptr)     col.q->setBounds (cell (2));

            col.toggle->setBounds (cell (3).withSizeKeepingCentre (std::min (44, cell (3).getWidth()), 22));
        }

        if (dialog != nullptr)
            dialog->setCentrePosition (getLocalBounds().getCentre());
    }
}
