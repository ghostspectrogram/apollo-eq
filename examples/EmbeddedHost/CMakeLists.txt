# A minimal host that embeds Apollo EQ the way a DAW would: no plugin loading, just the
# module linked in and the processor run inside an AudioProcessorGraph.
#
#   cmake -B build -DFETCHCONTENT_SOURCE_DIR_JUCE=<path to JUCE 8>   (optional: reuse a local JUCE)
#   cmake --build build --config Release

cmake_minimum_required(VERSION 3.22)
project(ApolloEmbeddedHost VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# 1. Your project brings JUCE (Apollo EQ reuses it).
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

# 2. Add Apollo EQ. In your own project use the git repository instead:
#      FetchContent_Declare(ApolloEQ GIT_REPOSITORY https://github.com/ghostspectrogram/apollo-eq.git GIT_TAG v1.0.0)
#      FetchContent_MakeAvailable(ApolloEQ)
add_subdirectory(../.. apollo_eq)

juce_add_gui_app(ApolloEmbeddedHost PRODUCT_NAME "Apollo Embedded Host")
target_sources(ApolloEmbeddedHost PRIVATE Main.cpp)
target_compile_definitions(ApolloEmbeddedHost PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

# 3. Link the module.
target_link_libraries(ApolloEmbeddedHost
    PRIVATE Apollo::apollo_eq
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
