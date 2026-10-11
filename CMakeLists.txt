cmake_minimum_required(VERSION 3.22)

project(ApolloEQ VERSION 1.0.0 LANGUAGES C CXX)

# As the top-level project this builds the VST3 / Standalone plugin and the tests.
# Added to another project (add_subdirectory / FetchContent) it only provides the
# JUCE module target Apollo::apollo_eq.
option(APOLLO_EQ_BUILD_PLUGIN "Build the Apollo EQ VST3/Standalone plugin and its tests" ${PROJECT_IS_TOP_LEVEL})
option(APOLLO_EQ_PATCH_JUCE "Patch JUCE to render on the CPU (avoids a Direct3D crash inside Adobe Audition)" ${PROJECT_IS_TOP_LEVEL})

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    # Static MSVC runtime, so the installed plugin doesn't need the Visual C++ redistributable.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Use the parent project's JUCE if it already has one.
if(NOT COMMAND juce_add_module)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
    set(JUCE_SOURCE_DIR "${juce_SOURCE_DIR}")
endif()

if(APOLLO_EQ_PATCH_JUCE)
    if(NOT JUCE_SOURCE_DIR)
        message(FATAL_ERROR "APOLLO_EQ_PATCH_JUCE needs JUCE_SOURCE_DIR set to the JUCE checkout.")
    endif()
    include(cmake/PatchJuce.cmake)
endif()

juce_add_module("${CMAKE_CURRENT_SOURCE_DIR}/modules/apollo_eq")
add_library(Apollo::apollo_eq ALIAS apollo_eq)

if(NOT APOLLO_EQ_BUILD_PLUGIN)
    return()
endif()

set(APOLLO_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_GHOST_NO_GPU=1) # see cmake/PatchJuce.cmake

juce_add_plugin(ApolloEQ
    COMPANY_NAME "Apollo"
    PRODUCT_NAME "Apollo EQ"
    PLUGIN_MANUFACTURER_CODE Aplo
    PLUGIN_CODE ApEq
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx EQ)

target_sources(ApolloEQ PRIVATE Plugin/PluginEntry.cpp)
target_compile_definitions(ApolloEQ PUBLIC ${APOLLO_DEFINITIONS})
target_link_libraries(ApolloEQ
    PRIVATE
        Apollo::apollo_eq
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Drives the real processor with Audition's block pattern and checks the response against the model.
# Also renders the editor: ApolloTests --snapshot out.png <preset index>
juce_add_console_app(ApolloTests PRODUCT_NAME "ApolloTests")
target_sources(ApolloTests PRIVATE Tests/EqTests.cpp)
target_compile_definitions(ApolloTests PRIVATE ${APOLLO_DEFINITIONS} JucePlugin_Name="Apollo EQ" JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(ApolloTests
    PRIVATE Apollo::apollo_eq
    PUBLIC juce::juce_recommended_config_flags)

enable_testing()
add_test(NAME ApolloTests COMMAND ApolloTests)
