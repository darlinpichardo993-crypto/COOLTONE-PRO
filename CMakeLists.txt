cmake_minimum_required(VERSION 3.20)
project(COOLTONE VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# static MSVC runtime: the plugin does not need the Visual C++ redistributable on the user's PC
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(MSVC)
    add_compile_options(/utf-8)
endif()

option(COOLTONE_BUILD_TESTS  "Build the Core unit tests" ON)
option(COOLTONE_BUILD_PLUGIN "Build the JUCE VST3 + Standalone (downloads JUCE)" ON)

# ---------------------------------------------------------------------------------------
# Core: pure C++17, no JUCE, fully unit-tested (pitch detection, correction, vibrato, melisma, PSOLA, expression, export)
# ---------------------------------------------------------------------------------------
add_library(cooltone_core STATIC
    Source/Core/ScaleEngine.cpp
    Source/Core/PitchDetection.cpp
    Source/Core/NoteSegmenter.cpp
    Source/Core/WavIO.cpp
    Source/Core/Params.cpp
    Source/Core/PresetManager.cpp
    Source/Core/VibratoEngine.cpp
    Source/Core/NoteEditor.cpp
    Source/Core/MelismaEngine.cpp
    Source/Core/MagicVibrato.cpp
    Source/Core/PitchCorrection.cpp
    Source/Core/FormantProcessor.cpp
    Source/Core/BreathProcessor.cpp
    Source/Core/VoiceExpression.cpp
    Source/Core/AudioEngine.cpp
    Source/Core/MidiExport.cpp)
target_include_directories(cooltone_core PUBLIC Source/Core)
if(MSVC)
    target_compile_options(cooltone_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(cooltone_core PRIVATE -Wall -Wextra -Wpedantic)
    set_target_properties(cooltone_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(COOLTONE_BUILD_TESTS)
    enable_testing()
    add_executable(cooltone_tests Tests/test_main.cpp Tests/test_engine.cpp)
    target_link_libraries(cooltone_tests PRIVATE cooltone_core)
    add_test(NAME cooltone_core_tests COMMAND cooltone_tests)
endif()

# ---------------------------------------------------------------------------------------
# Plugin: JUCE 8, VST3 + Standalone
# ---------------------------------------------------------------------------------------
if(COOLTONE_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(COOLTONE
        COMPANY_NAME "YMY RECORDS"
        PRODUCT_NAME "COOLTONE"
        PLUGIN_MANUFACTURER_CODE Ymyr
        PLUGIN_CODE Clto
        FORMATS VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        VST3_CATEGORIES Fx
        COPY_PLUGIN_AFTER_BUILD FALSE
        BUNDLE_ID com.ymyrecords.cooltone)

    target_sources(COOLTONE PRIVATE
        Source/Plugin/PluginProcessor.cpp
        Source/UI/PianoRoll.cpp
        Source/UI/PluginEditor.cpp)
    target_include_directories(COOLTONE PRIVATE Source/Plugin Source/UI)
    target_compile_definitions(COOLTONE PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(COOLTONE
        PRIVATE
            cooltone_core
            juce::juce_audio_utils
            juce::juce_audio_formats
            juce::juce_audio_processors
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags)
endif()
