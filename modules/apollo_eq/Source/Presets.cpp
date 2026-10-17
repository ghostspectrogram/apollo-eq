#include "Presets.h"

namespace apollo
{
    namespace
    {
        constexpr int slope12 = 1, slope18 = 2, slope24 = 3, slope36 = 5, slope48 = 7;

        FactoryPreset flat (const juce::String& name)
        {
            FactoryPreset p { name, {} };

            for (int b = 0; b < numBands; ++b)
                p.bands[(size_t) b] = { bandSpecs[(size_t) b].defaultOn, bandSpecs[(size_t) b].defaultFreq };

            return p;
        }

        std::vector<FactoryPreset> makeFactoryPresets()
        {
            std::vector<FactoryPreset> list;

            list.push_back (flat ("(Default)"));

            { // Audition's own Vocal Enhancer
                auto p = flat ("Vocal Enhancer");
                auto& b = p.bands;
                b[band::hp] = { true, 80.0f, 0.0f, defaultQ, slope24 };
                b[band::ls] = { true, 110.0f, 6.0f };
                b[band::b1].on = false;
                b[band::b2].on = false;
                b[band::b3] = { true, 291.0f, -3.0f, 1.5f };
                b[band::hs] = { true, 17458.0f, 13.9f };
                p.ultraQuiet = true;
                list.push_back (p);
            }

            { // Narrow, honky radio band with steep walls
                auto p = flat ("Walkie Talkie");
                auto& b = p.bands;
                b[band::hp] = { true, 450.0f, 0.0f, defaultQ, slope48 };
                b[band::lp] = { true, 3000.0f, 0.0f, defaultQ, slope48 };
                b[band::ls].on = false;
                b[band::hs].on = false;
                b[band::b2] = { true, 900.0f, 3.0f, 1.2f };
                b[band::b3] = { true, 1700.0f, 7.0f, 1.0f };
                b[band::b4] = { true, 2600.0f, 4.0f, 2.5f };
                p.master = -6.0f;
                list.push_back (p);
            }

            { // Someone talking through a wall or from under a blanket
                auto p = flat ("Muffled (Through a Wall)");
                auto& b = p.bands;
                b[band::hp] = { true, 45.0f, 0.0f, defaultQ, slope12 };
                b[band::ls] = { true, 150.0f, 4.0f };
                b[band::b2] = { true, 300.0f, 3.0f, 0.9f };
                b[band::b3] = { true, 1200.0f, -6.0f, 0.7f };
                b[band::hs] = { true, 2500.0f, -14.0f };
                b[band::lp] = { true, 750.0f, 0.0f, defaultQ, slope24 };
                p.master = -2.0f;
                list.push_back (p);
            }

            { // The reverse: brings back a muffled recording
                auto p = flat ("Clarity (Fix Muffled Voice)");
                auto& b = p.bands;
                b[band::hp] = { true, 80.0f, 0.0f, defaultQ, slope18 };
                b[band::ls].on = false;
                b[band::b2] = { true, 220.0f, -4.0f, 1.1f };
                b[band::b3] = { true, 450.0f, -2.5f, 1.4f };
                b[band::b4] = { true, 2800.0f, 4.5f, 0.9f };
                b[band::b5] = { true, 5500.0f, 2.5f, 1.2f };
                b[band::hs] = { true, 10000.0f, 4.0f };
                p.ultraQuiet = true;
                list.push_back (p);
            }

            {
                auto p = flat ("Telephone");
                auto& b = p.bands;
                b[band::hp] = { true, 300.0f, 0.0f, defaultQ, slope24 };
                b[band::lp] = { true, 3400.0f, 0.0f, defaultQ, slope24 };
                b[band::b3] = { true, 1000.0f, 4.0f, 0.8f };
                p.master = -2.0f;
                list.push_back (p);
            }

            {
                auto p = flat ("Podcast Voice");
                auto& b = p.bands;
                b[band::hp] = { true, 75.0f, 0.0f, defaultQ, slope18 };
                b[band::ls] = { true, 120.0f, 2.0f };
                b[band::b2] = { true, 320.0f, -3.0f, 1.2f };
                b[band::b4] = { true, 3500.0f, 2.5f, 1.0f };
                b[band::hs] = { true, 11000.0f, 3.0f };
                p.ultraQuiet = true;
                list.push_back (p);
            }

            {
                auto p = flat ("De-Mud");
                auto& b = p.bands;
                b[band::hp] = { true, 60.0f, 0.0f, defaultQ, slope12 };
                b[band::b2] = { true, 250.0f, -5.0f, 1.0f };
                b[band::b3] = { true, 500.0f, -2.0f, 1.5f };
                list.push_back (p);
            }

            {
                auto p = flat ("Bass Boost");
                auto& b = p.bands;
                b[band::ls] = { true, 100.0f, 6.0f };
                b[band::b1] = { true, 55.0f, 2.0f, 1.0f };
                b[band::b2] = { true, 250.0f, -2.0f, 1.2f };
                p.master = -3.0f;
                list.push_back (p);
            }

            {
                auto p = flat ("Air & Presence");
                auto& b = p.bands;
                b[band::b4] = { true, 4000.0f, 2.0f, 0.8f };
                b[band::hs] = { true, 12000.0f, 5.0f };
                p.ultraQuiet = true;
                list.push_back (p);
            }

            {
                auto p = flat ("Remove Rumble");
                p.bands[band::hp] = { true, 100.0f, 0.0f, defaultQ, slope36 };
                list.push_back (p);
            }

            return list;
        }

