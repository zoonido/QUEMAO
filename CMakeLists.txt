cmake_minimum_required(VERSION 3.22)
project(quemao VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
if(DEFINED JUCE_LOCAL_PATH)
    add_subdirectory(${JUCE_LOCAL_PATH} juce)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.2
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(quemao
    COMPANY_NAME "ZOONIDO"
    PLUGIN_MANUFACTURER_CODE Zoon
    PLUGIN_CODE Quem
    FORMATS VST3
    PRODUCT_NAME "quemao"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum)

target_sources(quemao PRIVATE
    source/plugin_processor.cpp
    source/plugin_editor.cpp)

target_include_directories(quemao PRIVATE source)

target_compile_definitions(quemao PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(quemao
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Local-only window snapshot tool (never built on GitHub)
if(DEFINED JUCE_LOCAL_PATH)
    juce_add_console_app(quemao_snapshot PRODUCT_NAME "quemao_snapshot")
    target_sources(quemao_snapshot PRIVATE tools/snapshot.cpp source/plugin_processor.cpp source/plugin_editor.cpp)
    target_include_directories(quemao_snapshot PRIVATE source)
    target_compile_definitions(quemao_snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="quemao")
    target_link_libraries(quemao_snapshot PRIVATE juce::juce_audio_utils)
endif()
