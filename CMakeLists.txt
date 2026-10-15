cmake_minimum_required(VERSION 3.22)
project(Replicant303 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------------------
# JUCE Dependency via FetchContent (JUCE 8)
# ------------------------------------------------------------------------------
include(FetchContent)

set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(JUCE)

# ------------------------------------------------------------------------------
# Formats: VST3, AU (macOS only), and Standalone
# ------------------------------------------------------------------------------
set(PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

# ------------------------------------------------------------------------------
# Plugin Target Definition
# ------------------------------------------------------------------------------
juce_add_plugin(Replicant303
    COMPANY_NAME "DanimanX"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE DnmX
    PLUGIN_CODE R303
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Replicant303"
    DESCRIPTION "Virtual Analog TB-303 Synthesizer with Multi-Distortion Engine"
    BUNDLE_ID "com.danimanx.replicant303"
)

# ------------------------------------------------------------------------------
# Source Files
# ------------------------------------------------------------------------------
target_sources(Replicant303 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    
    # DSP Engine
    Source/DSP/Common303.h
    Source/DSP/Oscillator303.h
    Source/DSP/Oscillator303.cpp
    Source/DSP/DiodeLadderFilter.h
    Source/DSP/DiodeLadderFilter.cpp
    Source/DSP/Envelope303.h
    Source/DSP/DistortionUnit.h
    Source/DSP/DistortionUnit.cpp
    Source/DSP/SynthEngine.h
    Source/DSP/SynthEngine.cpp

    # GUI Look & Feel
    Source/GUI/TranslucentGreenLookAndFeel.h
    Source/GUI/TranslucentGreenLookAndFeel.cpp
)

# ------------------------------------------------------------------------------
# Compilation & Link Settings
# ------------------------------------------------------------------------------
target_compile_definitions(Replicant303 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_include_directories(Replicant303 PRIVATE
    Source
    Source/DSP
    Source/GUI
)

target_link_libraries(Replicant303 PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
