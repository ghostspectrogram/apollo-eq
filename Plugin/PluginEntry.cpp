// Entry point for the VST3 / Standalone builds. Hosts embedding the module directly
// create apollo::ApolloProcessor themselves and don't need this file.

#include <apollo_eq/apollo_eq.h>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new apollo::ApolloProcessor(); }
