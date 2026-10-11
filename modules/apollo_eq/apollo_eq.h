/*
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 apollo_eq
  vendor:             Apollo
  version:            1.0.0
  name:               Apollo EQ
  description:        Audition-style 9-band parametric equalizer: processor, editor, presets and DSP.
  minimumCppStandard: 20
  dependencies:       juce_audio_processors, juce_audio_utils, juce_dsp, juce_gui_basics

 END_JUCE_MODULE_DECLARATION
*/

#pragma once
#define APOLLO_EQ_H_INCLUDED

// Public API:
//   apollo::ApolloProcessor  - the complete EQ as a juce::AudioProcessor (state, presets, analyser feed)
//   apollo::ApolloEditor     - its UI (returned by ApolloProcessor::createEditor)
//   apollo::PresetManager    - factory + user presets (ApolloProcessor::presets)
//   apollo::dsp::BandFilter  - one band's filter, usable without the processor
//   apollo::dsp::magnitudeDb - exact response of a band, for drawing curves in your own UI
//   apollo::ids, bandSpecs   - parameter IDs and band layout

#include "Source/Params.h"
#include "Source/EqDsp.h"
#include "Source/Presets.h"
#include "Source/PluginProcessor.h"
#include "Source/PluginEditor.h"
