add_executable(StitcherTests
    FeatureExtractorTest.cpp
    CorpusStoreTest.cpp
    ConcatenativeMatcherTest.cpp
    EQProcessorTest.cpp
    PresetManagerTest.cpp
    MidiLearnTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/FeatureExtractor.cpp
    ${CMAKE_SOURCE_DIR}/Source/CorpusStore.cpp
    ${CMAKE_SOURCE_DIR}/Source/ConcatenativeMatcher.cpp
    ${CMAKE_SOURCE_DIR}/Source/EQProcessor.cpp
    ${CMAKE_SOURCE_DIR}/Source/PresetManager.cpp
    ${CMAKE_SOURCE_DIR}/Source/MidiLearn.cpp
)

target_include_directories(StitcherTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_BINARY_DIR}/Stitcher_artefacts/JuceLibraryCode)

target_compile_definitions(StitcherTests PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(StitcherTests PRIVATE
    Catch2::Catch2WithMain
    juce::juce_dsp
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
