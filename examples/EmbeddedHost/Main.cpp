// Embeds Apollo EQ in a host's processing graph: audio input -> Apollo EQ -> audio output,
// with the EQ's own editor in the window. A DAW does the same per track or bus.

#include <apollo_eq/apollo_eq.h>
#include <juce_audio_utils/juce_audio_utils.h> // AudioProcessorPlayer, AudioDeviceManager

class HostWindow : public juce::DocumentWindow
{
public:
    HostWindow() : DocumentWindow ("Apollo Embedded Host", juce::Colours::black, allButtons)
    {
        using Graph = juce::AudioProcessorGraph;
        deviceManager.initialiseWithDefaultDevices (2, 2);

        // Build the graph: in -> EQ -> out.
        auto input  = graph.addNode (std::make_unique<Graph::AudioGraphIOProcessor> (Graph::AudioGraphIOProcessor::audioInputNode));
        auto output = graph.addNode (std::make_unique<Graph::AudioGraphIOProcessor> (Graph::AudioGraphIOProcessor::audioOutputNode));
        eqNode = graph.addNode (std::make_unique<apollo::ApolloProcessor>());

        for (int ch = 0; ch < 2; ++ch)
        {
            graph.addConnection ({ { input->nodeID, ch }, { eqNode->nodeID, ch } });
            graph.addConnection ({ { eqNode->nodeID, ch }, { output->nodeID, ch } });
        }

        auto& eq = *dynamic_cast<apollo::ApolloProcessor*> (eqNode->getProcessor());
        eq.presets.loadFactory (1); // "Vocal Enhancer"; see apollo::factoryPresets()

        player.setProcessor (&graph);
        deviceManager.addAudioCallback (&player);

        setContentOwned (eq.createEditor(), true);
        setResizable (true, false);
        setUsingNativeTitleBar (true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    ~HostWindow() override
    {
        clearContentComponent(); // the editor must go before its processor
        deviceManager.removeAudioCallback (&player);
        player.setProcessor (nullptr);
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

private:
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorGraph graph;
    juce::AudioProcessorPlayer player;
    juce::AudioProcessorGraph::Node::Ptr eqNode;
};

class HostApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Apollo Embedded Host"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    void initialise (const juce::String&) override      { window = std::make_unique<HostWindow>(); }
    void shutdown() override                            { window.reset(); }

private:
    std::unique_ptr<HostWindow> window;
};

START_JUCE_APPLICATION (HostApp)
