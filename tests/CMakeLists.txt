add_executable(StitcherTests
    FeatureExtractorTest.cpp
    CorpusStoreTest.cpp
    ConcatenativeMatcherTest.cpp
    EQProcessorTest.cpp
    PresetManagerTest.cpp
    MidiLearnTest.cpp
    MorphPadTest.cpp
    ${CMAKE_SOURCE_DIR}/Source/FeatureExtractor.cpp
    ${CMAKE_SOURCE_DIR}/Source/CorpusStore.cpp
    ${CMAKE_SOURCE_DIR}/Source/ConcatenativeMatcher.cpp
    ${CMAKE_SOURCE_DIR}/Source/EQProcessor.cpp
    ${CMAKE_SOURCE_DIR}/Source/PresetManager.cpp
    ${CMAKE_SOURCE_DIR}/Source/MidiLearn.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/MorphPad.cpp
    ${CMAKE_SOURCE_DIR}/Source/LookAndFeel/StitcherLookAndFeel.cpp
)

target_include_directories(StitcherTests PRIVATE
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_BINARY_DIR}/Stitcher_artefacts/JuceLibraryCode
    ${CMAKE_BINARY_DIR}/juce_binarydata_StitcherBinaryData/JuceLibraryCode)

target_compile_definitions(StitcherTests PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(StitcherTests PRIVATE
    Catch2::Catch2WithMain
    StitcherBinaryData
    juce::juce_dsp
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
