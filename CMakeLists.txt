cmake_minimum_required(VERSION 3.22)
project(Replicant303 VERSION 1.2.0 LANGUAGES C CXX)

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
# Source files shared by the plugin, the docs renderer and the test suite.
# Declared once so the three targets can never drift apart.
# ------------------------------------------------------------------------------
set(R303_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h

    # DSP engine
    Source/DSP/Common303.h
    Source/DSP/Oscillator303.h
    Source/DSP/Oscillator303.cpp
    Source/DSP/ScopeTap.h
    Source/DSP/DiodeLadderFilter.h
    Source/DSP/DiodeLadderFilter.cpp
    Source/DSP/Envelope303.h
    Source/DSP/DistortionUnit.h
    Source/DSP/DistortionUnit.cpp
    Source/DSP/SynthEngine.h
    Source/DSP/SynthEngine.cpp
    Source/DSP/Pattern.h
    Source/DSP/PatternSequencer.h
    Source/DSP/PatternSequencer.cpp

    # Preset vault + .midi preset I/O
    Source/Presets/PresetBank.h
    Source/Presets/PresetBank.cpp
    Source/Presets/PresetIO.h
    Source/Presets/PresetIO.cpp

    # GUI
    Source/GUI/Replicant303LookAndFeel.h
    Source/GUI/Replicant303LookAndFeel.cpp
    Source/GUI/Replicant303Theme.h
    Source/GUI/Replicant303Theme.cpp
    Source/GUI/PanelLayout.h
    Source/GUI/PatternDrawer.h
    Source/GUI/PatternDrawer.cpp
    Source/GUI/CyberWavetable3D.h
    Source/GUI/CyberWavetable3D.cpp
)

set(R303_INCLUDE_DIRS Source Source/DSP Source/GUI)
set(R303_JUCE_LIBS
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_opengl
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

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
    DESCRIPTION "Virtual Analog TB-303 Synthesizer with Pattern Sequencer and Multi-Distortion Engine"
    BUNDLE_ID "com.danimanx.replicant303"
)

target_sources(Replicant303 PRIVATE ${R303_SOURCES})

target_compile_definitions(Replicant303 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_include_directories(Replicant303 PRIVATE ${R303_INCLUDE_DIRS})
target_link_libraries(Replicant303 PRIVATE ${R303_JUCE_LIBS})

# ------------------------------------------------------------------------------
# Internal tools (excluded from the default build)
# ------------------------------------------------------------------------------
function(r303_add_tool target source)
    add_executable(${target} EXCLUDE_FROM_ALL ${source} ${R303_SOURCES})
    target_include_directories(${target} PRIVATE ${R303_INCLUDE_DIRS})
    target_compile_definitions(${target} PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
    )
    target_link_libraries(${target} PRIVATE ${R303_JUCE_LIBS})
endfunction()

# Renders the editor off-screen to a PNG for the README previews.
r303_add_tool(generate_snapshot tools/generate_snapshot.cpp)

# Headless regression suite (patterns, clock, MIDI export, DSP, state).
r303_add_tool(run_tests tools/run_tests.cpp)

# Renders the acid demo used by the promo video through the real plugin DSP.
r303_add_tool(render_audio_demo tools/render_audio_demo.cpp)

# Writes the factory preset examples (and optionally the full 8 x 64 bank) as
# .midi files. It is the checked-in source of truth for presets/.
r303_add_tool(generate_presets tools/generate_presets.cpp)
