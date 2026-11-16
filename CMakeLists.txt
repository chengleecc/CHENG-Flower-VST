cmake_minimum_required(VERSION 3.22)
project(ChengFlower VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static MSVC runtime so the plugin needs no extra Visual C++ redistributable on Windows
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Use a local JUCE checkout if JUCE_PATH is given, otherwise download JUCE 8.0.4.
if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(FLOWER_SOURCES
    Source/FlowerDSP.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

set(FLOWER_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

juce_add_plugin(ChengFlower
    COMPANY_NAME "CHENG"
    PLUGIN_MANUFACTURER_CODE Chng
    PLUGIN_CODE ChFl
    FORMATS VST3
    PRODUCT_NAME "CHENG Flower"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Mastering Dynamics)

target_sources(ChengFlower PRIVATE ${FLOWER_SOURCES})
target_compile_definitions(ChengFlower PUBLIC ${FLOWER_DEFS})
target_link_libraries(ChengFlower
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Offline DSP tests + UI snapshot (not shipped)
option(FLOWER_BUILD_TESTS "Build DSP test runner" OFF)
if(FLOWER_BUILD_TESTS)
    juce_add_console_app(FlowerTests PRODUCT_NAME "FlowerTests")
    target_sources(FlowerTests PRIVATE tests/main.cpp ${FLOWER_SOURCES})
    target_include_directories(FlowerTests PRIVATE Source)
    target_compile_definitions(FlowerTests PRIVATE ${FLOWER_DEFS})
    target_link_libraries(FlowerTests PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()
