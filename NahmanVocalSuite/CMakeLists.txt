cmake_minimum_required(VERSION 3.22)
project(NahmanVocalSuite VERSION 0.1.0)

# JUCE is fetched at configure time. Requires an internet connection on first build.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(NahmanVocalSuite
    COMPANY_NAME "Nahman Audio"
    BUNDLE_ID "com.nahmanaudio.vocal-suite"
    PLUGIN_MANUFACTURER_CODE Nahn
    PLUGIN_CODE Nv01
    FORMATS VST3 Standalone
    PRODUCT_NAME "NAHMAN VOCAL SUITE"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE)

juce_generate_juce_header(NahmanVocalSuite)
target_sources(NahmanVocalSuite PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)
target_compile_features(NahmanVocalSuite PRIVATE cxx_std_17)
if (MSVC)
    # Keep Hebrew literals encoded as UTF-8 on Windows builds.
    target_compile_options(NahmanVocalSuite PRIVATE /utf-8)
endif()
target_compile_definitions(NahmanVocalSuite PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(NahmanVocalSuite PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_processors
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)