        constexpr const char* presetProperty = "preset";
        constexpr const char* presetUserProperty = "presetUser";
        constexpr const char* fileExtension = ".apollopreset";
    }

    const std::vector<FactoryPreset>& factoryPresets()
    {
        static const auto list = makeFactoryPresets();
        return list;
    }

    //==============================================================================
    PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state)
    {
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p); rp != nullptr && isSoundParameter (rp->getParameterID()))
                apvts.addParameterListener (rp->getParameterID(), this);

        if (! apvts.state.hasProperty (presetProperty))
            setCurrent (factoryPresets().front().name, false);
    }

    PresetManager::~PresetManager()
    {
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                apvts.removeParameterListener (rp->getParameterID(), this);
    }

    juce::File PresetManager::userDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Apollo EQ").getChildFile ("Presets");
    }

    bool PresetManager::isSoundParameter (const juce::String& id)
    {
        return id != ids::constantWidth && id != ids::range96 && id != ids::analyser;
    }

    juce::StringArray PresetManager::userPresetNames() const
    {
        juce::StringArray names;

        for (const auto& f : userDirectory().findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension))
            names.add (f.getFileNameWithoutExtension());

        names.sortNatural();
        return names;
    }

    void PresetManager::setParam (const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    }

    void PresetManager::setCurrent (const juce::String& name, bool user)
    {
        apvts.state.setProperty (presetProperty, name, nullptr);
        apvts.state.setProperty (presetUserProperty, user, nullptr);
        modified = false;
    }

    void PresetManager::loadFactory (int index)
    {
        const auto& list = factoryPresets();

        if (! juce::isPositiveAndBelow (index, (int) list.size()))
            return;

        const auto& preset = list[(size_t) index];
        applying = true;

        for (int b = 0; b < numBands; ++b)
        {
            const auto& bp = preset.bands[(size_t) b];
            const auto type = bandSpecs[(size_t) b].type;

            setParam (ids::on (b), bp.on ? 1.0f : 0.0f);
            setParam (ids::freq (b), bp.freq);

            if (hasGain (type))  setParam (ids::gain (b), bp.gain);
            if (hasQ (type))     setParam (ids::q (b), bp.q);
            if (hasSlope (type)) setParam (ids::slope (b), (float) bp.slope);

            setParam (ids::stereo (b), 0.0f); // factory presets are all Stereo
        }

        setParam (ids::master, preset.master);
        setParam (ids::ultraQuiet, preset.ultraQuiet ? 1.0f : 0.0f);
        applying = false;
        setCurrent (preset.name, false);
    }

    bool PresetManager::loadUser (const juce::String& name)
    {
        const auto xml = juce::XmlDocument::parse (userDirectory().getChildFile (name + fileExtension));

        if (xml == nullptr || ! xml->hasTagName ("ApolloPreset"))
            return false;

        applying = true;

        for (int b = 0; b < numBands; ++b)
            setParam (ids::stereo (b), 0.0f); // presets saved before 1.1 have no Channels: Stereo

        for (auto* param : xml->getChildWithTagNameIterator ("PARAM"))
        {
            const auto id = param->getStringAttribute ("id");

            if (isSoundParameter (id))
                setParam (id, (float) param->getDoubleAttribute ("value"));
        }

        applying = false;
        setCurrent (name, true);
        return true;
    }

    bool PresetManager::saveUser (const juce::String& rawName)
    {
        const auto name = juce::File::createLegalFileName (rawName.trim());

        if (name.isEmpty())
            return false;

        juce::XmlElement xml ("ApolloPreset");
        xml.setAttribute ("name", name);

        for (auto* p : apvts.processor.getParameters())
        {
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p); rp != nullptr && isSoundParameter (rp->getParameterID()))
            {
                auto* e = xml.createNewChildElement ("PARAM");
                e->setAttribute ("id", rp->getParameterID());
                e->setAttribute ("value", rp->convertFrom0to1 (rp->getValue()));
            }
        }

        const auto dir = userDirectory();

        if (! dir.createDirectory() || ! xml.writeTo (dir.getChildFile (name + fileExtension)))
            return false;

        setCurrent (name, true);
        return true;
    }

    bool PresetManager::deleteUser (const juce::String& name)
    {
        if (! userDirectory().getChildFile (name + fileExtension).deleteFile())
            return false;

        if (isCurrentUser() && getCurrentName() == name)
            modified = true; // keeps the sound, but it no longer matches a preset

        return true;
    }

    juce::String PresetManager::getCurrentName() const { return apvts.state.getProperty (presetProperty).toString(); }
    bool PresetManager::isCurrentUser() const          { return (bool) apvts.state.getProperty (presetUserProperty); }

    void PresetManager::restoreState (const juce::ValueTree& state)
    {
        applying = true;
        apvts.replaceState (state);
        applying = false;
        modified = (bool) state.getProperty ("presetModified", false);
    }

    void PresetManager::parameterChanged (const juce::String&, float)
    {
        if (! applying)
            modified = true;
    }
}
