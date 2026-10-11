#pragma once

#include "PluginProcessor.h"
#include "EqGraph.h"
#include "Widgets.h"

namespace apollo
{
    class ApolloEditor : public juce::AudioProcessorEditor, private juce::Timer
    {
    public:
        explicit ApolloEditor (ApolloProcessor&);
        ~ApolloEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        struct Column
        {
            std::unique_ptr<ValueBox> freq, gain, q;
            std::unique_ptr<juce::ComboBox> slope;
            std::unique_ptr<juce::ComboBoxParameterAttachment> slopeAttachment;
            std::unique_ptr<juce::TextButton> toggle;
            std::unique_ptr<juce::ButtonParameterAttachment> toggleAttachment;
            juce::Rectangle<int> bounds;
        };

        void timerCallback() override;
        void refreshPresetList();
        void refreshPresetText();
        void stepPreset (int delta);
        void savePreset();
        void deletePreset();
        juce::RangedAudioParameter& param (const juce::String& id) { return *proc.apvts.getParameter (id); }

        ApolloProcessor& proc;
        LookAndFeel lnf;
        juce::TooltipWindow tooltips { this, 600 };

        juce::ComboBox presetBox;
        IconButton prevButton { "Previous Preset", icons::chevron (false) };
        IconButton nextButton { "Next Preset", icons::chevron (true) };
        IconButton saveButton { "Save Preset", icons::save() };
        IconButton deleteButton { "Delete Preset", icons::trash() };
        juce::TextButton analyserButton { "Analyzer" };
        juce::ButtonParameterAttachment analyserAttachment;

        juce::Slider masterSlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
        juce::SliderParameterAttachment masterAttachment;
        ValueBox masterBox;

        EqGraph graph;
        std::array<Column, numBands> columns;

        SegmentedSwitch constantSwitch, rangeSwitch;
        juce::ToggleButton ultraQuietButton { "Ultra-Quiet" };
        juce::ButtonParameterAttachment ultraQuietAttachment;

        juce::Rectangle<int> topBar, gridArea, bottomBar;
        int labelWidth = 88, rowHeight = 28;
        juce::String shownPreset;
        bool shownModified = false;
        std::unique_ptr<juce::AlertWindow> dialog;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ApolloEditor)
    };
}
