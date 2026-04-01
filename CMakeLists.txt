cmake_minimum_required(VERSION 3.27)

set(COMPANY_NAME Absinthismus)
set(PROJECT_NAME Stitcher)
set(PRODUCT_NAME Stitcher)

project(${PROJECT_NAME} VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_XCODE_GENERATE_SCHEME OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# CPM setup
set(CPM_DOWNLOAD_VERSION 0.34.0)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
if(NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION})
endif()
include(${CPM_DOWNLOAD_LOCATION})

# Dependencies
CPMAddPackage("gh:juce-framework/JUCE#master")
CPMAddPackage("gh:catchorg/Catch2@3.7.1")

set(SourceFiles
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/Parameters.h
    Source/ProtectYourEars.h
    Source/FeatureExtractor.h
    Source/FeatureExtractor.cpp
    Source/CorpusStore.h
    Source/CorpusStore.cpp
    Source/ConcatenativeMatcher.h
    Source/ConcatenativeMatcher.cpp
    Source/EQProcessor.h
    Source/EQProcessor.cpp
    Source/ReverbProcessor.h
    Source/ReverbProcessor.cpp
)

juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME                 ${COMPANY_NAME}
    IS_SYNTH                     FALSE
    NEEDS_MIDI_INPUT             FALSE
    NEEDS_MIDI_OUTPUT            FALSE
    IS_MIDI_EFFECT               FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS  FALSE
    JUCE_VST3_CAN_REPLACE_VST2   FALSE
    COPY_PLUGIN_AFTER_BUILD      TRUE
    PLUGIN_MANUFACTURER_CODE     Ab42
    PLUGIN_CODE                  CSS1
    FORMATS                      VST3 AU Standalone
    PRODUCT_NAME                 ${PRODUCT_NAME}
)

# pluginval must come after juce_add_plugin
CPMAddPackage("gh:Tracktion/pluginval#develop")

juce_generate_juce_header(${PROJECT_NAME})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SourceFiles})

target_sources(${PROJECT_NAME} PRIVATE ${SourceFiles})

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:${PROJECT_NAME},JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:${PROJECT_NAME},JUCE_VERSION>")

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        juce::juce_gui_extra
    PUBLIC
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Tests
add_subdirectory(tests)
