#pragma once

#include "Params.h"

namespace apollo
{
    struct BandPreset
    {
        bool on = true;
        float freq = 1000.0f;
        float gain = 0.0f;
        float q = defaultQ;
        int slope = defaultSlope;
    };

    struct FactoryPreset
    {
        juce::String name;
        std::array<BandPreset, numBands> bands;
        float master = 0.0f;
        bool ultraQuiet = false;
    };

    const std::vector<FactoryPreset>& factoryPresets();

    /** Factory presets plus user presets saved as XML in %APPDATA%\Apollo EQ\Presets. */
    class PresetManager : private juce::AudioProcessorValueTreeState::Listener
    {
    public:
        explicit PresetManager (juce::AudioProcessorValueTreeState&);
        ~PresetManager() override;

        static juce::File userDirectory();
        static bool isSoundParameter (const juce::String& id);

        juce::StringArray userPresetNames() const;

        void loadFactory (int index);
        bool loadUser (const juce::String& name);
        bool saveUser (const juce::String& name);
        bool deleteUser (const juce::String& name);

        juce::String getCurrentName() const;
        bool isCurrentUser() const;
        bool isModified() const { return modified.load(); }

        /** Call around a whole-state restore so it doesn't count as an edit. */
        void restoreState (const juce::ValueTree& state);

    private:
        void parameterChanged (const juce::String&, float) override;
        void setParam (const juce::String& id, float value);
        void setCurrent (const juce::String& name, bool user);

        juce::AudioProcessorValueTreeState& apvts;
        std::atomic<bool> applying { false }, modified { false };
    };
}
